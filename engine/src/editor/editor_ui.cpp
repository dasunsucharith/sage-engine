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
#include <filesystem>
#include <iostream>
#include <vector>

namespace sage {

EditorUI::~EditorUI() {
    shutdown();
}

bool EditorUI::initialize(Window& window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.09f, 0.13f, 0.88f);
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
    std::cout << "[SAGE][Editor] Dear ImGui " << IMGUI_VERSION << " v1.5 AAA World Editor ready (Press F1 to toggle)\n";
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
        ImGui::SetNextWindowSize(ImVec2(345.0f, 0.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("SAGE Engine v1.5.0 — AAA World Editor (F1)", &m_visible)) {
            ImGui::Text("FPS: %.0f (%.2f ms)", stats.fps, stats.frameMs);
            ImGui::PlotLines("##fps", m_fpsHistory, 90, m_fpsOffset, nullptr, 0.0f, 165.0f, ImVec2(-1.0f, 34.0f));
            ImGui::TextDisabled("GPU: %s", stats.gpuRenderer.c_str());
            ImGui::Text("Entities: %zu (Culled: %zu) | Ragdolls: %zu",
                world.entityCount(), stats.culledEntities, world.activeRagdollCount());
            ImGui::Text("Draw Calls: %zu | Batches: %zu (%zu props, -%zu calls)",
                stats.drawCalls, stats.instancedBatches, stats.instancedProps, stats.savedDrawCalls);
            ImGui::Text("Grass Blades: %zu | Jolt: %zu | Lights: %zu",
                stats.grassClumps * 2, physics.bodyCount(), world.pointLights().size());

            ImGui::Separator();
            if (ImGui::Button("Hot-Reload (F5)")) {
                if (actions.onHotReload) actions.onHotReload();
            }
            ImGui::SameLine();
            if (ImGui::Button(stats.freeFlyMode ? "Play Cam (Tab)" : "Free-Fly (Tab)")) {
                if (actions.onToggleFreeFly) actions.onToggleFreeFly();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Scene")) {
                if (actions.onSaveScene && actions.onSaveScene()) {
                    m_saveStatusMessage = "Saved -> scenes/exported_scene.json";
                } else {
                    m_saveStatusMessage = "Save failed (check console)";
                }
            }
            if (!m_saveStatusMessage.empty()) {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.65f, 1.0f), "%s", m_saveStatusMessage.c_str());
            }

