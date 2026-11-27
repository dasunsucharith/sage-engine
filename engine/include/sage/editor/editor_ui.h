#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace sage {

class AudioEngine;
class PhysicsWorld;
class Window;
class World;

struct EditorStats {
    float fps = 60.0f;
    float frameMs = 16.6f;
    std::string gpuRenderer;
    std::size_t loadedModels = 0;
    std::size_t luaResources = 0;
    bool freeFlyMode = false;
};

struct EditorActions {
    std::function<void()> onHotReload;
    std::function<void()> onToggleFreeFly;
    std::function<void()> onTriggerShockwave;
    std::function<void()> onRenderCustomUi; // Called inside ImGui frame for Lua ui.* HUDs!
};

// Live in-game Dear ImGui viewport HUD & Scene Inspector (toggle with F1).
class EditorUI {
public:
    EditorUI() = default;
    ~EditorUI();

    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    bool initialize(Window& window);
    void shutdown();

    void processEvent(const void* sdlEvent);
    void render(
        World& world,
        PhysicsWorld& physics,
        AudioEngine& audio,
        const EditorStats& stats,
        const EditorActions& actions);

    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }
    void toggleVisible() { m_visible = !m_visible; }

    bool wantsCaptureMouse() const;
    bool wantsCaptureKeyboard() const;

private:
    bool m_initialized = false;
    bool m_visible = false; // Hidden during gameplay by default — press F1 to open developer inspector!
    int m_selectedEntityIndex = -1;
    float m_fpsHistory[90]{};
    int m_fpsOffset = 0;
};

} // namespace sage
