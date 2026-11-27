#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace sage {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Color3 {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

enum class EntityShape {
    Box,
    Plane,
    Triangle,
    Ring,
    Model
};

// Scene entity rendered in 3D perspective (either a primitive or a .glb/.gltf model).
struct Entity {
    std::string id;
    EntityShape shape = EntityShape::Box;
    std::string modelPath;           // Used when shape == EntityShape::Model
    Vec3 position{0.0f, 0.0f, 0.0f}; // World meters (+Y up)
    Vec3 rotation{0.0f, 0.0f, 0.0f}; // pitch (X), yaw (Y), roll (Z) in degrees
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Color3 color{1.0f, 1.0f, 1.0f};  // Albedo for primitives; tint multiplier for .glb models
    bool emissive = false;
    bool visible = true;
};

struct SkySettings {
    float timeOfDay = 17.5f;                       // 0..24 hours (17.5 = golden sunset)
    Vec3 sunDirection{0.48f, 0.42f, 0.77f};        // Unit vector pointing toward the sun
    Color3 sunColor{1.0f, 0.84f, 0.62f};
    Color3 skyZenithColor{0.14f, 0.24f, 0.48f};
    Color3 skyHorizonColor{0.82f, 0.54f, 0.42f};
    Color3 groundAmbientColor{0.18f, 0.17f, 0.20f};
    float ambientIntensity = 0.36f;
    float fogStart = 75.0f;
    float fogEnd = 240.0f;
};

struct CameraState {
    Vec3 position{0.0f, 8.0f, -18.0f};
    Vec3 target{0.0f, 1.5f, 0.0f};
    float fovDegrees = 60.0f;
    float nearPlane = 0.2f;
    float farPlane = 450.0f;
};

class World {
public:
    std::string spawnEntity(
        const std::string& id,
        EntityShape shape,
        Vec3 position = {},
        Vec3 scale = {1.0f, 1.0f, 1.0f},
        Color3 color = {0.9f, 0.9f, 0.9f});

    std::string spawnModel(
        const std::string& id,
        const std::string& modelPath,
        Vec3 position = {},
        Vec3 scale = {1.0f, 1.0f, 1.0f});

    bool setPosition(const std::string& id, float x, float y, float z);
    bool setRotation(const std::string& id, float pitch, float yaw, float roll);
    bool setScale(const std::string& id, float x, float y, float z);
    bool setColor(const std::string& id, float r, float g, float b);
    bool setEmissive(const std::string& id, bool emissive);
    bool setVisible(const std::string& id, bool visible);

    bool hasEntity(const std::string& id) const;
    Vec3 getPosition(const std::string& id) const;
    bool destroy(const std::string& id);
    void clear();

    // Computes realistic sun angle, sky gradient, and fog tint from hour (0..24)
    void setTimeOfDay(float hours);

    const std::vector<Entity>& entities() const { return m_entities; }
    std::size_t entityCount() const { return m_entities.size(); }

    CameraState& camera() { return m_camera; }
    const CameraState& camera() const { return m_camera; }

    SkySettings& sky() { return m_sky; }
    const SkySettings& sky() const { return m_sky; }

private:
    Entity* findMutable(const std::string& id);
    const Entity* findConst(const std::string& id) const;

    std::vector<Entity> m_entities;
    std::unordered_map<std::string, std::size_t> m_indexById;
    CameraState m_camera{};
    SkySettings m_sky{};
};

} // namespace sage
