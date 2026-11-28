#include "sage/world/scene_loader.h"

#include "sage/physics/physics_world.h"
#include "sage/world/world.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace sage {

namespace {

Vec3 readVec3(const nlohmann::json& obj, const char* key, Vec3 fallback) {
    if (!obj.contains(key) || !obj[key].is_array() || obj[key].size() < 3) {
        return fallback;
    }
    return {
        obj[key][0].get<float>(),
        obj[key][1].get<float>(),
        obj[key][2].get<float>(),
    };
}

Color3 readColor3(const nlohmann::json& obj, const char* key, Color3 fallback) {
    if (!obj.contains(key) || !obj[key].is_array() || obj[key].size() < 3) {
        return fallback;
    }
    return {
        obj[key][0].get<float>(),
        obj[key][1].get<float>(),
        obj[key][2].get<float>(),
    };
}

} // namespace

bool SceneLoader::loadFromFile(const std::filesystem::path& scenePath, World& world, PhysicsWorld* physics) {
    if (!std::filesystem::exists(scenePath)) {
        std::cerr << "[SAGE][Scene] Scene file not found: " << scenePath.string() << "\n";
        return false;
    }

    std::ifstream file(scenePath);
    if (!file.is_open()) {
        std::cerr << "[SAGE][Scene] Failed to open scene file: " << scenePath.string() << "\n";
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception& ex) {
        std::cerr << "[SAGE][Scene] JSON parse error in " << scenePath.string()
                  << ":\n  " << ex.what() << "\n";
        return false;
    }

    const std::string sceneName = root.value("name", scenePath.stem().string());

    if (root.contains("terrain") && root["terrain"].is_object()) {
        const auto& tJson = root["terrain"];
        world.terrain().enabled = tJson.value("enabled", world.terrain().enabled);
        const std::string modeStr = tJson.value("mode", "city");
        world.terrain().mode = (modeStr == "island" || modeStr == "wilderness") ? 1 : 0;
        world.terrain().halfSize = tJson.value("halfSize", world.terrain().halfSize);
        world.terrain().maxHeight = tJson.value("maxHeight", world.terrain().maxHeight);
        world.terrain().gridResolution = tJson.value("gridResolution", world.terrain().gridResolution);
    }

    if (root.contains("sky") && root["sky"].is_object()) {
        const auto& skyJson = root["sky"];
        if (skyJson.contains("timeOfDay") && skyJson["timeOfDay"].is_number()) {
            world.setTimeOfDay(skyJson["timeOfDay"].get<float>());
        }
        if (skyJson.contains("sunColor")) {
            world.sky().sunColor = readColor3(skyJson, "sunColor", world.sky().sunColor);
        }
        if (skyJson.contains("skyZenithColor")) {
            world.sky().skyZenithColor = readColor3(skyJson, "skyZenithColor", world.sky().skyZenithColor);
        }
        if (skyJson.contains("skyHorizonColor")) {
            world.sky().skyHorizonColor = readColor3(skyJson, "skyHorizonColor", world.sky().skyHorizonColor);
        }
        if (skyJson.contains("ambientIntensity") && skyJson["ambientIntensity"].is_number()) {
            world.sky().ambientIntensity = skyJson["ambientIntensity"].get<float>();
        }
        if (skyJson.contains("fogStart") && skyJson["fogStart"].is_number()) {
            world.sky().fogStart = skyJson["fogStart"].get<float>();
        }
        if (skyJson.contains("fogEnd") && skyJson["fogEnd"].is_number()) {
            world.sky().fogEnd = skyJson["fogEnd"].get<float>();
        }
        if (skyJson.contains("rainIntensity") && skyJson["rainIntensity"].is_number()) {
            world.sky().rainIntensity = skyJson["rainIntensity"].get<float>();
        }
        if (skyJson.contains("wetness") && skyJson["wetness"].is_number()) {
            world.sky().wetness = skyJson["wetness"].get<float>();
        }
    }

    if (root.contains("camera") && root["camera"].is_object()) {
        const auto& camJson = root["camera"];
        world.camera().position = readVec3(camJson, "position", world.camera().position);
        world.camera().target = readVec3(camJson, "target", world.camera().target);
        world.camera().fovDegrees = camJson.value("fov", world.camera().fovDegrees);
    }

    std::size_t loadedCount = 0;
    std::size_t colliderCount = 0;
    if (root.contains("entities") && root["entities"].is_array()) {
        for (std::size_t i = 0; i < root["entities"].size(); ++i) {
            const auto& e = root["entities"][i];
            if (!e.is_object()) {
                continue;
            }

            const std::string id = e.value("id", "scene_entity_" + std::to_string(i));
            const std::string type = e.value("type", "primitive_box");
            Vec3 pos = readVec3(e, "position", {0.0f, 0.0f, 0.0f});
            const Vec3 rot = readVec3(e, "rotation", {0.0f, 0.0f, 0.0f});
            const Vec3 scale = readVec3(e, "scale", {1.0f, 1.0f, 1.0f});
            const bool emissive = e.value("emissive", false);
            const bool visible = e.value("visible", true);

            // Auto-ground off-road 3D models cleanly onto sculpted 3D terrain hills using 5-point footprint sampling
            // so buildings, boulders & trees embed firmly on steep slopes and NEVER float in mid-air!
            if (type == "model" && world.terrain().enabled && std::abs(pos.y) < 0.05f) {
                const float hc = world.sampleTerrain(pos.x, pos.z).height;
                if (world.terrain().mode == 1) {
                    const float footR = std::clamp(std::max(scale.x, scale.z) * 1.45f, 0.8f, 3.6f);
                    const float hMin = std::min({
                        hc,
                        world.sampleTerrain(pos.x + footR, pos.z).height,
                        world.sampleTerrain(pos.x - footR, pos.z).height,
                        world.sampleTerrain(pos.x, pos.z + footR).height,
                        world.sampleTerrain(pos.x, pos.z - footR).height
                    });
                    // Sink gently toward lowest edge of footprint so slopes never expose floating undersides!
                    pos.y += (hMin * 0.78f + hc * 0.22f) - 0.14f;
                } else if (hc > 0.05f) {
                    pos.y += hc;
                }
            }

            if (type == "model") {
                const std::string modelPath = e.value("model", "");
                if (modelPath.empty()) {
                    std::cerr << "[SAGE][Scene] Entity \"" << id << "\" in " << scenePath.string()
                              << " has type=\"model\" but is missing \"model\" path.\n";
                    continue;
                }
                world.spawnModel(id, modelPath, pos, scale);
                if (e.contains("color")) {
                    const Color3 tint = readColor3(e, "color", {1.0f, 1.0f, 1.0f});
                    world.setColor(id, tint.r, tint.g, tint.b);
                }
            } else {
                EntityShape shape = EntityShape::Box;
                if (type == "primitive_plane") {
                    shape = EntityShape::Plane;
                } else if (type == "water") {
                    shape = EntityShape::Water;
                } else if (type == "primitive_sphere") {
                    shape = EntityShape::Sphere;
                } else if (type == "primitive_ring") {
                    shape = EntityShape::Ring;
                } else if (type == "primitive_triangle") {
                    shape = EntityShape::Triangle;
                }
                const Color3 color = readColor3(e, "color", {0.8f, 0.8f, 0.8f});
                world.spawnEntity(id, shape, pos, scale, color);
                if (shape == EntityShape::Water && e.contains("seabedY") && e["seabedY"].is_number()) {
                    world.setWaterSeabed(id, e["seabedY"].get<float>());
                }
            }

            world.setRotation(id, rot.x, rot.y, rot.z);
            world.setEmissive(id, emissive);
            world.setVisible(id, visible);
            ++loadedCount;

            // Optional declarative collider in JSON — automatically solid for Jolt AND vehicles!
            if (physics && e.contains("collider") && e["collider"].is_object()) {
                const auto& col = e["collider"];
                const std::string colType = col.value("type", "box");
                const float offsetY = col.value("offsetY", 0.0f);
                const std::string colId = "scene_" + id;

                if (colType == "cylinder") {
                    const float radius = col.value("radius", 0.6f);
                    const float height = col.value("height", 8.0f);
                    physics->addStaticCylinder(colId, {pos.x, pos.y + offsetY, pos.z}, radius, height);
                    ++colliderCount;
                } else if (colType == "ramp") {
                    const float w = col.value("width", 5.0f * scale.x);
                    const float h = col.value("height", 2.2f * scale.y);
                    const float l = col.value("length", 7.5f * scale.z);
                    physics->addStaticRamp(colId, {pos.x, pos.y + offsetY, pos.z}, w, h, l, rot.y);
                    ++colliderCount;
                } else if (colType == "deck") {
                    const Vec3 he = readVec3(col, "halfExtents", {scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f});
                    physics->addStaticDeck(colId, {pos.x, pos.y + offsetY, pos.z}, he, rot.y);
                    ++colliderCount;
                } else {
                    const Vec3 he = readVec3(col, "halfExtents", {scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f});
                    physics->addStaticBox(colId, {pos.x, pos.y + offsetY, pos.z}, he, rot);
                    ++colliderCount;
                }
            }
        }
    }

    // Optional declarative dynamic point lights in JSON ("lights": [...])
    std::size_t lightCount = 0;
    if (root.contains("lights") && root["lights"].is_array()) {
        for (std::size_t i = 0; i < root["lights"].size(); ++i) {
            const auto& l = root["lights"][i];
            if (!l.is_object()) {
                continue;
            }
            const std::string lid = l.value("id", "scene_light_" + std::to_string(i));
            const Vec3 pos = readVec3(l, "position", {0.0f, 3.0f, 0.0f});
            const Color3 col = readColor3(l, "color", {1.0f, 0.9f, 0.7f});
            const float radius = l.value("radius", 18.0f);
            const float intensity = l.value("intensity", 2.0f);
            world.setPointLight(lid, pos, col, radius, intensity);
            ++lightCount;
        }
    }

    if (physics && world.terrain().enabled) {
        physics->buildTerrainCollider(world);
    }

    std::cout << "[SAGE][Scene] Loaded \"" << sceneName << "\" from " << scenePath.string()
              << " (" << loadedCount << " entities, " << colliderCount << " solid collider(s), "
              << lightCount << " dynamic light(s))\n";
    return true;
}

