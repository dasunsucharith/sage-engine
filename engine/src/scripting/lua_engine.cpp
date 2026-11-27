#include "sage/scripting/lua_engine.h"

#include "sage/audio/audio_engine.h"
#include "sage/physics/physics_world.h"
#include "sage/platform/input.h"
#include "sage/world/world.h"

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace sage {

namespace fs = std::filesystem;

namespace {

MouseButton mouseButtonFromName(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lower == "right" || lower == "rmb") return MouseButton::Right;
    if (lower == "middle" || lower == "mmb") return MouseButton::Middle;
    return MouseButton::Left;
}

} // namespace

struct LoadedResource {
    std::string name;
    std::string version;
    std::string author;
    fs::path rootDir;
    sol::environment env;
    sol::protected_function onStart;
    sol::protected_function onTick;
    sol::protected_function onRenderUI;
    sol::protected_function onShutdown;
    bool active = false;
};

struct LuaEngine::Impl {
    fs::path resourcesRoot;
    World* world = nullptr;
    PhysicsWorld* physics = nullptr;
    AudioEngine* audio = nullptr;
    const InputState* input = nullptr;
    LuaHostCallbacks callbacks;

    std::unique_ptr<sol::state> lua;
    std::vector<LoadedResource> resources;
    std::unordered_map<std::string, fs::file_time_type> watchedTimestamps;
    std::size_t reloadCount = 0;

