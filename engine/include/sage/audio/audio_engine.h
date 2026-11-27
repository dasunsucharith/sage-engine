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

    // profile: 0 = Supercar V12, 1 = Superbike I4, 2 = Marine Speedboat V8, 3 = Sports Coupe V8
    void setVehicleProfile(int profile);
    void setVehicleState(float rpm01, float throttle01, float drift01, float boost01, bool airborne);
    void playShiftPop(float intensity01 = 0.75f);
    // surface: 0 = Asphalt/Concrete, 1 = Grass/Dirt, 2 = Wood Pier, 3 = Water Splash
    void playFootstep(int surface = 0, float intensity01 = 0.65f);
    void playImpact(float intensity01 = 0.8f);
    void playChime();
    void playExplosion();

    // 3D Spatial Audio Listener & Ambient Emitters (v1.3.0: up to 8 live stereo-panned emitters!)
    // type: 0 = Ocean Surf, 1 = Plaza Fountain, 2 = Helicopter Rotor, 3 = Wind Turbine/Breeze
    void setListener(float x, float y, float z, float fwdX, float fwdZ);
    void setSpatialEmitter(int slot, int type, float x, float y, float z, float radius, float gain);
    void clearSpatialEmitters();

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
