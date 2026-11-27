#pragma once

#include <memory>

namespace sage {

// Real-time vehicle & game audio engine powered by miniaudio 0.11.
// Synthesizes dynamic V12 engine harmonics, nitro afterburner, tire drift screech,
// crate smash impacts, shockwave booms, and delivery chimes out of the box.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // silentMode = true skips opening the OS audio device (used for --smoke-test)
    bool initialize(bool silentMode = false);
    void shutdown();

    void setVehicleState(float rpm01, float throttle01, float drift01, float boost01, bool airborne);
    void playImpact(float intensity01 = 0.8f);
    void playChime();
    void playExplosion();

    void setMasterVolume(float volume01);
    float masterVolume() const;
    void setMuted(bool muted);
    bool isMuted() const;
    bool isActive() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
