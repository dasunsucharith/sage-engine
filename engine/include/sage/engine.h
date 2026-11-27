#pragma once

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
    std::string appName = "SAGE Engine v0.3 — 3D Stunt & Delivery Sandbox";
    int windowWidth = 1280;
    int windowHeight = 720;
    bool vsync = true;
    double fixedTimeStep = 1.0 / 60.0; // 60 Hz deterministic simulation tick
    int maxFrames = 0;                 // 0 = run until window closes (non-zero for --smoke-test)
    std::filesystem::path resourcesPath = "resources";
    std::filesystem::path scenePath = "scenes/stunt_city.json";
};

class Engine {
public:
    explicit Engine(const EngineConfig& config);
    ~Engine();

    // Initializes Window, 3D Renderer, JSON Scene, and Lua VM, then runs the main loop.
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

    std::filesystem::path resolveProjectPath(const std::filesystem::path& relativePath) const;

    EngineConfig m_config;
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_resolvedScenePath;
    std::filesystem::file_time_type m_sceneWriteTime{};
    bool m_hasSceneWriteTime = false;

    Window m_window;
    InputState m_input;
    World m_world;
    LuaEngine m_lua;
    std::unique_ptr<Renderer> m_renderer;

    // Built-in Free-Fly 3D Camera (press Tab anytime to inspect the 3D scene!)
    bool m_freeFlyMode = false;
    Vec3 m_flyPos{0.0f, 12.0f, -24.0f};
    float m_flyYawDeg = 0.0f;
    float m_flyPitchDeg = -20.0f;
    float m_flySpeed = 24.0f;

    std::string m_scriptTitle;
    double m_startTime = 0.0;
    bool m_running = false;
};

} // namespace sage
