#include "sage/physics/physics_world.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace sage {

namespace {

namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
} // namespace Layers

namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr JPH::uint NUM_LAYERS = 2;
} // namespace BroadPhaseLayers

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return m_objectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer)) {
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING): return "NON_MOVING";
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING): return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

void joltTraceImpl(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    std::cout << "[SAGE][Jolt] " << buffer << "\n";
}

JPH::Quat eulerDegToJoltQuat(Vec3 rotDeg) {
    // Match gl_renderer.cpp Euler order: Yaw(Y) -> Pitch(X) -> Roll(Z)
    const glm::quat qY = glm::angleAxis(glm::radians(rotDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat qX = glm::angleAxis(glm::radians(rotDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat qZ = glm::angleAxis(glm::radians(rotDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::quat q = glm::normalize(qY * qX * qZ);
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

bool g_joltGlobalsInitialized = false;

} // namespace

struct BodyEntry {
    JPH::BodyID bodyId;
    bool isDynamic = false;
};

struct StaticObstacle2D {
    std::string id;
    bool isCylinder = false;
    float cx = 0.0f;
    float cz = 0.0f;
    float minY = -10.0f;
    float maxY = 100.0f;
    float hx = 1.0f;
    float hz = 1.0f;
    float radius = 0.5f;
    float yawRad = 0.0f;
};

struct StaticRampInfo {
    float cx = 0.0f;
    float cz = 0.0f;
    float baseY = 0.0f;
    float width = 5.0f;
    float height = 2.2f;
    float length = 7.5f;
    float yawDeg = 0.0f;
};

struct PhysicsWorld::Impl {
    bool initialized = false;
    BPLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl objectVsObjectLayerFilter;

    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    std::unordered_map<std::string, BodyEntry> bodiesByEntity;
    std::unordered_map<JPH::uint32, std::string> entityByBodyRaw;
    std::vector<StaticObstacle2D> staticObstacles;
    std::vector<StaticRampInfo> staticRamps;

    void removeExisting(const std::string& entityId) {
        const auto it = bodiesByEntity.find(entityId);
        if (it == bodiesByEntity.end()) {
            return;
        }
        auto& bi = physicsSystem->GetBodyInterface();
        const JPH::BodyID bid = it->second.bodyId;
        if (bi.IsAdded(bid)) {
            bi.RemoveBody(bid);
        }
        bi.DestroyBody(bid);
        entityByBodyRaw.erase(bid.GetIndexAndSequenceNumber());
        bodiesByEntity.erase(it);
    }
};

PhysicsWorld::PhysicsWorld()
    : m_impl(std::make_unique<Impl>()) {}

PhysicsWorld::~PhysicsWorld() {
    shutdown();
}

bool PhysicsWorld::initialize() {
    if (m_impl->initialized) {
        return true;
    }

    if (!g_joltGlobalsInitialized) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = joltTraceImpl;
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        g_joltGlobalsInitialized = true;
    }

    m_impl->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    const unsigned int hwThreads = std::max(1u, std::thread::hardware_concurrency());
    const unsigned int workerThreads = std::max(1u, hwThreads > 1 ? hwThreads - 1 : 1u);
    m_impl->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        static_cast<int>(workerThreads));

    m_impl->physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    constexpr JPH::uint kMaxBodies = 2048;
    constexpr JPH::uint kNumBodyMutexes = 0;
    constexpr JPH::uint kMaxBodyPairs = 4096;
    constexpr JPH::uint kMaxContactConstraints = 2048;

    m_impl->physicsSystem->Init(
        kMaxBodies,
        kNumBodyMutexes,
        kMaxBodyPairs,
        kMaxContactConstraints,
        m_impl->broadPhaseLayerInterface,
        m_impl->objectVsBroadPhaseLayerFilter,
        m_impl->objectVsObjectLayerFilter);

    m_impl->physicsSystem->SetGravity(JPH::Vec3(0.0f, -22.0f, 0.0f));
    m_impl->initialized = true;

    std::cout << "[SAGE][Physics] Jolt Physics 5.7 initialized (" << workerThreads
              << " worker thread(s))\n";
    return true;
}

void PhysicsWorld::shutdown() {
    if (!m_impl || !m_impl->initialized) {
        return;
    }
    clearBodies();
    m_impl->physicsSystem.reset();
    m_impl->jobSystem.reset();
    m_impl->tempAllocator.reset();
    m_impl->initialized = false;
}

void PhysicsWorld::clearBodies() {
    if (!m_impl->initialized) {
        return;
    }
    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    for (const auto& [id, entry] : m_impl->bodiesByEntity) {
        if (bi.IsAdded(entry.bodyId)) {
            bi.RemoveBody(entry.bodyId);
        }
        bi.DestroyBody(entry.bodyId);
    }
    m_impl->bodiesByEntity.clear();
    m_impl->entityByBodyRaw.clear();
    m_impl->staticObstacles.clear();
    m_impl->staticRamps.clear();
}

bool PhysicsWorld::addStaticBox(const std::string& entityId, Vec3 pos, Vec3 halfExtents, Vec3 rotDeg) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    const JPH::Vec3 he(
        std::max(halfExtents.x, 0.05f),
        std::max(halfExtents.y, 0.05f),
        std::max(halfExtents.z, 0.05f));

    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(he),
        JPH::RVec3(pos.x, pos.y, pos.z),
        eulerDegToJoltQuat(rotDeg),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);
    settings.mFriction = 0.75f;
    settings.mRestitution = 0.2f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, false};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;

    // Register solid above-ground boxes as driveable obstacle colliders (skip buried ground floor slab)
    if (pos.y + he.GetY() > 0.25f) {
        StaticObstacle2D obs;
        obs.id = entityId;
        obs.isCylinder = false;
        obs.cx = pos.x;
        obs.cz = pos.z;
        obs.minY = pos.y - he.GetY();
        obs.maxY = pos.y + he.GetY();
        obs.hx = he.GetX();
        obs.hz = he.GetZ();
        obs.yawRad = glm::radians(rotDeg.y);
        m_impl->staticObstacles.push_back(std::move(obs));
    }
    return true;
}

bool PhysicsWorld::addStaticCylinder(const std::string& entityId, Vec3 basePos, float radius, float height) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    const float r = std::max(radius, 0.12f);
    const float h = std::max(height, 0.5f);
    const float halfH = h * 0.5f;

    JPH::BodyCreationSettings settings(
        new JPH::CylinderShape(halfH, r),
        JPH::RVec3(basePos.x, basePos.y + halfH, basePos.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);
    settings.mFriction = 0.7f;
    settings.mRestitution = 0.25f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, false};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;

    StaticObstacle2D obs;
    obs.id = entityId;
    obs.isCylinder = true;
    obs.cx = basePos.x;
    obs.cz = basePos.z;
    obs.minY = basePos.y;
    obs.maxY = basePos.y + h;
    obs.radius = r;
    m_impl->staticObstacles.push_back(std::move(obs));
    return true;
}

