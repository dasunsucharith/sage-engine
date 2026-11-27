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

// Multithreaded 3D rigid-body physics world powered by Jolt Physics 5.7.
// Synchronizes dynamic 3D positions & quaternions back to sage::World every fixed tick.
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool initialize();
    void shutdown();

    // Removes all bodies cleanly (called on scene/Lua hot-reload)
    void clearBodies();

    bool addStaticBox(const std::string& entityId, Vec3 pos, Vec3 halfExtents, Vec3 rotDeg = {});
    bool addDynamicBox(
        const std::string& entityId,
        Vec3 pos,
        Vec3 halfExtents,
        float mass = 25.0f,
        float restitution = 0.25f,
        float friction = 0.65f,
        Vec3 rotDeg = {});
    bool addKinematicBox(const std::string& entityId, Vec3 pos, Vec3 halfExtents, Vec3 rotDeg = {});

    bool moveKinematic(const std::string& entityId, Vec3 pos, Vec3 rotDeg, float dt);
    bool applyImpulse(const std::string& entityId, Vec3 impulse);
    int applyExplosion(Vec3 center, float radius, float strength);

    float getLinearSpeed(const std::string& entityId) const;
    void setGravity(Vec3 gravity);
    Vec3 getGravity() const;

    RaycastHit raycast(Vec3 origin, Vec3 direction, float maxDistance) const;

    void step(float dt, World& world);

    std::size_t bodyCount() const;
    std::size_t dynamicBodyCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
