#include "sage/audio/audio_engine.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#include <miniaudio.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace sage {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
} // namespace

struct AudioEngine::Impl {
    ma_device device{};
    bool deviceStarted = false;

    // Lock-free parameters written by game thread, read by audio thread
    std::atomic<float> rpm{0.08f};
    std::atomic<float> throttle{0.0f};
    std::atomic<float> drift{0.0f};
    std::atomic<float> boost{0.0f};
    std::atomic<bool> airborne{false};

    std::atomic<float> masterVolume{0.42f};
    std::atomic<bool> muted{false};

    // One-shot trigger mailboxes (game thread sets > 0, audio thread consumes)
    std::atomic<float> triggerImpact{0.0f};
    std::atomic<bool> triggerChime{false};
    std::atomic<bool> triggerExplosion{false};

    // Audio-thread synth state
    float sampleRate = 48000.0f;
    float smoothRpm = 0.08f;
    float smoothThrottle = 0.0f;
    float smoothDrift = 0.0f;
    float smoothBoost = 0.0f;

    float enginePhase = 0.0f;
    float screechPhase = 0.0f;
    float boostPhase = 0.0f;
    float lpNoise = 0.0f;
    std::uint32_t rngState = 0x12345678u;

    // Active one-shot envelopes
    float impactEnv = 0.0f;
    float impactFreq = 140.0f;
    float impactPhase = 0.0f;

    float chimeTime = -1.0f;
    float chimePhase = 0.0f;

    float explosionEnv = 0.0f;
    float explosionPhase = 0.0f;

    float nextNoise() {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return (static_cast<float>(rngState & 0xFFFFu) / 32767.5f) - 1.0f;
    }

    static void dataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount) {
        auto* self = static_cast<Impl*>(pDevice->pUserData);
        auto* out = static_cast<float*>(pOutput);
        if (!self || !out) {
            return;
        }

        const float targetRpm = self->rpm.load(std::memory_order_relaxed);
        const float targetThrottle = self->throttle.load(std::memory_order_relaxed);
        const float targetDrift = self->drift.load(std::memory_order_relaxed);
        const float targetBoost = self->boost.load(std::memory_order_relaxed);
        const float gain = self->muted.load(std::memory_order_relaxed)
            ? 0.0f
            : self->masterVolume.load(std::memory_order_relaxed);

        if (const float imp = self->triggerImpact.exchange(0.0f, std::memory_order_relaxed); imp > 0.01f) {
            self->impactEnv = std::max(self->impactEnv, std::clamp(imp, 0.2f, 1.0f));
            self->impactFreq = 120.0f + imp * 90.0f;
        }
        if (self->triggerChime.exchange(false, std::memory_order_relaxed)) {
            self->chimeTime = 0.0f;
        }
        if (self->triggerExplosion.exchange(false, std::memory_order_relaxed)) {
            self->explosionEnv = 1.0f;
        }

        const float dt = 1.0f / self->sampleRate;
        const float smoothK = 1.0f - std::exp(-18.0f * dt);

        for (ma_uint32 i = 0; i < frameCount; ++i) {
            self->smoothRpm += (targetRpm - self->smoothRpm) * smoothK;
            self->smoothThrottle += (targetThrottle - self->smoothThrottle) * smoothK;
            self->smoothDrift += (targetDrift - self->smoothDrift) * smoothK;
            self->smoothBoost += (targetBoost - self->smoothBoost) * smoothK;

            // 1. Lamborghini V12 firing-order harmonic synth (55 Hz idle -> 280 Hz redline)
            const float baseFreq = 52.0f + self->smoothRpm * 235.0f;
            self->enginePhase += baseFreq * dt;
            if (self->enginePhase >= 1.0f) self->enginePhase -= 1.0f;

            const float p = self->enginePhase * kTwoPi;
            float v12 = 0.50f * std::sin(p)
                      + 0.34f * std::sin(p * 2.0f)
                      + 0.24f * std::sin(p * 3.0f)
                      + 0.14f * std::sin(p * 6.0f);
            // Soft saturation growl under throttle
            v12 = std::tanh(v12 * (1.15f + self->smoothThrottle * 1.35f));
            const float engineVol = 0.14f + self->smoothThrottle * 0.14f + self->smoothRpm * 0.06f;

            // 2. Tire drift screech (high-frequency modulated squeal + grit)
            const float noise = self->nextNoise();
            self->lpNoise += (noise - self->lpNoise) * 0.12f;
            self->screechPhase += (820.0f + self->smoothRpm * 320.0f + self->lpNoise * 95.0f) * dt;
            if (self->screechPhase >= 1.0f) self->screechPhase -= 1.0f;
            const float driftSig = (std::sin(self->screechPhase * kTwoPi) * 0.55f + (noise - self->lpNoise) * 0.35f)
                                 * self->smoothDrift * 0.18f;

            // 3. Nitro afterburner jet roar + rising whine
            self->boostPhase += (310.0f + self->smoothRpm * 420.0f) * dt;
            if (self->boostPhase >= 1.0f) self->boostPhase -= 1.0f;
            const float boostSig = (self->lpNoise * 0.85f + std::sin(self->boostPhase * kTwoPi) * 0.22f)
                                 * self->smoothBoost * 0.24f;

            // 4. One-shot crate/wall impact thud
            float impactSig = 0.0f;
            if (self->impactEnv > 0.001f) {
                self->impactPhase += self->impactFreq * dt;
                if (self->impactPhase >= 1.0f) self->impactPhase -= 1.0f;
                impactSig = (std::sin(self->impactPhase * kTwoPi) * 0.72f + self->lpNoise * 0.45f) * self->impactEnv * 0.48f;
                self->impactFreq = std::max(42.0f, self->impactFreq - 280.0f * dt);
                self->impactEnv *= std::exp(-11.0f * dt);
            }

            // 5. One-shot delivery / stunt jump chime (C5 -> E5 -> G5 -> C6)
            float chimeSig = 0.0f;
            if (self->chimeTime >= 0.0f) {
                constexpr float kNotes[4] = {523.25f, 659.25f, 783.99f, 1046.50f};
                const int noteIdx = std::min(3, static_cast<int>(self->chimeTime / 0.085f));
                self->chimePhase += kNotes[noteIdx] * dt;
                if (self->chimePhase >= 1.0f) self->chimePhase -= 1.0f;
                const float env = std::exp(-5.2f * self->chimeTime);
                chimeSig = (std::sin(self->chimePhase * kTwoPi) + 0.3f * std::sin(self->chimePhase * 2.0f * kTwoPi)) * env * 0.30f;
                self->chimeTime += dt;
                if (self->chimeTime > 0.55f) {
                    self->chimeTime = -1.0f;
                }
            }

            // 6. One-shot shockwave explosion boom
            float expSig = 0.0f;
            if (self->explosionEnv > 0.001f) {
                const float expFreq = 38.0f + self->explosionEnv * 95.0f;
                self->explosionPhase += expFreq * dt;
                if (self->explosionPhase >= 1.0f) self->explosionPhase -= 1.0f;
                expSig = (std::sin(self->explosionPhase * kTwoPi) * 0.70f + self->lpNoise * 0.65f) * self->explosionEnv * 0.55f;
                self->explosionEnv *= std::exp(-3.4f * dt);
            }

            const float mixed = std::tanh((v12 * engineVol + driftSig + boostSig + impactSig + chimeSig + expSig) * gain);
            out[i * 2 + 0] = mixed;
            out[i * 2 + 1] = mixed;
        }
    }
};