bool PhysicsWorld::addStaticRamp(
    const std::string& entityId,
    Vec3 pos,
    float width,
    float height,
    float length,
    float yawDeg) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    const float hx = std::max(width * 0.5f, 0.1f);
    const float hz = std::max(length * 0.5f, 0.1f);
    const float h = std::max(height, 0.1f);

    // 6-vertex 3D ramp rising from -Z (y=0) to +Z (y=h)
    JPH::Array<JPH::Vec3> points;
    points.push_back(JPH::Vec3(-hx, 0.0f, -hz));
    points.push_back(JPH::Vec3( hx, 0.0f, -hz));
    points.push_back(JPH::Vec3(-hx, 0.0f,  hz));
    points.push_back(JPH::Vec3( hx, 0.0f,  hz));
    points.push_back(JPH::Vec3(-hx, h,     hz));
    points.push_back(JPH::Vec3( hx, h,     hz));

    JPH::ConvexHullShapeSettings hullSettings(points, 0.02f);
    const auto shapeResult = hullSettings.Create();
    if (shapeResult.HasError()) {
        std::cerr << "[SAGE][Jolt] Failed to build ramp hull for \"" << entityId
                  << "\": " << shapeResult.GetError() << "\n";
        return false;
    }

    JPH::BodyCreationSettings settings(
        shapeResult.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        eulerDegToJoltQuat({0.0f, yawDeg, 0.0f}),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);
    settings.mFriction = 0.65f;
    settings.mRestitution = 0.2f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, false};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;
    m_impl->staticRamps.push_back({pos.x, pos.z, pos.y, width, height, length, yawDeg});
    return true;
}

