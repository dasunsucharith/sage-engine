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
    std::function<double()> getTimeSeconds;
    std::function<void()> beforeResourcesStart;
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

    bool reloadAll();
    void tick(double dt);
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