AudioEngine::AudioEngine()
    : m_impl(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(bool silentMode) {
    if (silentMode) {
        std::cout << "[SAGE][Audio] miniaudio initialized (silent smoke-test mode)\n";
        return true;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = &Impl::dataCallback;
    config.pUserData = m_impl.get();

    if (ma_device_init(nullptr, &config, &m_impl->device) != MA_SUCCESS) {
        std::cerr << "[SAGE][Audio] Warning: could not open audio playback device (continuing muted).\n";
        return false;
    }

    m_impl->sampleRate = static_cast<float>(m_impl->device.sampleRate);
    if (ma_device_start(&m_impl->device) != MA_SUCCESS) {
        std::cerr << "[SAGE][Audio] Warning: failed to start audio playback device.\n";
        ma_device_uninit(&m_impl->device);
        return false;
    }

    m_impl->deviceStarted = true;
    std::cout << "[SAGE][Audio] miniaudio 0.11 active (" << m_impl->device.sampleRate
              << " Hz stereo, V12 & SFX synth ready)\n";
    return true;
}

void AudioEngine::shutdown() {
    if (m_impl && m_impl->deviceStarted) {
        ma_device_uninit(&m_impl->device);
        m_impl->deviceStarted = false;
    }
}

void AudioEngine::setVehicleState(float rpm01, float throttle01, float drift01, float boost01, bool airborne) {
    m_impl->rpm.store(std::clamp(rpm01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->throttle.store(std::clamp(throttle01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->drift.store(airborne ? 0.0f : std::clamp(drift01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->boost.store(std::clamp(boost01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->airborne.store(airborne, std::memory_order_relaxed);
}

void AudioEngine::playImpact(float intensity01) {
    m_impl->triggerImpact.store(std::clamp(intensity01, 0.1f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::playChime() {
    m_impl->triggerChime.store(true, std::memory_order_relaxed);
}

void AudioEngine::playExplosion() {
    m_impl->triggerExplosion.store(true, std::memory_order_relaxed);
}

void AudioEngine::setMasterVolume(float volume01) {
    m_impl->masterVolume.store(std::clamp(volume01, 0.0f, 1.0f), std::memory_order_relaxed);
}

float AudioEngine::masterVolume() const {
    return m_impl->masterVolume.load(std::memory_order_relaxed);
}

void AudioEngine::setMuted(bool muted) {
    m_impl->muted.store(muted, std::memory_order_relaxed);
}

bool AudioEngine::isMuted() const {
    return m_impl->muted.load(std::memory_order_relaxed);
}

bool AudioEngine::isActive() const {
    return m_impl->deviceStarted;
}

} // namespace sage