    void bindSageApi(sol::state& state) {
        // --- sage.* namespace ---
        sol::table sageTbl = state.create_named_table("sage");
        sageTbl.set_function("log", [](const std::string& message) {
            std::cout << "[Lua] " << message << "\n";
        });
        sageTbl.set_function("getTime", [this]() -> double {
            return callbacks.getTimeSeconds ? callbacks.getTimeSeconds() : 0.0;
        });
        sageTbl.set_function("setTitle", [this](const std::string& title) {
            if (callbacks.setWindowTitle) {
                callbacks.setWindowTitle(title);
            }
        });
        sageTbl.set_function("quit", [this]() {
            if (callbacks.requestQuit) {
                callbacks.requestQuit();
            }
        });
        sageTbl.set_function("saveData", [this](const std::string& key, const std::string& data) -> bool {
            std::string safeKey;
            for (char c : key) {
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
                    safeKey.push_back(c);
                }
            }
            if (safeKey.empty()) return false;
            const fs::path saveDir = resourcesRoot.parent_path() / "saves";
            std::error_code ec;
            fs::create_directories(saveDir, ec);
            std::ofstream out(saveDir / (safeKey + ".dat"), std::ios::trunc);
            if (!out.is_open()) return false;
            out << data;
            return true;
        });
        sageTbl.set_function("loadData", [this](const std::string& key) -> std::string {
            std::string safeKey;
            for (char c : key) {
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
                    safeKey.push_back(c);
                }
            }
            if (safeKey.empty()) return "";
            const fs::path filePath = resourcesRoot.parent_path() / "saves" / (safeKey + ".dat");
            std::ifstream in(filePath);
            if (!in.is_open()) return "";
            return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        });

        // --- input.* namespace ---
        sol::table inputTbl = state.create_named_table("input");
        inputTbl.set_function("isKeyDown", [this](const std::string& keyName) -> bool {
            if (callbacks.isUiCapturingKeyboard && callbacks.isUiCapturingKeyboard()) {
                return false;
            }
            return input ? input->isKeyDown(keyFromName(keyName)) : false;
        });
        inputTbl.set_function("wasKeyPressed", [this](const std::string& keyName) -> bool {
            if (callbacks.isUiCapturingKeyboard && callbacks.isUiCapturingKeyboard()) {
                return false;
            }
            return input ? input->wasKeyPressed(keyFromName(keyName)) : false;
        });
        inputTbl.set_function("wasKeyReleased", [this](const std::string& keyName) -> bool {
            return input ? input->wasKeyReleased(keyFromName(keyName)) : false;
        });
        inputTbl.set_function("isMouseDown", [this](const std::string& btnName) -> bool {
            if (callbacks.isUiCapturingMouse && callbacks.isUiCapturingMouse()) {
                return false;
            }
            return input ? input->isMouseButtonDown(mouseButtonFromName(btnName)) : false;
        });
        inputTbl.set_function("getMouseDelta", [this]() -> std::tuple<float, float> {
            if (!input || (callbacks.isUiCapturingMouse && callbacks.isUiCapturingMouse())) {
                return {0.0f, 0.0f};
            }
            return {input->mouseDeltaX(), input->mouseDeltaY()};
        });

        // --- world.* namespace ---
        sol::table worldTbl = state.create_named_table("world");
        worldTbl.set_function("spawnBox", [this](const std::string& id, float x, float y, float z) -> std::string {
            return world ? world->spawnEntity(id, EntityShape::Box, {x, y, z}) : "";
        });
        worldTbl.set_function("spawnPlane", [this](const std::string& id, float x, float y, float z) -> std::string {
            return world ? world->spawnEntity(id, EntityShape::Plane, {x, y, z}) : "";
        });
        worldTbl.set_function("spawnWater", [this](const std::string& id, float x, float y, float z) -> std::string {
            return world ? world->spawnEntity(id, EntityShape::Water, {x, y, z}) : "";
        });
        worldTbl.set_function("spawnSphere", [this](const std::string& id, float x, float y, float z) -> std::string {
            return world ? world->spawnEntity(id, EntityShape::Sphere, {x, y, z}) : "";
        });
        worldTbl.set_function("spawnTriangle", [this](const std::string& id, float x, float y, float z) -> std::string {
            return world ? world->spawnEntity(id, EntityShape::Triangle, {x, y, z}) : "";
        });
        worldTbl.set_function("spawnRing", [this](const std::string& id, float x, float y, float z) -> std::string {
            return world ? world->spawnEntity(id, EntityShape::Ring, {x, y, z}) : "";
        });
        worldTbl.set_function("spawnModel", [this](const std::string& id, const std::string& path, float x, float y, float z) -> std::string {
            return world ? world->spawnModel(id, path, {x, y, z}) : "";
        });
        worldTbl.set_function("setPosition", [this](const std::string& id, float x, float y, float z) -> bool {
            return world ? world->setPosition(id, x, y, z) : false;
        });
        worldTbl.set_function("getPosition", [this](const std::string& id) -> std::tuple<float, float, float> {
            if (!world) {
                return {0.0f, 0.0f, 0.0f};
            }
            const Vec3 p = world->getPosition(id);
            return {p.x, p.y, p.z};
        });
        worldTbl.set_function("setRotation", [this](const std::string& id, float pitch, float yaw, float roll) -> bool {
            return world ? world->setRotation(id, pitch, yaw, roll) : false;
        });
        worldTbl.set_function("setScale", [this](const std::string& id, float x, float y, float z) -> bool {
            return world ? world->setScale(id, x, y, z) : false;
        });
        worldTbl.set_function("setColor", [this](const std::string& id, float r, float g, float b) -> bool {
            return world ? world->setColor(id, r, g, b) : false;
        });
        worldTbl.set_function("setEmissive", [this](const std::string& id, bool emissive) -> bool {
            return world ? world->setEmissive(id, emissive) : false;
        });
        worldTbl.set_function("setVisible", [this](const std::string& id, bool visible) -> bool {
            return world ? world->setVisible(id, visible) : false;
        });
        worldTbl.set_function("setTimeOfDay", [this](float hours) {
            if (world) {
                world->setTimeOfDay(hours);
            }
        });
        worldTbl.set_function("setBloom", [this](bool enabled, sol::optional<float> intensity) {
            if (world) {
                world->sky().bloomEnabled = enabled;
                if (intensity.has_value()) {
                    world->sky().bloomIntensity = std::clamp(*intensity, 0.0f, 2.0f);
                }
            }
        });
        worldTbl.set_function("setFXAA", [this](bool enabled) {
            if (world) {
                world->sky().fxaaEnabled = enabled;
            }
        });
        worldTbl.set_function("setSSAO", [this](bool enabled) {
            if (world) {
                world->sky().ssaoEnabled = enabled;
            }
        });
        worldTbl.set_function("setGodRays", [this](bool enabled, sol::optional<float> intensity) {
            if (world) {
                world->sky().godRaysEnabled = enabled;
                if (intensity.has_value()) {
                    world->sky().godRayIntensity = std::clamp(*intensity, 0.0f, 1.5f);
                }
            }
        });
        worldTbl.set_function("setCloudShadows", [this](bool enabled) {
            if (world) {
                world->sky().cloudShadowsEnabled = enabled;
            }
        });
        worldTbl.set_function("setTerrainEnabled", [this](bool enabled) {
            if (world) {
                world->terrain().enabled = enabled;
            }
        });
        worldTbl.set_function("setSSR", [this](bool enabled, sol::optional<float> intensity) {
            if (world) {
                world->sky().ssrEnabled = enabled;
                if (intensity.has_value()) {
                    world->sky().ssrIntensity = std::clamp(*intensity, 0.0f, 1.5f);
                }
            }
        });
        worldTbl.set_function("setGrassEnabled", [this](bool enabled) {
            if (world) {
                world->sky().grassEnabled = enabled;
            }
        });
        worldTbl.set_function("setGrassInteractPos", [this](float x, float y, float z) {
            if (world) {
                world->sky().grassInteractPos = {x, y, z};
            }
        });
        worldTbl.set_function("sampleTerrain", [this](float x, float z)
            -> std::tuple<float, float, float, float, int> {
            if (!world) {
                return {0.0f, 0.0f, 1.0f, 0.0f, 0};
            }
            const TerrainSample s = world->sampleTerrain(x, z);
            return {s.height, s.normal.x, s.normal.y, s.normal.z, s.biomeId};
        });
        worldTbl.set_function("setWeather", [this](float rainIntensity, sol::optional<float> wetness) {
            if (world) {
                const float r = std::clamp(rainIntensity, 0.0f, 1.0f);
                world->sky().rainIntensity = r;
                world->sky().wetness = std::clamp(wetness.value_or(r), 0.0f, 1.0f);
            }
        });
        worldTbl.set_function("setPointLight", [this](
            const std::string& id, float x, float y, float z, float r, float g, float b,
            sol::optional<float> radius, sol::optional<float> intensity) {
            if (world) {
                world->setPointLight(id, {x, y, z}, {r, g, b}, radius.value_or(18.0f), intensity.value_or(2.0f));
            }
        });
        worldTbl.set_function("removePointLight", [this](const std::string& id) -> bool {
            return world ? world->removePointLight(id) : false;
        });
        worldTbl.set_function("spawnParticles", [this](
            float x, float y, float z, float vx, float vy, float vz, float r, float g, float b, int count,
            sol::optional<float> size, sol::optional<float> life, sol::optional<bool> emissive, sol::optional<float> gravity) {
            if (world) {
                world->spawnParticles(
                    {x, y, z}, {vx, vy, vz}, {r, g, b}, count,
                    size.value_or(0.25f), life.value_or(0.65f), emissive.value_or(false), gravity.value_or(14.0f));
            }
        });
        worldTbl.set_function("sampleWater", [this](float x, float z)
            -> std::tuple<bool, float, float, float, float> {
            if (!world) {
                return {false, -10.0f, 0.0f, 0.0f, 0.0f};
            }
            const float t = callbacks.getTimeSeconds ? static_cast<float>(callbacks.getTimeSeconds()) : 0.0f;
            const WaterSampleResult w = world->sampleWater(x, z, t);
            return {w.inWater, w.surfaceY, w.seabedY, w.depth, w.shoreDist};
        });
        worldTbl.set_function("playAnimation", [this](
            const std::string& id,
            sol::optional<std::string> clipName,
            sol::optional<bool> loop,
            sol::optional<float> speed,
            sol::optional<float> blendSeconds) -> bool {
            return world ? world->playAnimation(
                id,
                clipName.value_or(""),
                loop.value_or(true),
                speed.value_or(1.0f),
                blendSeconds.value_or(0.18f)) : false;
        });
        worldTbl.set_function("stopAnimation", [this](const std::string& id) -> bool {
            return world ? world->stopAnimation(id) : false;
        });
        worldTbl.set_function("setAnimationSpeed", [this](const std::string& id, float speed) -> bool {
            return world ? world->setAnimationSpeed(id, speed) : false;
        });
        worldTbl.set_function("destroy", [this](const std::string& id) -> bool {
            return world ? world->destroy(id) : false;
        });
        worldTbl.set_function("clear", [this]() {
            if (world) {
                world->clear();
            }
        });

        // --- camera.* namespace ---
        sol::table cameraTbl = state.create_named_table("camera");
        cameraTbl.set_function("setPosition", [this](float x, float y, float z) {
            if (world) {
                world->camera().position = {x, y, z};
            }
        });
        cameraTbl.set_function("setTarget", sol::overload(
            [this](float x, float y, float z) {
                if (world) {
                    world->camera().target = {x, y, z};
                }
            },
            [this](float x, float z) {
                if (world) {
                    world->camera().target = {x, 0.0f, z};
                }
            }));
        cameraTbl.set_function("setFov", [this](float fovDegrees) {
            if (world) {
                world->camera().fovDegrees = std::clamp(fovDegrees, 25.0f, 110.0f);
            }
        });

        // --- physics.* namespace (Jolt Physics 5.7) ---
        sol::table physTbl = state.create_named_table("physics");
        physTbl.set_function("addStaticBox", [this](const std::string& id, float px, float py, float pz, float hx, float hy, float hz) -> bool {
            return physics ? physics->addStaticBox(id, {px, py, pz}, {hx, hy, hz}) : false;
        });
        physTbl.set_function("addStaticDeck", [this](
            const std::string& id, float px, float py, float pz, float hx, float hy, float hz, sol::optional<float> yawDeg) -> bool {
            return physics ? physics->addStaticDeck(id, {px, py, pz}, {hx, hy, hz}, yawDeg.value_or(0.0f)) : false;
        });
        physTbl.set_function("addStaticCylinder", [this](const std::string& id, float px, float py, float pz, float radius, float height) -> bool {
            return physics ? physics->addStaticCylinder(id, {px, py, pz}, radius, height) : false;
        });
        physTbl.set_function("addStaticRamp", [this](
            const std::string& id, float px, float py, float pz, float w, float h, float l, float yawDeg) -> bool {
            return physics ? physics->addStaticRamp(id, {px, py, pz}, w, h, l, yawDeg) : false;
        });
        physTbl.set_function("addDynamicBox", [this](
            const std::string& id, float px, float py, float pz, float hx, float hy, float hz,
            sol::optional<float> mass, sol::optional<float> restitution, sol::optional<float> friction) -> bool {
            return physics ? physics->addDynamicBox(
                id, {px, py, pz}, {hx, hy, hz},
                mass.value_or(25.0f), restitution.value_or(0.28f), friction.value_or(0.65f)) : false;
        });
        physTbl.set_function("addDynamicSphere", [this](
            const std::string& id, float px, float py, float pz, float radius,
            sol::optional<float> mass, sol::optional<float> restitution, sol::optional<float> friction) -> bool {
            return physics ? physics->addDynamicSphere(
                id, {px, py, pz}, radius,
                mass.value_or(30.0f), restitution.value_or(0.55f), friction.value_or(0.45f)) : false;
        });
        physTbl.set_function("addDynamicCapsule", [this](
            const std::string& id, float px, float py, float pz, float radius, float halfHeight,
            sol::optional<float> mass, sol::optional<float> restitution, sol::optional<float> friction) -> bool {
            return physics ? physics->addDynamicCapsule(
                id, {px, py, pz}, radius, halfHeight,
                mass.value_or(10.0f), restitution.value_or(0.25f), friction.value_or(0.65f)) : false;
        });
        physTbl.set_function("addKinematicBox", [this](const std::string& id, float px, float py, float pz, float hx, float hy, float hz) -> bool {
            return physics ? physics->addKinematicBox(id, {px, py, pz}, {hx, hy, hz}) : false;
        });
        physTbl.set_function("moveKinematic", [this](
            const std::string& id, float px, float py, float pz, float pitch, float yaw, float roll, float dt) -> bool {
            return physics ? physics->moveKinematic(id, {px, py, pz}, {pitch, yaw, roll}, dt) : false;
        });
        physTbl.set_function("applyImpulse", [this](const std::string& id, float ix, float iy, float iz) -> bool {
            return physics ? physics->applyImpulse(id, {ix, iy, iz}) : false;
        });
        physTbl.set_function("applyExplosion", [this](float cx, float cy, float cz, float radius, float strength) -> int {
            return physics ? physics->applyExplosion({cx, cy, cz}, radius, strength) : 0;
        });
        physTbl.set_function("getSpeed", [this](const std::string& id) -> float {
            return physics ? physics->getLinearSpeed(id) : 0.0f;
        });
        physTbl.set_function("setGravity", [this](float gx, float gy, float gz) {
            if (physics) {
                physics->setGravity({gx, gy, gz});
            }
        });
        physTbl.set_function("raycast", [this](float ox, float oy, float oz, float dx, float dy, float dz, float maxDist)
            -> std::tuple<bool, float, float, float, std::string> {
            if (!physics) {
                return {false, 0.0f, 0.0f, 0.0f, ""};
            }
            const RaycastHit hit = physics->raycast({ox, oy, oz}, {dx, dy, dz}, maxDist);
            return {hit.hit, hit.position.x, hit.position.y, hit.position.z, hit.entityId};
        });
        physTbl.set_function("resolveCircleXZ", [this](
            float x, float y, float z, float radius, float vx, float vz, sol::optional<float> bounce)
            -> std::tuple<bool, float, float, float, float, std::string> {
            if (!physics) {
                return {false, x, z, vx, vz, ""};
            }
            const CircleCollisionResult r = physics->resolveCircleXZ(x, y, z, radius, vx, vz, bounce.value_or(0.32f));
            return {r.hit, r.x, r.z, r.vx, r.vz, r.hitEntityId};
        });
        physTbl.set_function("sampleRampSurface", [this](float x, float z)
            -> std::tuple<bool, float, float, float> {
            if (!physics) {
                return {false, 0.0f, 0.0f, 0.0f};
            }
            const RampSampleResult r = physics->sampleRampSurface(x, z);
            return {r.onRamp, r.groundY, r.rampHeight, r.rampYawDeg};
        });

        // --- audio.* namespace (miniaudio 0.11) ---
        sol::table audioTbl = state.create_named_table("audio");
        audioTbl.set_function("setVehicleProfile", [this](int profile) {
            if (audio) {
                audio->setVehicleProfile(profile);
            }
        });
        audioTbl.set_function("setVehicleState", [this](float rpm01, float throttle01, float drift01, float boost01, bool airborne) {
            if (audio) {
                audio->setVehicleState(rpm01, throttle01, drift01, boost01, airborne);
            }
        });
        audioTbl.set_function("playShiftPop", [this](sol::optional<float> intensity) {
            if (audio) {
                audio->playShiftPop(intensity.value_or(0.75f));
            }
        });
        audioTbl.set_function("playFootstep", [this](sol::optional<int> surface, sol::optional<float> intensity) {
            if (audio) {
                audio->playFootstep(surface.value_or(0), intensity.value_or(0.65f));
            }
        });
        audioTbl.set_function("playImpact", [this](sol::optional<float> intensity) {
            if (audio) {
                audio->playImpact(intensity.value_or(0.8f));
            }
        });
        audioTbl.set_function("playChime", [this]() {
            if (audio) {
                audio->playChime();
            }
        });
        audioTbl.set_function("playExplosion", [this]() {
            if (audio) {
                audio->playExplosion();
            }
        });
        audioTbl.set_function("setVolume", [this](float vol) {
            if (audio) {
                audio->setMasterVolume(vol);
            }
        });
        audioTbl.set_function("setListener", [this](float x, float y, float z, float fwdX, float fwdZ) {
            if (audio) {
                audio->setListener(x, y, z, fwdX, fwdZ);
            }
        });
        audioTbl.set_function("setSpatialEmitter", [this](
            int slot, int type, float x, float y, float z, sol::optional<float> radius, sol::optional<float> gain) {
            if (audio) {
                audio->setSpatialEmitter(slot, type, x, y, z, radius.value_or(65.0f), gain.value_or(0.65f));
            }
        });

        // --- ui.* namespace (Lua-driven Dear ImGui Game HUDs & Fullscreen Menus!) ---
        sol::table uiTbl = state.create_named_table("ui");
        uiTbl.set_function("getViewportSize", []() -> std::tuple<float, float> {
            const ImVec2 sz = ImGui::GetIO().DisplaySize;
            return {sz.x, sz.y};
        });
        uiTbl.set_function("beginCenteredWindow", [](const std::string& title, float w, float h) -> bool {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
            constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
            return ImGui::Begin(title.c_str(), nullptr, flags);
        });
        uiTbl.set_function("beginWindow", [](
            const std::string& title, float x, float y, float w, float h, sol::optional<bool> overlay) -> bool {
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
            if (w > 0.0f || h > 0.0f) {
                ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
            }
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;
            if (overlay.value_or(true)) {
                flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
            }
            return ImGui::Begin(title.c_str(), nullptr, flags);
        });
        uiTbl.set_function("endWindow", []() {
            ImGui::End();
        });
        uiTbl.set_function("text", [](const std::string& msg) {
            ImGui::TextUnformatted(msg.c_str());
        });
        uiTbl.set_function("textCentered", [](const std::string& msg) {
            const float winW = ImGui::GetWindowSize().x;
            const float txtW = ImGui::CalcTextSize(msg.c_str()).x;
            ImGui::SetCursorPosX(std::max((winW - txtW) * 0.5f, 8.0f));
            ImGui::TextUnformatted(msg.c_str());
        });
        uiTbl.set_function("setFontScale", [](float scale) {
            ImGui::SetWindowFontScale(std::clamp(scale, 0.5f, 3.0f));
        });
        uiTbl.set_function("progressBar", [](float fraction, sol::optional<std::string> overlay) {
            ImGui::ProgressBar(
                std::clamp(fraction, 0.0f, 1.0f),
                ImVec2(-1.0f, 0.0f),
                overlay.has_value() ? overlay->c_str() : nullptr);
        });
        uiTbl.set_function("button", [](const std::string& label, sol::optional<float> w, sol::optional<float> h) -> bool {
            return ImGui::Button(label.c_str(), ImVec2(w.value_or(0.0f), h.value_or(0.0f)));
        });
        uiTbl.set_function("sameLine", []() {
            ImGui::SameLine();
        });
        uiTbl.set_function("spacing", []() {
            ImGui::Spacing();
        });
        uiTbl.set_function("separator", []() {
            ImGui::Separator();
        });
        // 2D Canvas Vector Drawing inside any ui.beginWindow() — ideal for Minimaps, Radars & Crosshairs!
        uiTbl.set_function("drawRect", [](
            float x, float y, float w, float h, float r, float g, float b, float a,
            sol::optional<bool> filled, sol::optional<float> rounding) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 winPos = ImGui::GetWindowPos();
            const ImVec2 p0(winPos.x + x, winPos.y + y);
            const ImVec2 p1(p0.x + w, p0.y + h);
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
            if (filled.value_or(true)) {
                dl->AddRectFilled(p0, p1, col, rounding.value_or(0.0f));
            } else {
                dl->AddRect(p0, p1, col, rounding.value_or(0.0f), 0, 1.5f);
            }
        });
        uiTbl.set_function("drawCircle", [](
            float cx, float cy, float radius, float r, float g, float b, float a, sol::optional<bool> filled) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 winPos = ImGui::GetWindowPos();
            const ImVec2 center(winPos.x + cx, winPos.y + cy);
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
            if (filled.value_or(true)) {
                dl->AddCircleFilled(center, radius, col);
            } else {
                dl->AddCircle(center, radius, col, 0, 1.5f);
            }
        });
        uiTbl.set_function("drawLine", [](
            float x1, float y1, float x2, float y2, float r, float g, float b, float a, sol::optional<float> thickness) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 winPos = ImGui::GetWindowPos();
            const ImVec2 p0(winPos.x + x1, winPos.y + y1);
            const ImVec2 p1(winPos.x + x2, winPos.y + y2);
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
            dl->AddLine(p0, p1, col, thickness.value_or(1.5f));
        });
    }

    void snapshotWatchedFiles() {
        watchedTimestamps.clear();
        if (!fs::exists(resourcesRoot)) {
            return;
        }
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(resourcesRoot, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            const auto fname = entry.path().filename().string();
            if (ext == ".lua" || fname == "manifest.json") {
                const auto writeTime = fs::last_write_time(entry.path(), ec);
                if (!ec) {
                    watchedTimestamps[entry.path().string()] = writeTime;
                }
            }
        }
    }

    bool hasWatchedFilesChanged(std::string& changedPathOut) const {
        if (!fs::exists(resourcesRoot)) {
            return false;
        }
        std::error_code ec;
        std::size_t currentFileCount = 0;
        for (const auto& entry : fs::recursive_directory_iterator(resourcesRoot, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            const auto fname = entry.path().filename().string();
            if (ext != ".lua" && fname != "manifest.json") {
                continue;
            }
            ++currentFileCount;
            const std::string key = entry.path().string();
            const auto writeTime = fs::last_write_time(entry.path(), ec);
            if (ec) {
                continue;
            }
            const auto it = watchedTimestamps.find(key);
            if (it == watchedTimestamps.end() || it->second != writeTime) {
                changedPathOut = key;
                return true;
            }
        }
        if (currentFileCount != watchedTimestamps.size()) {
            changedPathOut = resourcesRoot.string();
            return true;
        }
        return false;
    }

    void callShutdownHooks() {
        for (auto& res : resources) {
            if (res.active && res.onShutdown.valid()) {
                sol::protected_function_result result = res.onShutdown();
                if (!result.valid()) {
                    const sol::error err = result;
                    std::cerr << "[SAGE][Lua] Error in " << res.name << "::onShutdown(): "
                              << err.what() << "\n";
                }
            }
        }
        resources.clear();
    }

    bool loadSingleResource(const fs::path& resourceDir) {
        const fs::path manifestPath = resourceDir / "manifest.json";
        if (!fs::exists(manifestPath)) {
            return false;
        }

        std::ifstream manifestFile(manifestPath);
        if (!manifestFile.is_open()) {
            std::cerr << "[SAGE][Lua] Could not open manifest: " << manifestPath.string() << "\n";
            return false;
        }

        nlohmann::json manifest;
        try {
            manifestFile >> manifest;
        } catch (const nlohmann::json::exception& ex) {
            std::cerr << "[SAGE][Lua] Invalid JSON in " << manifestPath.string()
                      << ": " << ex.what() << "\n";
            return false;
        }

        const bool enabled = manifest.value("enabled", true);
        const std::string name = manifest.value("name", resourceDir.filename().string());
        if (!enabled) {
            std::cout << "[SAGE][Lua] Skipping disabled resource: " << name << "\n";
            return false;
        }

        LoadedResource res;
        res.name = name;
        res.version = manifest.value("version", "0.0.0");
        res.author = manifest.value("author", "Unknown");
        res.rootDir = resourceDir;
        res.env = sol::environment(*lua, sol::create, lua->globals());

        if (!manifest.contains("scripts") || !manifest["scripts"].is_array()) {
            std::cerr << "[SAGE][Lua] Manifest " << manifestPath.string()
                      << " is missing a \"scripts\" array.\n";
            return false;
        }

        for (const auto& scriptItem : manifest["scripts"]) {
            if (!scriptItem.is_string()) {
                continue;
            }
            const fs::path scriptPath = resourceDir / scriptItem.get<std::string>();
            if (!fs::exists(scriptPath)) {
                std::cerr << "[SAGE][Lua] Script not found in resource \"" << res.name
                          << "\": " << scriptPath.string() << "\n";
                return false;
            }

            sol::protected_function_result execResult = lua->safe_script_file(
                scriptPath.string(),
                res.env,
                sol::script_pass_on_error);

            if (!execResult.valid()) {
                const sol::error err = execResult;
                std::cerr << "[SAGE][Lua] Failed to load script " << scriptPath.string()
                          << ":\n  " << err.what() << "\n";
                return false;
            }
        }

        res.onStart = res.env["onStart"];
        res.onTick = res.env["onTick"];
        res.onRenderUI = res.env["onRenderUI"];
        res.onShutdown = res.env["onShutdown"];
        res.active = true;

        std::cout << "[SAGE][Lua] Loaded resource \"" << res.name << "\" v" << res.version
                  << " by " << res.author << "\n";

        if (res.onStart.valid()) {
            sol::protected_function_result startResult = res.onStart();
            if (!startResult.valid()) {
                const sol::error err = startResult;
                std::cerr << "[SAGE][Lua] Runtime error in " << res.name << "::onStart():\n  "
                          << err.what() << "\n";
                res.active = false;
                return false;
            }
        }

        resources.push_back(std::move(res));
        return true;
    }
};

