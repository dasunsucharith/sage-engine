#pragma once

#include "sage/world/world.h"

#include <cstddef>
#include <memory>
#include <string>

namespace sage {

struct RaycastHit {
    bool hit = false;
    Vec3 position{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    std::string entityId;
};

struct CircleCollisionResult {
    bool hit = false;
    float x = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vz = 0.0f;
    std::string hitEntityId;
};

struct RampSampleResult {
    bool onRamp = false;
    float groundY = 0.0f;
    float rampHeight = 0.0f;
    float rampYawDeg = 0.0f;
};

// Multithreaded general-purpose 3D rigid-body physics world powered by Jolt Physics 5.7,
// plus fast unified obstacle & ramp queries for vehicles and character controllers.
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool initialize();
    void shutdown();

    void clearBodies();

    bool addStaticBox(const std::string& entityId, Vec3 pos, Vec3 halfExtents, Vec3 rotDeg = {});
    bool addStaticDeck(const std::string& entityId, Vec3 pos, Vec3 halfExtents, float yawDeg = 0.0f);
    bool addStaticCylinder(const std::string& entityId, Vec3 basePos, float radius, float height);
    bool addStaticRamp(const std::string& entityId, Vec3 pos, float width, float height, float length, float yawDeg);

    bool addDynamicBox(
        const std::string& entityId,
        Vec3 pos,
        Vec3 halfExtents,
        float mass = 25.0f,
        float restitution = 0.25f,
        float friction = 0.65f,
        Vec3 rotDeg = {});

    bool addDynamicSphere(
        const std::string& entityId,
        Vec3 pos,
        float radius,
        float mass = 30.0f,
        float restitution = 0.55f,
        float friction = 0.50f);

    bool addDynamicCapsule(
        const std::string& entityId,
        Vec3 pos,
        float radius,
        float halfHeight,
        float mass = 12.0f,
        float restitution = 0.25f,
        float friction = 0.65f);

    bool addKinematicBox(const std::string& entityId, Vec3 pos, Vec3 halfExtents, Vec3 rotDeg = {});

    bool moveKinematic(const std::string& entityId, Vec3 pos, Vec3 rotDeg, float dt);
    bool applyImpulse(const std::string& entityId, Vec3 impulse);
    int applyExplosion(Vec3 center, float radius, float strength);

    // Pushes a horizontal circle (e.g. car or character) out of ALL static boxes & cylinders in the world!
    CircleCollisionResult resolveCircleXZ(
        float x, float y, float z, float radius, float vx, float vz, float bounce = 0.32f) const;

    // Samples elevation across ALL registered 3D stunt ramps AND drivable decks/piers!
    RampSampleResult sampleRampSurface(float x, float z) const;

    float getLinearSpeed(const std::string& entityId) const;
    void setGravity(Vec3 gravity);
    Vec3 getGravity() const;

    RaycastHit raycast(Vec3 origin, Vec3 direction, float maxDistance) const;

    void step(float dt, World& world, float elapsedSeconds = 0.0f);

    std::size_t bodyCount() const;
    std::size_t dynamicBodyCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
