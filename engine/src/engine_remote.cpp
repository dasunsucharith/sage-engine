// Remote control: the engine side of the MCP bridge (tools/sage_mcp/server.py).
//
// Requests arrive from RemoteServer (localhost TCP, one JSON object per line) and are
// executed here, on the main thread, between frames. Protocol:
//   request : {"id": 1, "method": "list_entities", "params": {...}}
//   response: {"id": 1, "ok": true, "result": {...}}   or   {"id": 1, "ok": false, "error": "..."}
// Every method is documented in docs/mcp.md.

#include "sage/engine.h"
#include "sage/version.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace sage {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

// A clear, user-facing error for bad parameters (becomes {"ok": false, "error": ...}).
struct RemoteError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

Vec3 vec3Param(const json& params, const char* key, Vec3 fallback, bool required = false) {
    if (!params.contains(key)) {
        if (required) throw RemoteError(std::string("missing parameter '") + key + "' ([x, y, z])");
        return fallback;
    }
    const json& v = params[key];
    if (!v.is_array() || v.size() != 3 || !v[0].is_number() || !v[1].is_number() || !v[2].is_number()) {
        throw RemoteError(std::string("parameter '") + key + "' must be [x, y, z] numbers");
    }
    return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
}

std::string stringParam(const json& params, const char* key, bool required = true, const std::string& fallback = "") {
    if (!params.contains(key)) {
        if (required) throw RemoteError(std::string("missing parameter '") + key + "'");
        return fallback;
    }
    if (!params[key].is_string()) throw RemoteError(std::string("parameter '") + key + "' must be a string");
    return params[key].get<std::string>();
}

json vec3Json(const Vec3& v) { return json::array({v.x, v.y, v.z}); }

const char* shapeName(EntityShape s) {
    switch (s) {
    case EntityShape::Box: return "box";
    case EntityShape::Plane: return "plane";
    case EntityShape::Water: return "water";
    case EntityShape::Sphere: return "sphere";
    case EntityShape::Triangle: return "triangle";
    case EntityShape::Ring: return "ring";
    case EntityShape::Model: return "model";
    }
    return "unknown";
}

EntityShape shapeFromName(const std::string& name) {
    if (name == "box") return EntityShape::Box;
    if (name == "plane") return EntityShape::Plane;
    if (name == "water") return EntityShape::Water;
    if (name == "sphere") return EntityShape::Sphere;
    if (name == "triangle") return EntityShape::Triangle;
    if (name == "ring") return EntityShape::Ring;
    throw RemoteError("unknown shape '" + name + "' (use box, plane, water, sphere, triangle or ring, or pass 'model')");
}

json entitySummary(const Entity& e) {
    json j = {{"id", e.id}, {"shape", shapeName(e.shape)}, {"position", vec3Json(e.position)}, {"visible", e.visible}};
    if (e.shape == EntityShape::Model) j["model"] = e.modelPath;
    return j;
}

json timingsJson(const std::vector<TimingSample>& samples) {
    json j = json::object();
    for (const TimingSample& t : samples) j[t.name] = t.ms;
    return j;
}

} // namespace

void Engine::processRemoteCommands() {
    if (!m_remote.isRunning()) return;
    std::vector<RemoteMessage> messages;
    m_remote.poll(messages);
    for (const RemoteMessage& msg : messages) {
        m_remote.send(msg.clientId, handleRemoteRequest(msg.line));
    }
}

