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

    // 3D Spatial Audio Listener & 8 Lock-Free Ambient Emitters (v1.3.0)
    static constexpr int kMaxSpatialEmitters = 8;
    struct SpatialEmitterSlot {
        std::atomic<bool> active{false};
        std::atomic<int> type{0}; // 0=Ocean Surf, 1=Plaza Fountain, 2=Helicopter Rotor, 3=Wind/Breeze
        std::atomic<float> x{0.0f}, y{0.0f}, z{0.0f};
        std::atomic<float> radius{60.0f};
        std::atomic<float> gain{0.0f};
    };
    std::atomic<float> listenerX{0.0f}, listenerY{2.0f}, listenerZ{0.0f};
    std::atomic<float> listenerFwdX{0.0f}, listenerFwdZ{1.0f};
    SpatialEmitterSlot spatialEmitters[kMaxSpatialEmitters]{};

    // Spatial audio synth phases & filters (audio-thread only)
    float surfPhase = 0.0f;
    float rotorPhase = 0.0f;
    SvfFilter surfFilter{};
    SvfFilter fountainFilter{};
    SvfFilter breezeFilter{};

    // One-shot trigger mailboxes
    std::atomic<float> triggerImpact{0.0f};
    std::atomic<float> triggerShiftPop{0.0f};
    std::atomic<float> triggerFootstep{0.0f};
    std::atomic<int> triggerFootSurface{0};
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
    SvfFilter footFilter{};

    // Active one-shot envelopes
    float popEnv = 0.0f;
    float popCooldown = 0.0f;

    float footEnv = 0.0f;
    float footPhase = 0.0f;
    float footFreq = 110.0f;
    int footSurface = 0;

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

        // Precompute equal-power stereo gains for the 4 spatial ambient voice types once per audio buffer!
        const float lx = self->listenerX.load(std::memory_order_relaxed);
        const float ly = self->listenerY.load(std::memory_order_relaxed);
        const float lz = self->listenerZ.load(std::memory_order_relaxed);
        float fx = self->listenerFwdX.load(std::memory_order_relaxed);
        float fz = self->listenerFwdZ.load(std::memory_order_relaxed);
        const float fLen = std::sqrt(fx * fx + fz * fz);
        if (fLen > 0.001f) { fx /= fLen; fz /= fLen; } else { fx = 0.0f; fz = 1.0f; }
        // Right-handed +Y up: right = cross(forward, up) = (-fz, 0, fx)
        const float rx = -fz;
        const float rz = fx;

        float spatialGainL[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float spatialGainR[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        for (int s = 0; s < kMaxSpatialEmitters; ++s) {
            if (!self->spatialEmitters[s].active.load(std::memory_order_relaxed)) continue;
            const int stype = std::clamp(self->spatialEmitters[s].type.load(std::memory_order_relaxed), 0, 3);
            const float ex = self->spatialEmitters[s].x.load(std::memory_order_relaxed);
            const float ey = self->spatialEmitters[s].y.load(std::memory_order_relaxed);
            const float ez = self->spatialEmitters[s].z.load(std::memory_order_relaxed);
            const float rad = std::max(self->spatialEmitters[s].radius.load(std::memory_order_relaxed), 4.0f);
            const float egain = self->spatialEmitters[s].gain.load(std::memory_order_relaxed);
            const float dx = ex - lx, dy = ey - ly, dz = ez - lz;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < rad && egain > 0.001f) {
                const float att = 1.0f - (dist / rad);
                const float vol = att * att * egain;
                const float invD = dist > 0.1f ? (1.0f / dist) : 0.0f;
                const float pan = std::clamp((dx * rx + dz * rz) * invD, -1.0f, 1.0f);
                const float panAngle = (pan * 0.5f + 0.5f) * (kPi * 0.5f);
                spatialGainL[stype] += vol * std::cos(panAngle);
                spatialGainR[stype] += vol * std::sin(panAngle);
            }
        }

        if (const float imp = self->triggerImpact.exchange(0.0f, std::memory_order_relaxed); imp > 0.01f) {
            self->impactEnv = std::max(self->impactEnv, std::clamp(imp, 0.2f, 1.0f));
            self->impactFreq = 110.0f + imp * 85.0f;
        }
        if (const float pop = self->triggerShiftPop.exchange(0.0f, std::memory_order_relaxed); pop > 0.01f) {
            self->popEnv = std::max(self->popEnv, std::clamp(pop, 0.35f, 1.0f));
        }
        if (const float step = self->triggerFootstep.exchange(0.0f, std::memory_order_relaxed); step > 0.01f) {
            self->footEnv = std::clamp(step, 0.15f, 1.0f);
            self->footSurface = self->triggerFootSurface.load(std::memory_order_relaxed);
            self->footPhase = 0.0f;
            const float jitter = 0.90f + 0.20f * (0.5f + 0.5f * self->nextNoise());
            self->footFreq = (self->footSurface == 2 ? 128.0f : (self->footSurface == 0 ? 105.0f : 82.0f)) * jitter;
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

            // Procedural surface-aware footstep synth (0=Asphalt, 1=Grass/Sand, 2=Wood Pier, 3=Water Splash)
            float footSig = 0.0f;
            if (self->footEnv > 0.002f) {
                self->footPhase += self->footFreq * dt;
                if (self->footPhase >= 1.0f) self->footPhase -= 1.0f;
                const float thump = std::sin(self->footPhase * kTwoPi);
                float decay = 28.0f;
                if (self->footSurface == 1) {
                    // Soft grass / sandy crunch
                    const float crunch = self->footFilter.processBandpass(white, 1350.0f, 0.9f, self->sampleRate);
                    footSig = (thump * 0.22f + crunch * 0.78f) * self->footEnv * 0.34f;
                    decay = 20.0f;
                } else if (self->footSurface == 2) {
                    // Hollow wooden Marina Pier board thump
                    const float wood = self->footFilter.processBandpass(thump + white * 0.22f, 240.0f, 2.2f, self->sampleRate);
                    footSig = (thump * 0.58f + wood * 0.75f) * self->footEnv * 0.42f;
                    decay = 24.0f;
                } else if (self->footSurface == 3) {
                    // Shallow water splash
                    const float splash = self->footFilter.processBandpass(white, 920.0f + self->footEnv * 750.0f, 0.75f, self->sampleRate);
                    footSig = (splash * 0.85f + self->lpNoise * 0.30f) * self->footEnv * 0.38f;
                    decay = 11.5f;
                } else {
                    // Crisp sneaker sole on asphalt / concrete
                    const float scuff = self->footFilter.processBandpass(white, 780.0f, 1.2f, self->sampleRate);
                    footSig = (thump * 0.52f + scuff * 0.48f) * self->footEnv * 0.32f;
                    decay = 32.0f;
                }
                self->footFreq = std::max(42.0f, self->footFreq - 380.0f * dt);
                self->footEnv *= std::exp(-decay * dt);
            }

            // -----------------------------------------------------------------
            // 6. 3D SPATIAL WORLD AMBIENT EMITTERS (Ocean, Fountain, Heli, Wind!)
            // -----------------------------------------------------------------
            float spL = 0.0f, spR = 0.0f;
            if (spatialGainL[0] + spatialGainR[0] > 0.001f) {
                self->surfPhase += 0.19f * dt;
                if (self->surfPhase >= 1.0f) self->surfPhase -= 1.0f;
                const float swell = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(self->surfPhase * kTwoPi));
                const float surf = self->surfFilter.processLowpass(white, 240.0f + swell * 480.0f, 0.8f, self->sampleRate) * swell * 0.36f;
                spL += surf * spatialGainL[0];
                spR += surf * spatialGainR[0];
            }
            if (spatialGainL[1] + spatialGainR[1] > 0.001f) {
                const float fnt = self->fountainFilter.processBandpass(white, 2200.0f, 0.7f, self->sampleRate) * 0.24f;
                spL += fnt * spatialGainL[1];
                spR += fnt * spatialGainR[1];
            }
            if (spatialGainL[2] + spatialGainR[2] > 0.001f) {
                self->rotorPhase += 13.5f * dt;
                if (self->rotorPhase >= 1.0f) self->rotorPhase -= 1.0f;
                const float chop = std::pow(0.5f + 0.5f * std::sin(self->rotorPhase * kTwoPi), 3.0f);
                const float rotor = (std::sin(self->rotorPhase * kTwoPi * 2.0f) * 0.45f + self->lpNoise * 0.85f) * chop * 0.42f;
                spL += rotor * spatialGainL[2];
                spR += rotor * spatialGainR[2];
            }
            if (spatialGainL[3] + spatialGainR[3] > 0.001f) {
                const float wind = self->breezeFilter.processBandpass(white, 380.0f, 1.4f, self->sampleRate) * 0.25f;
                spL += wind * spatialGainL[3];
                spR += wind * spatialGainR[3];
            }

            const float sharedFx = popSig + boostRoar + turboWhine + driftSig + impactSig + chimeSig + expSig + footSig;
            const float left  = std::tanh((engL + windBand * windVol + sharedFx + spL) * gain);
            const float right = std::tanh((engR + (windBand * 0.85f + self->lpNoise2 * 0.15f) * windVol + sharedFx + spR) * gain);
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

void AudioEngine::playFootstep(int surface, float intensity01) {
    m_impl->triggerFootSurface.store(std::clamp(surface, 0, 3), std::memory_order_relaxed);
    m_impl->triggerFootstep.store(std::clamp(intensity01, 0.15f, 1.0f), std::memory_order_relaxed);
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

void AudioEngine::setListener(float x, float y, float z, float fwdX, float fwdZ) {
    m_impl->listenerX.store(x, std::memory_order_relaxed);
    m_impl->listenerY.store(y, std::memory_order_relaxed);
    m_impl->listenerZ.store(z, std::memory_order_relaxed);
    m_impl->listenerFwdX.store(fwdX, std::memory_order_relaxed);
    m_impl->listenerFwdZ.store(fwdZ, std::memory_order_relaxed);
}

void AudioEngine::setSpatialEmitter(int slot, int type, float x, float y, float z, float radius, float gain) {
    if (slot < 0 || slot >= Impl::kMaxSpatialEmitters) {
        return;
    }
    auto& e = m_impl->spatialEmitters[slot];
    e.type.store(std::clamp(type, 0, 3), std::memory_order_relaxed);
    e.x.store(x, std::memory_order_relaxed);
    e.y.store(y, std::memory_order_relaxed);
    e.z.store(z, std::memory_order_relaxed);
    e.radius.store(std::max(radius, 4.0f), std::memory_order_relaxed);
    e.gain.store(std::clamp(gain, 0.0f, 1.5f), std::memory_order_relaxed);
    e.active.store(gain > 0.001f, std::memory_order_relaxed);
}

void AudioEngine::clearSpatialEmitters() {
    for (int i = 0; i < Impl::kMaxSpatialEmitters; ++i) {
        m_impl->spatialEmitters[i].active.store(false, std::memory_order_relaxed);
    }
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