bool PhysicsWorld::addDynamicBox(
    const std::string& entityId,
    Vec3 pos,
    Vec3 halfExtents,
    float mass,
    float restitution,
    float friction,
    Vec3 rotDeg) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    const JPH::Vec3 he(
        std::max(halfExtents.x, 0.05f),
        std::max(halfExtents.y, 0.05f),
        std::max(halfExtents.z, 0.05f));

    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(he),
        JPH::RVec3(pos.x, pos.y, pos.z),
        eulerDegToJoltQuat(rotDeg),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(mass, 0.5f);
    settings.mRestitution = std::clamp(restitution, 0.0f, 0.95f);
    settings.mFriction = std::clamp(friction, 0.0f, 2.0f);
    settings.mLinearDamping = 0.12f;
    settings.mAngularDamping = 0.18f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, true};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;
    return true;
}

bool PhysicsWorld::addDynamicSphere(
    const std::string& entityId,
    Vec3 pos,
    float radius,
    float mass,
    float restitution,
    float friction) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    JPH::BodyCreationSettings settings(
        new JPH::SphereShape(std::max(radius, 0.08f)),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(mass, 0.5f);
    settings.mRestitution = std::clamp(restitution, 0.0f, 0.96f);
    settings.mFriction = std::clamp(friction, 0.0f, 2.0f);
    settings.mLinearDamping = 0.06f;
    settings.mAngularDamping = 0.08f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, true};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;
    return true;
}

bool PhysicsWorld::addDynamicCapsule(
    const std::string& entityId,
    Vec3 pos,
    float radius,
    float halfHeight,
    float mass,
    float restitution,
    float friction) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    JPH::BodyCreationSettings settings(
        new JPH::CapsuleShape(std::max(halfHeight, 0.05f), std::max(radius, 0.08f)),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(mass, 0.5f);
    settings.mRestitution = std::clamp(restitution, 0.0f, 0.95f);
    settings.mFriction = std::clamp(friction, 0.0f, 2.0f);
    settings.mLinearDamping = 0.14f;
    settings.mAngularDamping = 0.20f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, true};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;
    return true;
}

bool PhysicsWorld::addKinematicBox(const std::string& entityId, Vec3 pos, Vec3 halfExtents, Vec3 rotDeg) {
    if (!m_impl->initialized) {
        return false;
    }
    m_impl->removeExisting(entityId);

    const JPH::Vec3 he(
        std::max(halfExtents.x, 0.05f),
        std::max(halfExtents.y, 0.05f),
        std::max(halfExtents.z, 0.05f));

    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(he),
        JPH::RVec3(pos.x, pos.y, pos.z),
        eulerDegToJoltQuat(rotDeg),
        JPH::EMotionType::Kinematic,
        Layers::MOVING);
    settings.mFriction = 0.4f;
    settings.mRestitution = 0.35f;

    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID bid = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (bid.IsInvalid()) {
        return false;
    }

    m_impl->bodiesByEntity[entityId] = {bid, false};
    m_impl->entityByBodyRaw[bid.GetIndexAndSequenceNumber()] = entityId;
    return true;
}

bool PhysicsWorld::moveKinematic(const std::string& entityId, Vec3 pos, Vec3 rotDeg, float dt) {
    if (!m_impl->initialized) {
        return false;
    }
    const auto it = m_impl->bodiesByEntity.find(entityId);
    if (it == m_impl->bodiesByEntity.end()) {
        return false;
    }
    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.MoveKinematic(
        it->second.bodyId,
        JPH::RVec3(pos.x, pos.y, pos.z),
        eulerDegToJoltQuat(rotDeg),
        std::max(dt, 1.0f / 240.0f));
    return true;
}

bool PhysicsWorld::applyImpulse(const std::string& entityId, Vec3 impulse) {
    if (!m_impl->initialized) {
        return false;
    }
    const auto it = m_impl->bodiesByEntity.find(entityId);
    if (it == m_impl->bodiesByEntity.end() || !it->second.isDynamic) {
        return false;
    }
    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.ActivateBody(it->second.bodyId);
    bi.AddImpulse(it->second.bodyId, JPH::Vec3(impulse.x, impulse.y, impulse.z));
    return true;
}

