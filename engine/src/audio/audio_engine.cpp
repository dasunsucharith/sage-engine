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
constexpr float kPi = 3.14159265358979323846f;

// Simple 2-pole State-Variable Filter (gives realistic exhaust pipe & turbo formant resonance!)
struct SvfFilter {
    float low = 0.0f;
    float band = 0.0f;

    float processBandpass(float input, float cutoffHz, float q, float sampleRate) {
        const float f = std::clamp(2.0f * std::sin(kPi * std::min(cutoffHz, sampleRate * 0.42f) / sampleRate), 0.001f, 0.95f);
        const float damp = 1.0f / std::max(q, 0.5f);
        low += f * band;
        const float high = input - low - damp * band;
        band += f * high;
        return band;
    }

    float processLowpass(float input, float cutoffHz, float q, float sampleRate) {
        processBandpass(input, cutoffHz, q, sampleRate);
        return low;
    }
};

// Asymmetric combustion pulse shaper (sharp ignition transient + rich harmonic exhaust tail!)
float combustionWave(float phase01, float harmonicMix) {
    // Saw-like cylinder pressure pulse with smooth rounded combustion peak
    const float p = phase01 * kTwoPi;
    const float pulse = std::exp(-4.2f * phase01) * std::sin(p * 1.5f);
    const float fund  = std::sin(p);
    const float h2    = std::sin(p * 2.0f + 0.4f);
    const float h3    = std::sin(p * 3.0f + 1.1f);
    const float h4    = std::sin(p * 4.0f + 0.7f);
    const float h6    = std::sin(p * 6.0f + 1.6f);
    return pulse * 0.55f + fund * 0.48f + h2 * 0.34f + h3 * (0.28f * harmonicMix) + h4 * 0.16f + h6 * (0.14f * harmonicMix);
}

} // namespace

struct AudioEngine::Impl {
    ma_device device{};
    bool deviceStarted = false;

    // Lock-free parameters written by game thread, read by audio thread
    std::atomic<int> vehicleProfile{0}; // 0=V12 Supercar, 1=Superbike, 2=Speedboat, 3=V8 Coupe
    std::atomic<float> rpm{0.0f};
    std::atomic<float> throttle{0.0f};
    std::atomic<float> drift{0.0f};
    std::atomic<float> boost{0.0f};
    std::atomic<bool> airborne{false};

    std::atomic<float> masterVolume{0.46f};
    std::atomic<bool> muted{false};

    // One-shot trigger mailboxes
    std::atomic<float> triggerImpact{0.0f};
    std::atomic<float> triggerShiftPop{0.0f};
    std::atomic<bool> triggerChime{false};
    std::atomic<bool> triggerExplosion{false};

    // Audio-thread synth state
    float sampleRate = 48000.0f;
    float smoothRpm = 0.0f;
    float smoothThrottle = 0.0f;
    float smoothDrift = 0.0f;
    float smoothBoost = 0.0f;
    float smoothEngineActive = 0.0f;
    float prevThrottle = 0.0f;

    float phaseA = 0.0f;
    float phaseB = 0.0f;
    float lopePhase = 0.0f;
    float turboPhase = 0.0f;
    float screechPhase = 0.0f;
    float lpNoise = 0.0f;
    float lpNoise2 = 0.0f;
    std::uint32_t rngState = 0x5A6E1100u;

    // Exhaust formant resonators & tire filters
    SvfFilter exhaustLowL{};
    SvfFilter exhaustHighL{};
    SvfFilter exhaustLowR{};
    SvfFilter windFilter{};
    SvfFilter driftFilter{};
    SvfFilter popFilter{};

    // Active one-shot envelopes
    float popEnv = 0.0f;
    float popCooldown = 0.0f;

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

        const int profile = self->vehicleProfile.load(std::memory_order_relaxed);
        const float targetRpm = self->rpm.load(std::memory_order_relaxed);
        const float targetThrottle = self->throttle.load(std::memory_order_relaxed);
        const float targetDrift = self->drift.load(std::memory_order_relaxed);
        const float targetBoost = self->boost.load(std::memory_order_relaxed);
        const float targetActive = (targetRpm > 0.02f || targetThrottle > 0.02f) ? 1.0f : 0.0f;

        const float gain = self->muted.load(std::memory_order_relaxed)
            ? 0.0f
            : self->masterVolume.load(std::memory_order_relaxed);

