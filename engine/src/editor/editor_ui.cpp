#include "sage/editor/editor_ui.h"

#include "sage/audio/audio_engine.h"
#include "sage/physics/physics_world.h"
#include "sage/platform/window.h"
#include "sage/world/world.h"

#include <SDL3/SDL.h>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <iostream>

namespace sage {

EditorUI::~EditorUI() {
    shutdown();
}

bool EditorUI::initialize(Window& window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Keep workspace clean without imgui.ini clutter

    // Modern dark translucent SAGE Engine editor style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.09f, 0.13f, 0.86f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.24f, 0.38f, 0.95f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.36f, 0.56f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.48f, 0.72f, 0.95f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.25f, 0.82f, 0.95f, 1.0f);

    if (!ImGui_ImplSDL3_InitForOpenGL(window.nativeWindow(), window.nativeGLContext())) {
        std::cerr << "[SAGE][Editor] ImGui_ImplSDL3_InitForOpenGL failed.\n";
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        std::cerr << "[SAGE][Editor] ImGui_ImplOpenGL3_Init failed.\n";
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_initialized = true;
    std::cout << "[SAGE][Editor] Dear ImGui " << IMGUI_VERSION << " live viewport HUD ready (Press F1 to toggle)\n";
    return true;
}

void EditorUI::shutdown() {
    if (!m_initialized) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void EditorUI::processEvent(const void* sdlEvent) {
    if (!m_initialized || !sdlEvent) {
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdlEvent));
}

bool EditorUI::wantsCaptureMouse() const {
    return m_initialized && m_visible && ImGui::GetIO().WantCaptureMouse;
}

bool EditorUI::wantsCaptureKeyboard() const {
    return m_initialized && m_visible && ImGui::GetIO().WantCaptureKeyboard;
}

void EditorUI::render(
    World& world,
    PhysicsWorld& physics,
    AudioEngine& audio,
    const EditorStats& stats,
    const EditorActions& actions) {
    if (!m_initialized) {
        return;
    }

    m_fpsHistory[m_fpsOffset] = stats.fps;
    m_fpsOffset = (m_fpsOffset + 1) % 90;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (m_visible) {
        ImGui::SetNextWindowPos(ImVec2(14.0f, 14.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("SAGE Engine v0.4 — Live Inspector (F1)", &m_visible)) {
            ImGui::Text("FPS: %.0f (%.2f ms)", stats.fps, stats.frameMs);
            ImGui::PlotLines("##fps", m_fpsHistory, 90, m_fpsOffset, nullptr, 0.0f, 165.0f, ImVec2(-1.0f, 36.0f));
            ImGui::TextDisabled("GPU: %s", stats.gpuRenderer.c_str());
            ImGui::Text("Entities: %zu | .glb Models: %zu | Lua: %zu",
                world.entityCount(), stats.loadedModels, stats.luaResources);
            ImGui::Text("Jolt Bodies: %zu (%zu dynamic)",
                physics.bodyCount(), physics.dynamicBodyCount());

            ImGui::Separator();
            if (ImGui::Button("Hot-Reload (F5)")) {
                if (actions.onHotReload) actions.onHotReload();
            }
            ImGui::SameLine();
            if (ImGui::Button(stats.freeFlyMode ? "Drive Cam (Tab)" : "Free-Fly (Tab)")) {
                if (actions.onToggleFreeFly) actions.onToggleFreeFly();
            }
            ImGui::SameLine();
            if (ImGui::Button("Shockwave (X)")) {
                if (actions.onTriggerShockwave) actions.onTriggerShockwave();
            }

            // --- Sky & Lighting ---
            if (ImGui::CollapsingHeader("Sky & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                float tod = world.sky().timeOfDay;
                if (ImGui::SliderFloat("Time of Day", &tod, 0.0f, 24.0f, "%.1f h")) {
                    world.setTimeOfDay(tod);
                }
                if (ImGui::SmallButton("Sunrise")) world.setTimeOfDay(6.4f);
                ImGui::SameLine();
                if (ImGui::SmallButton("Noon")) world.setTimeOfDay(12.5f);
                ImGui::SameLine();
                if (ImGui::SmallButton("Sunset")) world.setTimeOfDay(17.5f);
                ImGui::SameLine();
                if (ImGui::SmallButton("Night")) world.setTimeOfDay(22.5f);
            }

            // --- Jolt Physics Controls ---
            if (ImGui::CollapsingHeader("Jolt Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                Vec3 g = physics.getGravity();
                float gy = g.y;
                if (ImGui::SliderFloat("Gravity Y", &gy, -40.0f, 2.0f, "%.1f m/s^2")) {
                    physics.setGravity({g.x, gy, g.z});
                }
                if (ImGui::SmallButton("Earth (-22)")) physics.setGravity({0.0f, -22.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::SmallButton("Moon (-5)")) physics.setGravity({0.0f, -5.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::SmallButton("Zero-G (0)")) physics.setGravity({0.0f, 0.0f, 0.0f});
            }

            // --- Audio Controls ---
            if (ImGui::CollapsingHeader("Audio (miniaudio)")) {
                bool muted = audio.isMuted();
                if (ImGui::Checkbox("Mute Audio", &muted)) {
                    audio.setMuted(muted);
                }
                float vol = audio.masterVolume();
                if (ImGui::SliderFloat("Master Volume", &vol, 0.0f, 1.0f, "%.2f")) {
                    audio.setMasterVolume(vol);
                }
            }

            // --- Scene Entity Inspector ---
            if (ImGui::CollapsingHeader("Scene Entities")) {
                auto& entities = world.entitiesMutable();
                if (ImGui::BeginListBox("##entities", ImVec2(-1.0f, 115.0f))) {
                    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
                        const bool selected = (m_selectedEntityIndex == i);
                        if (ImGui::Selectable(entities[i].id.c_str(), selected)) {
                            m_selectedEntityIndex = i;
                        }
                    }
                    ImGui::EndListBox();
                }

                if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size())) {
                    Entity& e = entities[m_selectedEntityIndex];
                    ImGui::Text("Selected: %s", e.id.c_str());
                    float pos[3] = {e.position.x, e.position.y, e.position.z};
                    if (ImGui::DragFloat3("Position", pos, 0.2f)) {
                        e.position = {pos[0], pos[1], pos[2]};
                    }
                    float rot[3] = {e.rotation.x, e.rotation.y, e.rotation.z};
                    if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
                        e.rotation = {rot[0], rot[1], rot[2]};
                        e.useQuaternion = false;
                    }
                    float scl[3] = {e.scale.x, e.scale.y, e.scale.z};
                    if (ImGui::DragFloat3("Scale", scl, 0.05f, 0.05f, 200.0f)) {
                        e.scale = {scl[0], scl[1], scl[2]};
                    }
                    float col[3] = {e.color.r, e.color.g, e.color.b};
                    if (ImGui::ColorEdit3("Color / Tint", col)) {
                        e.color = {col[0], col[1], col[2]};
                    }
                    ImGui::Checkbox("Visible", &e.visible);
                    ImGui::SameLine();
                    ImGui::Checkbox("Emissive", &e.emissive);
                }
            }

            ImGui::Separator();
            ImGui::TextWrapped("Controls: WASD Drive | Space Drift | Shift Nitro | C Cam | Right-Mouse Orbit | X Shockwave | B Drop Crates | T Sky");
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace sage
