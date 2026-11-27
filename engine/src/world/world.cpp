#include "sage/world/world.h"

#include <algorithm>
#include <cmath>

namespace sage {

namespace {

constexpr float kPi = 3.14159265358979323846f;

Color3 lerpColor(const Color3& a, const Color3& b, float t) {
    const float k = std::clamp(t, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * k,
        a.g + (b.g - a.g) * k,
        a.b + (b.b - a.b) * k,
    };
}

} // namespace

Entity* World::findEntity(const std::string& id) {
    const auto it = m_indexById.find(id);
    if (it == m_indexById.end()) {
        return nullptr;
    }
    return &m_entities[it->second];
}

const Entity* World::findEntity(const std::string& id) const {
    const auto it = m_indexById.find(id);
    if (it == m_indexById.end()) {
        return nullptr;
    }
    return &m_entities[it->second];
}

std::string World::spawnEntity(
    const std::string& id,
    EntityShape shape,
    Vec3 position,
    Vec3 scale,
    Color3 color) {
    if (Entity* existing = findEntity(id)) {
        existing->shape = shape;
        existing->modelPath.clear();
        existing->position = position;
        existing->rotation = {};
        existing->rotationQuat = {};
        existing->useQuaternion = false;
        existing->scale = scale;
        existing->color = color;
        existing->emissive = false;
        existing->visible = true;
        return existing->id;
    }

    Entity entity;
    entity.id = id;
    entity.shape = shape;
    entity.position = position;
    entity.scale = scale;
    entity.color = color;

    m_indexById[id] = m_entities.size();
    m_entities.push_back(std::move(entity));
    return id;
}

std::string World::spawnModel(
    const std::string& id,
    const std::string& modelPath,
    Vec3 position,
    Vec3 scale) {
    if (Entity* existing = findEntity(id)) {
        existing->shape = EntityShape::Model;
        existing->modelPath = modelPath;
        existing->position = position;
        existing->rotation = {};
        existing->rotationQuat = {};
        existing->useQuaternion = false;
        existing->scale = scale;
        existing->color = {1.0f, 1.0f, 1.0f};
        existing->emissive = false;
        existing->visible = true;
        return existing->id;
    }

    Entity entity;
    entity.id = id;
    entity.shape = EntityShape::Model;
    entity.modelPath = modelPath;
    entity.position = position;
    entity.scale = scale;
    entity.color = {1.0f, 1.0f, 1.0f};

    m_indexById[id] = m_entities.size();
    m_entities.push_back(std::move(entity));
    return id;
}

bool World::setPosition(const std::string& id, float x, float y, float z) {
    if (Entity* e = findEntity(id)) {
        e->position = {x, y, z};
        return true;
    }
    return false;
}

bool World::setRotation(const std::string& id, float pitch, float yaw, float roll) {
    if (Entity* e = findEntity(id)) {
        e->rotation = {pitch, yaw, roll};
        e->useQuaternion = false;
        return true;
    }
    return false;
}

bool World::setRotationQuat(const std::string& id, float x, float y, float z, float w) {
    if (Entity* e = findEntity(id)) {
        e->rotationQuat = {x, y, z, w};
        e->useQuaternion = true;
        return true;
    }
    return false;
}

bool World::setScale(const std::string& id, float x, float y, float z) {
    if (Entity* e = findEntity(id)) {
        e->scale = {x, y, z};
        return true;
    }
    return false;
}

bool World::setColor(const std::string& id, float r, float g, float b) {
    if (Entity* e = findEntity(id)) {
        e->color = {r, g, b};
        return true;
    }
    return false;
}

bool World::setEmissive(const std::string& id, bool emissive) {
    if (Entity* e = findEntity(id)) {
        e->emissive = emissive;
        return true;
    }
    return false;
}

bool World::setVisible(const std::string& id, bool visible) {
    if (Entity* e = findEntity(id)) {
        e->visible = visible;
        return true;
    }
    return false;
}

bool World::hasEntity(const std::string& id) const {
    return m_indexById.contains(id);
}

Vec3 World::getPosition(const std::string& id) const {
    if (const Entity* e = findEntity(id)) {
        return e->position;
    }
    return {};
}

bool World::destroy(const std::string& id) {
    const auto it = m_indexById.find(id);
    if (it == m_indexById.end()) {
        return false;
    }

    const std::size_t idx = it->second;
    const std::size_t lastIdx = m_entities.size() - 1;

    if (idx != lastIdx) {
        m_entities[idx] = std::move(m_entities[lastIdx]);
        m_indexById[m_entities[idx].id] = idx;
    }

    m_entities.pop_back();
    m_indexById.erase(it);
    return true;
}

void World::clear() {
    m_entities.clear();
    m_indexById.clear();
    m_camera = {};
    m_sky = {};
    setTimeOfDay(17.5f);
}

void World::setTimeOfDay(float hours) {
    float h = std::fmod(hours, 24.0f);
    if (h < 0.0f) {
        h += 24.0f;
    }
    m_sky.timeOfDay = h;

    const float sunAngle = ((h - 6.0f) / 12.0f) * kPi;
    const float sunElev = std::sin(sunAngle);
    const float sunHoriz = std::cos(sunAngle);

    const float dirY = sunElev >= -0.05f ? std::max(sunElev, 0.14f) : 0.45f;
    const float dirX = sunHoriz * 0.82f;
    const float dirZ = 0.55f;
    const float len = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    m_sky.sunDirection = {dirX / len, dirY / len, dirZ / len};

    constexpr Color3 kNightZenith{0.02f, 0.04f, 0.10f};
    constexpr Color3 kNightHorizon{0.07f, 0.09f, 0.18f};
    constexpr Color3 kNightSun{0.35f, 0.48f, 0.78f};

    constexpr Color3 kSunsetZenith{0.13f, 0.22f, 0.46f};
    constexpr Color3 kSunsetHorizon{0.88f, 0.50f, 0.36f};
    constexpr Color3 kSunsetSun{1.00f, 0.80f, 0.54f};

    constexpr Color3 kDayZenith{0.16f, 0.44f, 0.84f};
    constexpr Color3 kDayHorizon{0.66f, 0.82f, 0.96f};
    constexpr Color3 kDaySun{1.00f, 0.97f, 0.90f};

    if (sunElev <= 0.0f) {
        const float dusk = std::clamp((sunElev + 0.25f) / 0.25f, 0.0f, 1.0f);
        m_sky.skyZenithColor = lerpColor(kNightZenith, kSunsetZenith, dusk);
        m_sky.skyHorizonColor = lerpColor(kNightHorizon, kSunsetHorizon, dusk);
        m_sky.sunColor = lerpColor(kNightSun, kSunsetSun, dusk);
        m_sky.ambientIntensity = 0.22f + 0.14f * dusk;
        m_sky.groundAmbientColor = lerpColor({0.05f, 0.06f, 0.10f}, {0.18f, 0.15f, 0.16f}, dusk);
    } else {
        const float dayFactor = std::clamp(sunElev / 0.55f, 0.0f, 1.0f);
        m_sky.skyZenithColor = lerpColor(kSunsetZenith, kDayZenith, dayFactor);
        m_sky.skyHorizonColor = lerpColor(kSunsetHorizon, kDayHorizon, dayFactor);
        m_sky.sunColor = lerpColor(kSunsetSun, kDaySun, dayFactor);
        m_sky.ambientIntensity = 0.36f + 0.14f * dayFactor;
        m_sky.groundAmbientColor = lerpColor({0.18f, 0.15f, 0.16f}, {0.22f, 0.24f, 0.26f}, dayFactor);
    }
}

} // namespace sage
