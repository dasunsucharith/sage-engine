#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace sage {

class InputState;
class World;

struct LuaHostCallbacks {
    std::function<void(const std::string&)> setWindowTitle;
    std::function<double()> getTimeSeconds;
    std::function<void()> beforeResourcesStart; // Reloads active JSON scene before Lua onStart()
};

// FiveM-style Lua scripting runtime powered by Lua 5.4 + sol2.
// Discovers resources/<name>/manifest.json, executes isolated Lua scripts,
// and supports live hot-reloading without recompiling C++.
class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    bool initialize(
        const std::filesystem::path& resourcesRoot,
        World& world,
        const InputState& input,
        LuaHostCallbacks callbacks);

    // Shuts down active resources, clears the world, reloads the scene, and starts Lua resources.
    bool reloadAll();

    // Runs onTick(dt) on all active Lua resources.
    void tick(double dt);

    // Checks file timestamps under resources/ and reloads automatically if anything changed.
    bool checkHotReload();

    void shutdown();

    std::size_t activeResourceCount() const;
    std::size_t reloadCount() const;
    const std::filesystem::path& resourcesRoot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