std::string Engine::handleRemoteRequest(const std::string& line) {
    json response;
    json request = json::parse(line, nullptr, false);
    if (request.is_discarded() || !request.is_object()) {
        return json{{"id", nullptr}, {"ok", false}, {"error", "request is not a JSON object"}}.dump();
    }
    response["id"] = request.contains("id") ? request["id"] : json(nullptr);
    const std::string method = request.value("method", "");
    const json params = request.contains("params") && request["params"].is_object() ? request["params"] : json::object();

    try {
        json result;
        if (method == "info") {
            result = {
                {"engineVersion", kEngineVersion},
                {"buildConfig", buildConfigName()},
                {"scene", m_resolvedScenePath.string()},
                {"projectRoot", m_projectRoot.string()},
                {"luaResources", m_lua.activeResourceCount()},
                {"entities", m_world.entityCount()},
                {"pointLights", m_world.pointLights().size()},
                {"physicsBodies", m_physics.bodyCount()},
                {"fps", m_smoothedFps},
                {"simTimeSeconds", m_simTime},
                {"freeFlyCamera", m_freeFlyMode},
                {"window", {m_window.width(), m_window.height()}},
            };
        } else if (method == "list_entities") {
            const std::string prefix = stringParam(params, "prefix", false);
            const std::size_t limit = std::clamp<std::size_t>(params.value("limit", 200), 1, 5000);
            json list = json::array();
            std::size_t matched = 0;
            for (const Entity& e : m_world.entities()) {
                if (!prefix.empty() && e.id.rfind(prefix, 0) != 0) continue;
                ++matched;
                if (list.size() < limit) list.push_back(entitySummary(e));
            }
            result = {{"total", matched}, {"returned", list.size()}, {"entities", list}};
        } else if (method == "get_entity") {
            const std::string id = stringParam(params, "id");
            const Entity* e = m_world.findEntity(id);
            if (!e) throw RemoteError("no entity named '" + id + "' (use list_entities to see names)");
            result = entitySummary(*e);
            result["rotationDegrees"] = vec3Json(e->rotation);
            if (e->useQuaternion) {
                result["rotationQuat"] = json::array({e->rotationQuat.x, e->rotationQuat.y, e->rotationQuat.z, e->rotationQuat.w});
            }
            result["scale"] = vec3Json(e->scale);
            result["color"] = json::array({e->color.r, e->color.g, e->color.b});
            result["emissive"] = e->emissive;
            if (e->shape == EntityShape::Model) {
                result["animation"] = {{"clip", e->activeAnimation}, {"time", e->animTime}, {"speed", e->animSpeed},
                                       {"playing", e->animPlaying}, {"loop", e->animLoop}};
            }
            if (const LookAtComponent* look = m_world.lookAt(e->handle)) {
                result["lookAt"] = {{"target", vec3Json(look->target)}, {"weight", look->weight}};
            }
            if (const RagdollComponent* rag = m_world.ragdoll(e->handle)) {
                result["ragdoll"] = {{"active", rag->active}, {"blend", rag->blend}};
            }
            result["groundHeightBelow"] = m_world.sampleTerrain(e->position.x, e->position.z).height;
        } else if (method == "spawn") {
            const std::string id = stringParam(params, "id");
            Vec3 pos = vec3Param(params, "position", {}, true);
            if (params.value("snapToGround", false)) {
                pos.y += m_world.sampleTerrain(pos.x, pos.z).height; // y becomes "height above the ground"
            }
            const Vec3 scale = vec3Param(params, "scale", {1, 1, 1});
            if (params.contains("model")) {
                m_world.spawnModel(id, stringParam(params, "model"), pos, scale);
            } else {
                const Vec3 c = vec3Param(params, "color", {0.8f, 0.8f, 0.8f});
                m_world.spawnEntity(id, shapeFromName(stringParam(params, "shape", false, "box")), pos, scale, {c.x, c.y, c.z});
            }
            const Vec3 rot = vec3Param(params, "rotation", {});
            m_world.setRotation(id, rot.x, rot.y, rot.z);
            m_world.teleport(id, pos.x, pos.y, pos.z); // Appear in place (no interpolation slide)
            result = entitySummary(*m_world.findEntity(id));
        } else if (method == "set_transform") {
            const std::string id = stringParam(params, "id");
            Entity* e = m_world.findEntity(id);
            if (!e) throw RemoteError("no entity named '" + id + "'");
            if (params.contains("position")) {
                const Vec3 p = vec3Param(params, "position", {});
                if (params.value("teleport", true)) {
                    m_world.teleport(id, p.x, p.y, p.z);
                } else {
                    m_world.setPosition(id, p.x, p.y, p.z);
                }
            }
            if (params.contains("rotation")) {
                const Vec3 r = vec3Param(params, "rotation", {});
                m_world.setRotation(id, r.x, r.y, r.z);
            }
            if (params.contains("scale")) {
                const Vec3 s = vec3Param(params, "scale", {});
                m_world.setScale(id, s.x, s.y, s.z);
            }
            result = entitySummary(*m_world.findEntity(id));
            result["note"] = "If a Lua script also moves this entity every tick, the script will override this.";
        } else if (method == "set_visible") {
            const std::string id = stringParam(params, "id");
            if (!params.contains("visible") || !params["visible"].is_boolean()) throw RemoteError("missing boolean 'visible'");
            if (!m_world.setVisible(id, params["visible"].get<bool>())) throw RemoteError("no entity named '" + id + "'");
            result = {{"id", id}, {"visible", params["visible"]}};
        } else if (method == "destroy") {
            const std::string id = stringParam(params, "id");
            if (!m_world.destroy(id)) throw RemoteError("no entity named '" + id + "'");
            result = {{"destroyed", id}};
        } else if (method == "get_camera") {
            const CameraState& c = m_world.camera();
            result = {{"position", vec3Json(c.position)}, {"target", vec3Json(c.target)}, {"fov", c.fovDegrees}};
        } else if (method == "set_camera") {
            CameraState& c = m_world.camera();
            c.position = vec3Param(params, "position", c.position);
            c.target = vec3Param(params, "target", c.target);
            c.fovDegrees = std::clamp(params.value("fov", c.fovDegrees), 25.0f, 110.0f);
            m_world.snapCamera();
            result = {{"position", vec3Json(c.position)}, {"target", vec3Json(c.target)}, {"fov", c.fovDegrees},
                      {"note", "Scripts that drive the camera every tick will override this. For a one-off view, pass 'camera' to screenshot instead."}};
        } else if (method == "screenshot") {
            const int maxWidth = std::clamp(params.value("maxWidth", 1024), 0, 8192);
            const std::string format = stringParam(params, "format", false, "jpg");
            if (format != "jpg" && format != "png") throw RemoteError("format must be 'jpg' or 'png'");
            const fs::path dir = fs::temp_directory_path() / "sage_remote";
            const fs::path path = dir / ("shot_" + std::to_string(++m_remoteShotCounter) + "." + format);

            // Optional one-off camera: render from there without disturbing the game camera.
            const CameraState saved = m_world.camera();
            if (params.contains("camera") && params["camera"].is_object()) {
                const json& cam = params["camera"];
                CameraState& c = m_world.camera();
                c.position = vec3Param(cam, "position", c.position);
                c.target = vec3Param(cam, "target", c.target);
                c.fovDegrees = std::clamp(cam.value("fov", c.fovDegrees), 25.0f, 110.0f);
                m_remoteCameraOverride = true;
            }
            renderWorldInterpolated({0.05f, 0.07f, 0.11f, 1.0f}, m_renderTime); // 3D world only, no UI overlay
            const bool ok = m_renderer->captureScreenshotScaled(path, maxWidth);
            m_world.camera() = saved;
            m_remoteCameraOverride = false;
            if (!ok) throw RemoteError("screenshot capture failed");
            result = {{"path", path.string()}, {"format", format}};
        } else if (method == "get_logs") {
            const std::uint64_t after = params.value("after", static_cast<std::uint64_t>(0));
            const std::size_t limit = std::clamp<std::size_t>(params.value("limit", 200), 1, 2000);
            const bool errorsOnly = params.value("errorsOnly", false);
            json lines = json::array();
            for (const LogLine& l : m_logs.linesAfter(after, errorsOnly ? 2000 : limit)) {
                if (errorsOnly && !l.isError) continue;
                lines.push_back({{"seq", l.seq}, {"error", l.isError}, {"text", l.text}});
            }
            while (lines.size() > limit) lines.erase(lines.begin());
            result = {{"lines", lines}, {"lastSeq", m_logs.lastSeq()}};
        } else if (method == "get_performance") {
            result = {
                {"fps", m_smoothedFps},
                {"frameMs", m_lastFrameMs},
                {"cpuSectionsMs", timingsJson(m_cpuProfiler.last())},
                {"gpuPassesMs", timingsJson(m_renderer->lastGpuPassTimings())},
                {"drawCalls", m_renderer->lastDrawCallCount()},
                {"culledEntities", m_renderer->lastCulledCount()},
                {"entities", m_world.entityCount()},
                {"physicsBodies", m_physics.bodyCount()},
                {"note", "Single-frame snapshot. For reliable comparisons use 'sage.exe --benchmark' (see AGENTS.md)."},
            };
        } else if (method == "run_lua") {
            std::string output;
            const bool ok = m_lua.runSnippet(stringParam(params, "code"), output);
            result = {{"ok", ok}, {"output", output}};
        } else if (method == "reload") {
            m_scriptTitle.clear();
            m_lua.reloadAll();
            if (m_renderer) m_renderer->preloadModels(m_world);
            result = {{"reloaded", true}, {"entities", m_world.entityCount()}, {"luaResources", m_lua.activeResourceCount()}};
        } else if (method == "ping") {
            result = {{"pong", true}};
        } else {
            throw RemoteError("unknown method '" + method + "'. Methods: info, list_entities, get_entity, spawn, set_transform, "
                              "set_visible, destroy, get_camera, set_camera, screenshot, get_logs, get_performance, run_lua, reload, ping");
        }
        response["ok"] = true;
        response["result"] = std::move(result);
    } catch (const RemoteError& e) {
        response["ok"] = false;
        response["error"] = e.what();
    } catch (const json::exception& e) {
        response["ok"] = false;
        response["error"] = std::string("bad parameter: ") + e.what();
    }
    return response.dump(-1, ' ', false, json::error_handler_t::replace);
}

} // namespace sage