        if (const float imp = self->triggerImpact.exchange(0.0f, std::memory_order_relaxed); imp > 0.01f) {
            self->impactEnv = std::max(self->impactEnv, std::clamp(imp, 0.2f, 1.0f));
            self->impactFreq = 110.0f + imp * 85.0f;
        }
        if (const float pop = self->triggerShiftPop.exchange(0.0f, std::memory_order_relaxed); pop > 0.01f) {
            self->popEnv = std::max(self->popEnv, std::clamp(pop, 0.35f, 1.0f));
        }
        if (self->triggerChime.exchange(false, std::memory_order_relaxed)) {
            self->chimeTime = 0.0f;
        }
        if (self->triggerExplosion.exchange(false, std::memory_order_relaxed)) {
            self->explosionEnv = 1.0f;
        }

        // Automatic exhaust overrun crackle when lifting off throttle at high RPM!
        if (profile != 2 && self->prevThrottle > 0.55f && targetThrottle < 0.25f && self->smoothRpm > 0.48f && self->popCooldown <= 0.0f) {
            self->popEnv = 0.78f;
            self->popCooldown = 0.28f;
        }
        self->prevThrottle = targetThrottle;

        const float dt = 1.0f / self->sampleRate;
        const float smoothK = 1.0f - std::exp(-22.0f * dt);
        const float activeK = 1.0f - std::exp(-10.0f * dt);

