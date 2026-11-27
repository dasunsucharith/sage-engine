#pragma once

#include "sage/audio/audio_engine.h"
#include "sage/editor/editor_ui.h"
#include "sage/physics/physics_world.h"
#include "sage/platform/input.h"
#include "sage/platform/window.h"
#include "sage/renderer/renderer.h"
#include "sage/scripting/lua_engine.h"
#include "sage/world/world.h"

#include <filesystem>
#include <memory>
#include <string>

namespace sage {

struct EngineConfig {
    std::string appName = "SAGE Engine v0.4 — 3D Stunt & Delivery Sandbox";
    int windowWidth = 1280;
    int windowHeight = 720;
    bool vsync = true;
    double fixedTimeStep = 1.0 / 60.0;
    int maxFrames = 0;                 // 0 = run until window closes (non-zero for --smoke-test)
    bool silentAudio = false;          // true during --smoke-test
    std::filesystem::path resourcesPath = "resources";
    std::filesystem::path scenePath = "scenes/stunt_city.json";
};

class Engine {
public:
    explicit Engine(const EngineConfig& config);
    ~Engine();

    int run();

private:
    bool initialize();
    void shutdown();
    void fixedTick(double dt);
    void updateFreeFlyCamera(float dt);
    void syncFreeFlyFromWorldCamera();
    void render(double elapsedSeconds);
    void updateWindowTitle(double fps);
    bool checkSceneHotReload();
    void writeSessionTelemetry(int totalFrames, double totalSeconds) const;

    std::filesystem::path resolveProjectPath(const std::filesystem::path& relativePath) const;

    EngineConfig m_config;
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_resolvedScenePath;
    std::filesystem::file_time_type m_sceneWriteTime{};
    bool m_hasSceneWriteTime = false;

    Window m_window;
    InputState m_input;
    World m_world;
    PhysicsWorld m_physics;
    AudioEngine m_audio;
    EditorUI m_editor;
    LuaEngine m_lua;
    std::unique_ptr<Renderer> m_renderer;

    bool m_freeFlyMode = false;
    Vec3 m_flyPos{0.0f, 12.0f, -24.0f};
    float m_flyYawDeg = 0.0f;
    float m_flyPitchDeg = -20.0f;
    float m_flySpeed = 24.0f;

    float m_smoothedFps = 60.0f;
    float m_lastFrameMs = 16.6f;
    float m_peakFrameMs = 0.0f;
    double m_frameMsAccum = 0.0;

    std::string m_scriptTitle;
    double m_startTime = 0.0;
    bool m_running = false;
};

} // namespace sage