LuaEngine::LuaEngine()
    : m_impl(std::make_unique<Impl>()) {}

LuaEngine::~LuaEngine() {
    shutdown();
}

bool LuaEngine::initialize(
    const fs::path& resourcesRoot,
    World& world,
    PhysicsWorld& physics,
    AudioEngine& audio,
    const InputState& input,
    LuaHostCallbacks callbacks) {
    m_impl->resourcesRoot = resourcesRoot;
    m_impl->world = &world;
    m_impl->physics = &physics;
    m_impl->audio = &audio;
    m_impl->input = &input;
    m_impl->callbacks = std::move(callbacks);

    return reloadAll();
}

bool LuaEngine::reloadAll() {
    m_impl->callShutdownHooks();
    if (m_impl->physics) {
        m_impl->physics->clearBodies();
    }
    if (m_impl->world) {
        m_impl->world->clear();
    }

    if (m_impl->callbacks.beforeResourcesStart) {
        m_impl->callbacks.beforeResourcesStart();
    }

    m_impl->lua = std::make_unique<sol::state>();
    m_impl->lua->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::utf8);

    m_impl->bindSageApi(*m_impl->lua);

    if (!fs::exists(m_impl->resourcesRoot)) {
        std::cerr << "[SAGE][Lua] Resources directory not found: "
                  << m_impl->resourcesRoot.string() << "\n";
        m_impl->snapshotWatchedFiles();
        return false;
    }

    std::vector<fs::path> resourceDirs;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_impl->resourcesRoot, ec)) {
        if (entry.is_directory(ec) && fs::exists(entry.path() / "manifest.json", ec)) {
            resourceDirs.push_back(entry.path());
        }
    }
    std::sort(resourceDirs.begin(), resourceDirs.end());

    for (const auto& dir : resourceDirs) {
        m_impl->loadSingleResource(dir);
    }

    m_impl->snapshotWatchedFiles();
    ++m_impl->reloadCount;

    std::cout << "[SAGE][Lua] Active resources: " << m_impl->resources.size()
              << " | Entities: " << (m_impl->world ? m_impl->world->entityCount() : 0)
              << " | Jolt bodies: " << (m_impl->physics ? m_impl->physics->bodyCount() : 0)
              << "\n";
    return true;
}

