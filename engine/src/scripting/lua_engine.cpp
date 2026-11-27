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
        worldTbl.set_function("playAnimation", [this](
            const std::string& id,
            sol::optional<std::string> clipName,
            sol::optional<bool> loop,
            sol::optional<float> speed) -> bool {
            return world ? world->playAnimation(id, clipName.value_or(""), loop.value_or(true), speed.value_or(1.0f)) : false;
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

        // --- audio.* namespace (miniaudio 0.11) ---
        sol::table audioTbl = state.create_named_table("audio");
        audioTbl.set_function("setVehicleState", [this](float rpm01, float throttle01, float drift01, float boost01, bool airborne) {
            if (audio) {
                audio->setVehicleState(rpm01, throttle01, drift01, boost01, airborne);
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

        // --- ui.* namespace (Lua-driven Dear ImGui Game HUDs & Menus!) ---
        sol::table uiTbl = state.create_named_table("ui");
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
        uiTbl.set_function("progressBar", [](float fraction, sol::optional<std::string> overlay) {
            ImGui::ProgressBar(
                std::clamp(fraction, 0.0f, 1.0f),
                ImVec2(-1.0f, 0.0f),
                overlay.has_value() ? overlay->c_str() : nullptr);
        });
        uiTbl.set_function("button", [](const std::string& label) -> bool {
            return ImGui::Button(label.c_str());
        });
        uiTbl.set_function("separator", []() {
            ImGui::Separator();
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