int PhysicsWorld::applyExplosion(Vec3 center, float radius, float strength) {
    if (!m_impl->initialized || radius <= 0.01f) {
        return 0;
    }
    auto& bi = m_impl->physicsSystem->GetBodyInterface();
    int affected = 0;

    for (const auto& [id, entry] : m_impl->bodiesByEntity) {
        if (!entry.isDynamic) {
            continue;
        }
        const JPH::RVec3 p = bi.GetCenterOfMassPosition(entry.bodyId);
        const float dx = static_cast<float>(p.GetX()) - center.x;
        const float dy = static_cast<float>(p.GetY()) - center.y;
        const float dz = static_cast<float>(p.GetZ()) - center.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > radius) {
            continue;
        }

        const float falloff = 1.0f - (dist / radius);
        const float invDist = dist > 0.05f ? (1.0f / dist) : 0.0f;
        const float nx = dist > 0.05f ? (dx * invDist) : 0.0f;
        const float nz = dist > 0.05f ? (dz * invDist) : 0.0f;
        const float mag = strength * (0.35f + 0.65f * falloff);

        bi.ActivateBody(entry.bodyId);
        bi.AddImpulse(entry.bodyId, JPH::Vec3(nx * mag, (0.65f + std::max(dy * invDist, 0.0f)) * mag, nz * mag));
        bi.AddAngularImpulse(entry.bodyId, JPH::Vec3(nz * mag * 0.25f, mag * 0.15f, -nx * mag * 0.25f));
        ++affected;
    }
    return affected;
}

CircleCollisionResult PhysicsWorld::resolveCircleXZ(
    float x, float y, float z, float radius, float vx, float vz, float bounce) const {
    CircleCollisionResult res{};
    res.x = x;
    res.z = z;
    res.vx = vx;
    res.vz = vz;

    for (const StaticObstacle2D& obs : m_impl->staticObstacles) {
        if (y > obs.maxY || (y + 1.5f) < obs.minY) {
            continue;
        }

        if (obs.isCylinder) {
            const float dx = res.x - obs.cx;
            const float dz = res.z - obs.cz;
            const float minDist = radius + obs.radius;
            const float distSq = dx * dx + dz * dz;
            if (distSq < minDist * minDist) {
                const float dist = std::sqrt(std::max(distSq, 1e-6f));
                const float nx = dist > 1e-4f ? dx / dist : 1.0f;
                const float nz = dist > 1e-4f ? dz / dist : 0.0f;
                const float push = minDist - dist;
                res.x += nx * push;
                res.z += nz * push;

                const float vn = res.vx * nx + res.vz * nz;
                if (vn < 0.0f) {
                    res.vx -= (1.0f + bounce) * vn * nx;
                    res.vz -= (1.0f + bounce) * vn * nz;
                }
                res.hit = true;
                res.hitEntityId = obs.id;
            }
        } else {
            // Oriented 2D box push-out in obstacle local space
            const float c = std::cos(obs.yawRad);
            const float s = std::sin(obs.yawRad);
            const float dx = res.x - obs.cx;
            const float dz = res.z - obs.cz;

            const float localX =  dx * c - dz * s;
            const float localZ =  dx * s + dz * c;

            const float clampedX = std::clamp(localX, -obs.hx, obs.hx);
            const float clampedZ = std::clamp(localZ, -obs.hz, obs.hz);

            const float diffX = localX - clampedX;
            const float diffZ = localZ - clampedZ;
            const float distSq = diffX * diffX + diffZ * diffZ;

            if (distSq < radius * radius) {
                float lnx = 0.0f, lnz = 0.0f, push = 0.0f;
                if (distSq > 1e-6f) {
                    const float dist = std::sqrt(distSq);
                    lnx = diffX / dist;
                    lnz = diffZ / dist;
                    push = radius - dist;
                } else {
                    const float penX = obs.hx - std::abs(localX);
                    const float penZ = obs.hz - std::abs(localZ);
                    if (penX < penZ) {
                        lnx = localX >= 0.0f ? 1.0f : -1.0f;
                        push = penX + radius;
                    } else {
                        lnz = localZ >= 0.0f ? 1.0f : -1.0f;
                        push = penZ + radius;
                    }
                }

                // Rotate normal back to world XZ
                const float wnx =  lnx * c + lnz * s;
                const float wnz = -lnx * s + lnz * c;
                res.x += wnx * push;
                res.z += wnz * push;

                const float vn = res.vx * wnx + res.vz * wnz;
                if (vn < 0.0f) {
                    res.vx -= (1.0f + bounce) * vn * wnx;
                    res.vz -= (1.0f + bounce) * vn * wnz;
                }
                res.hit = true;
                res.hitEntityId = obs.id;
            }
        }
    }

    return res;
}