            // --- Sky, Weather & Lighting ---
            if (ImGui::CollapsingHeader("Sky, Weather & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                float tod = world.sky().timeOfDay;
                if (ImGui::SliderFloat("Time of Day", &tod, 0.0f, 24.0f, "%.1f h")) {
                    world.setTimeOfDay(tod);
                }
                if (ImGui::SmallButton("Sunrise")) world.setTimeOfDay(6.4f);
                ImGui::SameLine();
                if (ImGui::SmallButton("Noon")) world.setTimeOfDay(12.5f);
                ImGui::SameLine();
                if (ImGui::SmallButton("Sunset")) world.setTimeOfDay(17.4f);
                ImGui::SameLine();
                if (ImGui::SmallButton("Starry Night")) world.setTimeOfDay(22.5f);

                ImGui::SliderFloat("Rain Storm", &world.sky().rainIntensity, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Road Wetness", &world.sky().wetness, 0.0f, 1.0f, "%.2f");
                if (ImGui::SmallButton("Clear Sky")) {
                    world.sky().rainIntensity = 0.0f;
                    world.sky().wetness = 0.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Light Drizzle")) {
                    world.sky().rainIntensity = 0.35f;
                    world.sky().wetness = 0.55f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Neon Rainstorm")) {
                    world.sky().rainIntensity = 0.95f;
                    world.sky().wetness = 1.0f;
                }

                ImGui::Checkbox("3D Heightmap Terrain (Hills & Peaks)", &world.terrain().enabled);
                ImGui::Checkbox("GPU-Instanced Prop Batching (Trees/Lamps/Rocks)", &world.sky().instancedPropsEnabled);
                ImGui::Checkbox("GPU Instanced Wind Grass (57k+ Blades)", &world.sky().grassEnabled);
                ImGui::Checkbox("Screen-Space Reflections (SSR)", &world.sky().ssrEnabled);
                if (world.sky().ssrEnabled) {
                    ImGui::SliderFloat("SSR Strength", &world.sky().ssrIntensity, 0.0f, 1.2f, "%.2f");
                }
                ImGui::Checkbox("3-Cascade Soft Shadows (2048px 3-CSM)", &world.sky().shadowsEnabled);
                ImGui::Checkbox("Drifting Cloud Shadows", &world.sky().cloudShadowsEnabled);
                ImGui::Checkbox("Screen-Space Contact AO (SSAO)", &world.sky().ssaoEnabled);
                ImGui::Checkbox("FXAA 3.11 Edge Anti-Aliasing", &world.sky().fxaaEnabled);
                ImGui::Checkbox("Volumetric Sun Shafts (God-Rays)", &world.sky().godRaysEnabled);
                if (world.sky().godRaysEnabled) {
                    ImGui::SliderFloat("God-Ray Strength", &world.sky().godRayIntensity, 0.0f, 1.2f, "%.2f");
                }
                ImGui::Checkbox("HDR Bloom Post-Processing", &world.sky().bloomEnabled);
                if (world.sky().bloomEnabled) {
                    ImGui::SliderFloat("Bloom Intensity", &world.sky().bloomIntensity, 0.0f, 1.5f, "%.2f");
                }
                ImGui::SliderFloat("HDR Exposure", &world.sky().exposure, 0.4f, 2.0f, "%.2f");
                const TerrainSample ts = world.sampleTerrain(world.camera().target.x, world.camera().target.z);
                ImGui::TextDisabled("Terrain Elev @ Target: %.2fm | AI Telemetry: logs/last_session_telemetry.json", ts.height);
            }

            // --- Live World Builder (Spawn any .glb model or primitive in front of camera!) ---
            if (ImGui::CollapsingHeader("World Builder (Spawn Objects)")) {
                static std::vector<std::string> cachedModels;
                if (cachedModels.empty() && !stats.assetsDir.empty()) {
                    std::error_code ec;
                    const std::filesystem::path modelsDir = std::filesystem::path(stats.assetsDir) / "models";
                    if (std::filesystem::exists(modelsDir, ec)) {
                        for (const auto& entry : std::filesystem::directory_iterator(modelsDir, ec)) {
                            if (entry.is_regular_file(ec) && entry.path().extension() == ".glb") {
                                cachedModels.push_back(entry.path().filename().string());
                            }
                        }
                        std::sort(cachedModels.begin(), cachedModels.end());
                    }
                }

                const Vec3 spawnPos = {world.camera().target.x, 0.0f, world.camera().target.z};
                if (!cachedModels.empty()) {
                    m_selectedModelIndex = std::clamp(m_selectedModelIndex, 0, static_cast<int>(cachedModels.size()) - 1);
                    if (ImGui::BeginCombo(".glb Model", cachedModels[static_cast<std::size_t>(m_selectedModelIndex)].c_str())) {
                        for (int i = 0; i < static_cast<int>(cachedModels.size()); ++i) {
                            const bool sel = (m_selectedModelIndex == i);
                            if (ImGui::Selectable(cachedModels[static_cast<std::size_t>(i)].c_str(), sel)) {
                                m_selectedModelIndex = i;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Spawn Selected .glb Here", ImVec2(-1.0f, 0.0f))) {
                        const std::string id = "editor_model_" + std::to_string(m_spawnCounter++);
                        const std::string path = "assets/models/" + cachedModels[static_cast<std::size_t>(m_selectedModelIndex)];
                        world.spawnModel(id, path, spawnPos, {1.0f, 1.0f, 1.0f});
                        m_selectedEntityIndex = static_cast<int>(world.entityCount()) - 1;
                    }
                }
                if (ImGui::SmallButton("+ Box")) {
                    const std::string id = "editor_box_" + std::to_string(m_spawnCounter++);
                    world.spawnEntity(id, EntityShape::Box, {spawnPos.x, 1.0f, spawnPos.z}, {2.0f, 2.0f, 2.0f}, {0.2f, 0.8f, 1.0f});
                    m_selectedEntityIndex = static_cast<int>(world.entityCount()) - 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Water")) {
                    const std::string id = "editor_water_" + std::to_string(m_spawnCounter++);
                    world.spawnEntity(id, EntityShape::Water, {spawnPos.x, -0.25f, spawnPos.z}, {24.0f, 1.0f, 24.0f}, {0.06f, 0.58f, 0.72f});
                    m_selectedEntityIndex = static_cast<int>(world.entityCount()) - 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Glow Ring")) {
                    const std::string id = "editor_ring_" + std::to_string(m_spawnCounter++);
                    world.spawnEntity(id, EntityShape::Ring, {spawnPos.x, 0.2f, spawnPos.z}, {6.0f, 4.0f, 6.0f}, {1.0f, 0.8f, 0.15f});
                    world.setEmissive(id, true);
                    m_selectedEntityIndex = static_cast<int>(world.entityCount()) - 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Light")) {
                    const std::string lid = "editor_light_" + std::to_string(m_spawnCounter++);
                    world.setPointLight(lid, {spawnPos.x, 4.0f, spawnPos.z}, {0.2f, 0.9f, 1.0f}, 24.0f, 2.8f);
                }
            }

            // --- Jolt Physics Controls ---
            if (ImGui::CollapsingHeader("Jolt Physics")) {
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
                ImGui::SameLine();
                if (ImGui::SmallButton("Shockwave (X)")) {
                    if (actions.onTriggerShockwave) actions.onTriggerShockwave();
                }
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
                    if (ImGui::DragFloat3("Scale", scl, 0.05f, 0.05f, 300.0f)) {
                        e.scale = {scl[0], scl[1], scl[2]};
                    }
                    float col[3] = {e.color.r, e.color.g, e.color.b};
                    if (ImGui::ColorEdit3("Color / Tint", col)) {
                        e.color = {col[0], col[1], col[2]};
                    }
                    ImGui::Checkbox("Visible", &e.visible);
                    ImGui::SameLine();
                    ImGui::Checkbox("Emissive", &e.emissive);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete")) {
                        const std::string delId = e.id;
                        world.destroy(delId);
                        m_selectedEntityIndex = -1;
                    } else if (e.shape == EntityShape::Model && !e.activeAnimation.empty()) {
                        ImGui::TextDisabled("Anim: %s (%.2fx)%s",
                            e.activeAnimation.c_str(), e.animSpeed,
                            e.blendDuration > 0.0f ? " [Blending]" : "");
                    }
                }
            }

            ImGui::Separator();
            ImGui::TextWrapped("Controls: WASD Move | F Exit/Enter Car | Q Kinetic Blast | X Shockwave | G Weather | T Sky | Space Drift/Jump | Shift Nitro/Sprint | Esc Menu");
        }
        ImGui::End();
    }

    if (actions.onRenderCustomUi) {
        actions.onRenderCustomUi();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace sage