void LuaEngine::tick(double dt) {
    for (auto& res : m_impl->resources) {
        if (!res.active || !res.onTick.valid()) {
            continue;
        }
        sol::protected_function_result result = res.onTick(dt);
        if (!result.valid()) {
            const sol::error err = result;
            std::cerr << "[SAGE][Lua] Runtime error in " << res.name << "::onTick(dt):\n  "
                      << err.what() << "\n"
                      << "[SAGE][Lua] Resource \"" << res.name
                      << "\" paused — fix the script and save (or press F5) to recover live!\n";
            res.active = false;
        }
    }
}

void LuaEngine::renderUI() {
    for (auto& res : m_impl->resources) {
        if (!res.active || !res.onRenderUI.valid()) {
            continue;
        }
        sol::protected_function_result result = res.onRenderUI();
        if (!result.valid()) {
            const sol::error err = result;
            std::cerr << "[SAGE][Lua] Runtime error in " << res.name << "::onRenderUI():\n  "
                      << err.what() << "\n";
            res.active = false;
        }
    }
}

bool LuaEngine::checkHotReload() {
    std::string changedPath;
    if (m_impl->hasWatchedFilesChanged(changedPath)) {
        std::cout << "\n[SAGE][HotReload] Change detected in: " << changedPath << "\n";
        reloadAll();
        return true;
    }
    return false;
}

void LuaEngine::shutdown() {
    if (!m_impl) {
        return;
    }
    m_impl->callShutdownHooks();
    m_impl->lua.reset();
}

std::size_t LuaEngine::activeResourceCount() const {
    return m_impl->resources.size();
}

std::size_t LuaEngine::reloadCount() const {
    return m_impl->reloadCount;
}

const fs::path& LuaEngine::resourcesRoot() const {
    return m_impl->resourcesRoot;
}

} // namespace sage
