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

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Color3 {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

enum class EntityShape {
    Box,
    Plane,
    Water,
    Sphere,
    Triangle,
    Ring,
    Model
};

// Scene entity rendered in 3D perspective (primitive or animated .glb/.gltf model).
struct Entity {
    std::string id;
    EntityShape shape = EntityShape::Box;
    std::string modelPath;
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Quat rotationQuat{};
    bool useQuaternion = false;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Color3 color{1.0f, 1.0f, 1.0f};
    bool emissive = false;
    bool visible = true;

    // glTF 2.0 animation playback state
    std::string activeAnimation; // Empty = auto-play clip 0 if model contains animations
    float animTime = 0.0f;
    float animSpeed = 1.0f;
    bool animPlaying = true;
    bool animLoop = true;
};

struct SkySettings {
    float timeOfDay = 17.5f;
    Vec3 sunDirection{0.48f, 0.42f, 0.77f};
    Color3 sunColor{1.0f, 0.84f, 0.62f};
    Color3 skyZenithColor{0.14f, 0.24f, 0.48f};
    Color3 skyHorizonColor{0.82f, 0.54f, 0.42f};
    Color3 groundAmbientColor{0.18f, 0.17f, 0.20f};
    float ambientIntensity = 0.36f;
    float fogStart = 125.0f;
    float fogEnd = 420.0f;
    bool shadowsEnabled = true;
    bool bloomEnabled = true;
    float bloomIntensity = 0.55f;
};

struct CameraState {
    Vec3 position{0.0f, 8.0f, -18.0f};
    Vec3 target{0.0f, 1.5f, 0.0f};
    float fovDegrees = 60.0f;
    float nearPlane = 0.50f;
    float farPlane = 850.0f;
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
    bool setRotationQuat(const std::string& id, float x, float y, float z, float w);
    bool setScale(const std::string& id, float x, float y, float z);
    bool setColor(const std::string& id, float r, float g, float b);
    bool setEmissive(const std::string& id, bool emissive);
    bool setVisible(const std::string& id, bool visible);

    bool playAnimation(const std::string& id, const std::string& clipName = "", bool loop = true, float speed = 1.0f);
    bool stopAnimation(const std::string& id);
    bool setAnimationSpeed(const std::string& id, float speed);
    void updateAnimations(float dt);

    bool hasEntity(const std::string& id) const;
    Vec3 getPosition(const std::string& id) const;
    Entity* findEntity(const std::string& id);
    const Entity* findEntity(const std::string& id) const;
    bool destroy(const std::string& id);
    void clear();

    void setTimeOfDay(float hours);

    std::vector<Entity>& entitiesMutable() { return m_entities; }
    const std::vector<Entity>& entities() const { return m_entities; }
    std::size_t entityCount() const { return m_entities.size(); }

    CameraState& camera() { return m_camera; }
    const CameraState& camera() const { return m_camera; }

    SkySettings& sky() { return m_sky; }
    const SkySettings& sky() const { return m_sky; }

private:
    std::vector<Entity> m_entities;
    std::unordered_map<std::string, std::size_t> m_indexById;
    CameraState m_camera{};
    SkySettings m_sky{};
};

} // namespace sage