        for (ma_uint32 i = 0; i < frameCount; ++i) {
            self->smoothRpm += (targetRpm - self->smoothRpm) * smoothK;
            self->smoothThrottle += (targetThrottle - self->smoothThrottle) * smoothK;
            self->smoothDrift += (targetDrift - self->smoothDrift) * smoothK;
            self->smoothBoost += (targetBoost - self->smoothBoost) * smoothK;
            self->smoothEngineActive += (targetActive - self->smoothEngineActive) * activeK;
            if (self->popCooldown > 0.0f) self->popCooldown -= dt;

            const float white = self->nextNoise();
            self->lpNoise += (white - self->lpNoise) * 0.09f;
            self->lpNoise2 += (white - self->lpNoise2) * 0.24f;

            // -----------------------------------------------------------------
            // 1. MULTI-PROFILE COMBUSTION ENGINE SYNTH + EXHAUST FORMANT PIPES
            // -----------------------------------------------------------------
            float baseHz = 36.0f + self->smoothRpm * 215.0f;
            float bankRatio = 1.50f;    // Dual cylinder-bank harmonic interval
            float harmMix = 1.15f;
            float lopeHz = 8.5f + self->smoothRpm * 14.0f;
            float formantLowHz = 185.0f + self->smoothRpm * 240.0f;
            float formantHighHz = 620.0f + self->smoothRpm * 1150.0f;

            if (profile == 1) {
                // Cyberpunk Superbike: blistering high-revving 14,000 RPM inline-4 shriek!
                baseHz = 64.0f + self->smoothRpm * 340.0f;
                bankRatio = 2.00f;
                harmMix = 1.35f;
                formantLowHz = 280.0f + self->smoothRpm * 380.0f;
                formantHighHz = 950.0f + self->smoothRpm * 1650.0f;
            } else if (profile == 2) {
                // Luxury Racing Speedboat: deep rumbling twin marine V8 + water wake!
                baseHz = 28.0f + self->smoothRpm * 118.0f;
                bankRatio = 1.333f;
                harmMix = 0.85f;
                formantLowHz = 120.0f + self->smoothRpm * 150.0f;
                formantHighHz = 360.0f + self->smoothRpm * 420.0f;
            } else if (profile == 3) {
                // Sports Coupe V8: muscular cross-plane V8 burble!
                baseHz = 31.0f + self->smoothRpm * 155.0f;
                bankRatio = 1.25f;
                harmMix = 0.95f;
                lopeHz = 6.8f + self->smoothRpm * 10.0f;
                formantLowHz = 145.0f + self->smoothRpm * 190.0f;
                formantHighHz = 480.0f + self->smoothRpm * 780.0f;
            }

            // Natural mechanical cam-lope flutter (strongest at throaty idle!)
            self->lopePhase += lopeHz * dt;
            if (self->lopePhase >= 1.0f) self->lopePhase -= 1.0f;
            const float idleFactor = 1.0f - std::clamp(self->smoothRpm * 1.4f, 0.0f, 1.0f);
            const float lopeMod = 1.0f + std::sin(self->lopePhase * kTwoPi) * (0.14f * idleFactor + 0.03f);
            const float pitchFlutter = 1.0f + self->lpNoise * 0.014f;

            self->phaseA += baseHz * pitchFlutter * dt;
            if (self->phaseA >= 1.0f) self->phaseA -= 1.0f;
            self->phaseB += baseHz * bankRatio * (1.0f + 0.003f) * dt;
            if (self->phaseB >= 1.0f) self->phaseB -= 1.0f;

            // Left & right cylinder banks with slight natural timing offset
            const float bankA = combustionWave(self->phaseA, harmMix);
            const float bankB = combustionWave(self->phaseB, harmMix * 0.85f);
            const float subBass = std::sin(self->phaseA * kTwoPi * 0.5f) * 0.42f;
            const float mechGrit = self->lpNoise2 * (0.16f + self->smoothThrottle * 0.24f);

            const float rawEngine = (bankA * 0.62f + bankB * 0.36f + subBass + mechGrit) * lopeMod;
            const float drive = 1.15f + self->smoothThrottle * 1.65f + self->smoothRpm * 0.55f;
            const float driven = std::tanh(rawEngine * drive);

            // Dual Resonant Exhaust Formants (gives real pipe resonance instead of flat synth!)
            const float lowBodyL = self->exhaustLowL.processLowpass(driven, formantLowHz, 1.4f, self->sampleRate);
            const float lowBodyR = self->exhaustLowR.processLowpass(driven + bankB * 0.12f, formantLowHz * 1.03f, 1.4f, self->sampleRate);
            const float scream   = self->exhaustHighL.processBandpass(driven, formantHighHz, 1.8f, self->sampleRate);
            const float screamMix = (0.28f + self->smoothThrottle * 0.48f + self->smoothRpm * 0.35f);

            const float engGain = (0.15f + self->smoothThrottle * 0.16f + self->smoothRpm * 0.10f) * self->smoothEngineActive;
            float engL = (lowBodyL * 0.78f + scream * screamMix + driven * 0.22f) * engGain;
            float engR = (lowBodyR * 0.78f + scream * screamMix + driven * 0.22f) * engGain;

            // -----------------------------------------------------------------
            // 2. EXHAUST BACKFIRE CRACKLE / GEAR-SHIFT POPS
            // -----------------------------------------------------------------
            float popSig = 0.0f;
            if (self->popEnv > 0.002f) {
                const float crackle = self->popFilter.processBandpass(white, 340.0f + self->popEnv * 520.0f, 1.3f, self->sampleRate);
                const float flutter = (self->nextNoise() > 0.15f) ? 1.0f : 0.45f;
                popSig = std::tanh(crackle * 2.8f) * self->popEnv * flutter * 0.42f;
                self->popEnv *= std::exp(-18.0f * dt);
            }

            // -----------------------------------------------------------------
            // 3. AERODYNAMIC WIND / WATER WAKE RUSH + TURBO NITRO SPOOL
            // -----------------------------------------------------------------
            const float windCutoff = (profile == 2)
                ? (550.0f + self->smoothRpm * 1200.0f) // Foaming marine hull water spray!
                : (320.0f + self->smoothRpm * 900.0f);
            const float windBand = self->windFilter.processBandpass(white, windCutoff, 0.85f, self->sampleRate);
            const float windVol = (profile == 2)
                ? (self->smoothRpm * 0.26f * self->smoothEngineActive)
                : (std::max(0.0f, self->smoothRpm - 0.35f) * 0.14f * self->smoothEngineActive);

            self->turboPhase += (1150.0f + self->smoothRpm * 1650.0f + self->smoothBoost * 650.0f) * dt;
            if (self->turboPhase >= 1.0f) self->turboPhase -= 1.0f;
            const float turboWhine = std::sin(self->turboPhase * kTwoPi) * (0.045f * self->smoothThrottle * self->smoothRpm + 0.09f * self->smoothBoost);
            const float boostRoar = (self->lpNoise * 0.72f + windBand * 0.65f) * self->smoothBoost * 0.28f;

            // -----------------------------------------------------------------
            // 4. REALISTIC FILTERED TIRE DRIFT SQUEAL + ASPHALT SCRUB
            // -----------------------------------------------------------------
            self->screechPhase += (760.0f + self->smoothRpm * 260.0f + self->lpNoise * 130.0f) * dt;
            if (self->screechPhase >= 1.0f) self->screechPhase -= 1.0f;
            const float rawScreech = std::sin(self->screechPhase * kTwoPi) * 0.42f
                                   + std::sin(self->screechPhase * kTwoPi * 1.49f) * 0.28f
                                   + white * 0.38f;
            const float driftSig = self->driftFilter.processBandpass(rawScreech, 1180.0f + self->smoothRpm * 420.0f, 3.2f, self->sampleRate)
                                 * self->smoothDrift * 0.26f;

            // -----------------------------------------------------------------
            // 5. ONE-SHOT GAME SFX (Impacts, Chimes, Shockwave Booms)
            // -----------------------------------------------------------------
            float impactSig = 0.0f;
            if (self->impactEnv > 0.001f) {
                self->impactPhase += self->impactFreq * dt;
                if (self->impactPhase >= 1.0f) self->impactPhase -= 1.0f;
                impactSig = (std::sin(self->impactPhase * kTwoPi) * 0.68f + self->lpNoise * 0.55f) * self->impactEnv * 0.50f;
                self->impactFreq = std::max(38.0f, self->impactFreq - 290.0f * dt);
                self->impactEnv *= std::exp(-10.5f * dt);
            }

            float chimeSig = 0.0f;
            if (self->chimeTime >= 0.0f) {
                constexpr float kNotes[4] = {523.25f, 659.25f, 783.99f, 1046.50f};
                const int noteIdx = std::min(3, static_cast<int>(self->chimeTime / 0.085f));
                self->chimePhase += kNotes[noteIdx] * dt;
                if (self->chimePhase >= 1.0f) self->chimePhase -= 1.0f;
                const float env = std::exp(-5.2f * self->chimeTime);
                chimeSig = (std::sin(self->chimePhase * kTwoPi) + 0.3f * std::sin(self->chimePhase * 2.0f * kTwoPi)) * env * 0.28f;
                self->chimeTime += dt;
                if (self->chimeTime > 0.55f) {
                    self->chimeTime = -1.0f;
                }
            }

            float expSig = 0.0f;
            if (self->explosionEnv > 0.001f) {
                const float expFreq = 36.0f + self->explosionEnv * 92.0f;
                self->explosionPhase += expFreq * dt;
                if (self->explosionPhase >= 1.0f) self->explosionPhase -= 1.0f;
                expSig = (std::sin(self->explosionPhase * kTwoPi) * 0.70f + self->lpNoise * 0.72f) * self->explosionEnv * 0.58f;
                self->explosionEnv *= std::exp(-3.2f * dt);
            }

            const float sharedFx = popSig + boostRoar + turboWhine + driftSig + impactSig + chimeSig + expSig;
            const float left  = std::tanh((engL + windBand * windVol + sharedFx) * gain);
            const float right = std::tanh((engR + (windBand * 0.85f + self->lpNoise2 * 0.15f) * windVol + sharedFx) * gain);
            out[i * 2 + 0] = left;
            out[i * 2 + 1] = right;
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
              << " Hz stereo, 4-profile combustion + exhaust formant synth ready)\n";
    return true;
}

void AudioEngine::shutdown() {
    if (m_impl && m_impl->deviceStarted) {
        ma_device_uninit(&m_impl->device);
        m_impl->deviceStarted = false;
    }
}

void AudioEngine::setVehicleProfile(int profile) {
    m_impl->vehicleProfile.store(std::clamp(profile, 0, 3), std::memory_order_relaxed);
}

void AudioEngine::setVehicleState(float rpm01, float throttle01, float drift01, float boost01, bool airborne) {
    m_impl->rpm.store(std::clamp(rpm01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->throttle.store(std::clamp(throttle01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->drift.store(airborne ? 0.0f : std::clamp(drift01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->boost.store(std::clamp(boost01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_impl->airborne.store(airborne, std::memory_order_relaxed);
}

void AudioEngine::playShiftPop(float intensity01) {
    m_impl->triggerShiftPop.store(std::clamp(intensity01, 0.2f, 1.0f), std::memory_order_relaxed);
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