RampSampleResult PhysicsWorld::sampleRampSurface(float x, float z) const {
    RampSampleResult best{};
    for (const StaticRampInfo& r : m_impl->staticRamps) {
        const float rad = glm::radians(r.yawDeg);
        const float dx = x - r.cx;
        const float dz = z - r.cz;
        const float localZ = dx * std::sin(rad) + dz * std::cos(rad);
        const float localX = dx * std::cos(rad) - dz * std::sin(rad);

        if (std::abs(localX) <= (r.width * 0.5f + 0.65f) && std::abs(localZ) <= (r.length * 0.5f + 0.25f)) {
            const float t = std::clamp((localZ + r.length * 0.5f) / r.length, 0.0f, 1.0f);
            const float h = r.baseY + t * r.height;
            if (!best.onRamp || h > best.groundY) {
                best.onRamp = true;
                best.groundY = h;
                best.rampHeight = r.height;
                best.rampYawDeg = r.yawDeg;
            }
        }
    }
    return best;
}

float PhysicsWorld::getLinearSpeed(const std::string& entityId) const {
    if (!m_impl->initialized) {
        return 0.0f;
    }
    const auto it = m_impl->bodiesByEntity.find(entityId);
    if (it == m_impl->bodiesByEntity.end()) {
        return 0.0f;
    }
    const auto& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::Vec3 v = bi.GetLinearVelocity(it->second.bodyId);
    return v.Length();
}

void PhysicsWorld::setGravity(Vec3 gravity) {
    if (m_impl->initialized) {
        m_impl->physicsSystem->SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }
}

Vec3 PhysicsWorld::getGravity() const {
    if (!m_impl->initialized) {
        return {0.0f, -22.0f, 0.0f};
    }
    const JPH::Vec3 g = m_impl->physicsSystem->GetGravity();
    return {g.GetX(), g.GetY(), g.GetZ()};
}

RaycastHit PhysicsWorld::raycast(Vec3 origin, Vec3 direction, float maxDistance) const {
    RaycastHit out{};
    if (!m_impl->initialized) {
        return out;
    }
    const float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len < 0.0001f) {
        return out;
    }
    const JPH::RRayCast ray(
        JPH::RVec3(origin.x, origin.y, origin.z),
        JPH::Vec3(direction.x / len * maxDistance, direction.y / len * maxDistance, direction.z / len * maxDistance));

    JPH::RayCastResult hitResult;
    if (m_impl->physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hitResult)) {
        out.hit = true;
        const JPH::RVec3 hitPos = ray.GetPointOnRay(hitResult.mFraction);
        out.position = {
            static_cast<float>(hitPos.GetX()),
            static_cast<float>(hitPos.GetY()),
            static_cast<float>(hitPos.GetZ()),
        };
        const auto it = m_impl->entityByBodyRaw.find(hitResult.mBodyID.GetIndexAndSequenceNumber());
        if (it != m_impl->entityByBodyRaw.end()) {
            out.entityId = it->second;
        }
    }
    return out;
}

void PhysicsWorld::step(float dt, World& world) {
    if (!m_impl->initialized) {
        return;
    }

    constexpr int kCollisionSteps = 1;
    m_impl->physicsSystem->Update(dt, kCollisionSteps, m_impl->tempAllocator.get(), m_impl->jobSystem.get());

    const auto& bi = m_impl->physicsSystem->GetBodyInterface();
    for (const auto& [entityId, entry] : m_impl->bodiesByEntity) {
        if (!entry.isDynamic) {
            continue;
        }
        JPH::RVec3 pos;
        JPH::Quat rot;
        bi.GetPositionAndRotation(entry.bodyId, pos, rot);
        world.setPosition(
            entityId,
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ()));
        world.setRotationQuat(entityId, rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
    }
}

std::size_t PhysicsWorld::bodyCount() const {
    return m_impl->bodiesByEntity.size();
}

std::size_t PhysicsWorld::dynamicBodyCount() const {
    std::size_t count = 0;
    for (const auto& [id, entry] : m_impl->bodiesByEntity) {
        if (entry.isDynamic) {
            ++count;
        }
    }
    return count;
}

} // namespace sage
