#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "sage/renderer/renderer.h"

namespace sage {

class AudioEngine;
class PhysicsWorld;
class Window;
class World;

struct EditorStats {
    float fps = 60.0f;
    float frameMs = 16.6f;
    std::string gpuRenderer;
    std::string assetsDir;
    std::size_t loadedModels = 0;
    std::size_t grassClumps = 0;
    std::size_t culledEntities = 0;
    std::size_t drawCalls = 0;
    std::size_t instancedBatches = 0;
    std::size_t instancedProps = 0;
    std::size_t savedDrawCalls = 0;
    std::size_t luaResources = 0;
    bool freeFlyMode = false;
    std::vector<GpuPassTiming> gpuPasses; // Latest completed GPU frame, per render pass
};

struct EditorActions {
    std::function<void()> onHotReload;
    std::function<void()> onToggleFreeFly;
    std::function<void()> onTriggerShockwave;
    std::function<bool()> onSaveScene;
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
    int m_selectedModelIndex = 0;
    int m_spawnCounter = 1;
    std::string m_saveStatusMessage;
    float m_fpsHistory[90]{};
    int m_fpsOffset = 0;
    std::vector<std::pair<std::string, float>> m_gpuPassSmoothed; // Smoothed for readable display
};

} // namespace sage
