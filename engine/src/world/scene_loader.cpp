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
            const Vec3 pos = readVec3(e, "position", {0.0f, 0.0f, 0.0f});
            const Vec3 rot = readVec3(e, "rotation", {0.0f, 0.0f, 0.0f});
            const Vec3 scale = readVec3(e, "scale", {1.0f, 1.0f, 1.0f});
            const bool emissive = e.value("emissive", false);

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
                } else if (type == "primitive_sphere") {
                    shape = EntityShape::Sphere;
                } else if (type == "primitive_ring") {
                    shape = EntityShape::Ring;
                } else if (type == "primitive_triangle") {
                    shape = EntityShape::Triangle;
                }
                const Color3 color = readColor3(e, "color", {0.8f, 0.8f, 0.8f});
                world.spawnEntity(id, shape, pos, scale, color);
            }

            world.setRotation(id, rot.x, rot.y, rot.z);
            world.setEmissive(id, emissive);
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
                } else {
                    const Vec3 he = readVec3(col, "halfExtents", {scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f});
                    physics->addStaticBox(colId, {pos.x, pos.y + offsetY, pos.z}, he, rot);
                    ++colliderCount;
                }
            }
        }
    }

    std::cout << "[SAGE][Scene] Loaded \"" << sceneName << "\" from " << scenePath.string()
              << " (" << loadedCount << " entities, " << colliderCount << " solid collider(s))\n";
    return true;
}

} // namespace sage