bool SceneLoader::saveToFile(
    const std::filesystem::path& scenePath,
    const World& world,
    const std::string& sceneName) {
    nlohmann::json root;
    root["name"] = sceneName;
    root["version"] = 2;

    const SkySettings& sky = world.sky();
    root["sky"] = {
        {"timeOfDay", sky.timeOfDay},
        {"ambientIntensity", sky.ambientIntensity},
        {"fogStart", sky.fogStart},
        {"fogEnd", sky.fogEnd},
        {"rainIntensity", sky.rainIntensity},
        {"wetness", sky.wetness},
    };

    const CameraState& cam = world.camera();
    root["camera"] = {
        {"position", {cam.position.x, cam.position.y, cam.position.z}},
        {"target", {cam.target.x, cam.target.y, cam.target.z}},
        {"fov", cam.fovDegrees},
    };

    nlohmann::json lightsArr = nlohmann::json::array();
    for (const PointLight& pl : world.pointLights()) {
        lightsArr.push_back({
            {"id", pl.id},
            {"position", {pl.position.x, pl.position.y, pl.position.z}},
            {"color", {pl.color.r, pl.color.g, pl.color.b}},
            {"radius", pl.radius},
            {"intensity", pl.intensity},
        });
    }
    root["lights"] = std::move(lightsArr);

    nlohmann::json entitiesArr = nlohmann::json::array();
    for (const Entity& e : world.entities()) {
        nlohmann::json obj;
        obj["id"] = e.id;
        switch (e.shape) {
            case EntityShape::Model:
                obj["type"] = "model";
                obj["model"] = e.modelPath;
                break;
            case EntityShape::Plane: obj["type"] = "primitive_plane"; break;
            case EntityShape::Water:
                obj["type"] = "water";
                obj["seabedY"] = e.waterSeabedY;
                break;
            case EntityShape::Sphere: obj["type"] = "primitive_sphere"; break;
            case EntityShape::Ring: obj["type"] = "primitive_ring"; break;
            case EntityShape::Triangle: obj["type"] = "primitive_triangle"; break;
            case EntityShape::Box:
            default: obj["type"] = "primitive_box"; break;
        }
        obj["position"] = {e.position.x, e.position.y, e.position.z};
        obj["rotation"] = {e.rotation.x, e.rotation.y, e.rotation.z};
        obj["scale"] = {e.scale.x, e.scale.y, e.scale.z};
        obj["color"] = {e.color.r, e.color.g, e.color.b};
        if (e.emissive) obj["emissive"] = true;
        if (!e.visible) obj["visible"] = false;
        entitiesArr.push_back(std::move(obj));
    }
    root["entities"] = std::move(entitiesArr);

    std::error_code ec;
    if (scenePath.has_parent_path()) {
        std::filesystem::create_directories(scenePath.parent_path(), ec);
    }
    std::ofstream out(scenePath);
    if (!out.is_open()) {
        std::cerr << "[SAGE][Scene] Failed to save scene file: " << scenePath.string() << "\n";
        return false;
    }
    out << root.dump(2) << "\n";
    std::cout << "[SAGE][Scene] Saved \"" << sceneName << "\" -> " << scenePath.string()
              << " (" << world.entityCount() << " entities, " << world.pointLights().size() << " lights)\n";
    return true;
}

} // namespace sage
