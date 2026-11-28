#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace sage {

class AudioEngine;
class InputState;
class PhysicsWorld;
class World;

struct LuaHostCallbacks {
    std::function<void(const std::string&)> setWindowTitle;
    std::function<double()> getTimeSeconds;      // Simulation time (advances by the fixed timestep per tick)
    std::function<double()> getRealTimeSeconds;  // Wall-clock time since engine start (for UI effects/profiling)
    std::function<void()> beforeResourcesStart;
    std::function<void()> requestQuit;
    std::function<bool()> isUiCapturingKeyboard;
    std::function<bool()> isUiCapturingMouse;
};

// FiveM-style Lua scripting runtime powered by Lua 5.4 + sol2.
class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    bool initialize(
        const std::filesystem::path& resourcesRoot,
        World& world,
        PhysicsWorld& physics,
        AudioEngine& audio,
        const InputState& input,
        LuaHostCallbacks callbacks);

    void setResourceFilter(const std::string& onlyResourceName);
    bool reloadAll();
    void tick(double dt);
    void renderUI();
    bool checkHotReload();

    // Runs a Lua snippet for remote tools (the MCP bridge's run_lua): sandboxed like a
    // resource, with the normal world/physics/camera/... API, print() captured, and the
    // usual 1 s watchdog. Expressions return their value ("world.getPosition('hero')").
    // Returns false (with the error in `output`) if it fails to compile or errors.
    bool runSnippet(const std::string& code, std::string& output);
    void shutdown();

    std::size_t activeResourceCount() const;
    std::size_t reloadCount() const;
    const std::filesystem::path& resourcesRoot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
