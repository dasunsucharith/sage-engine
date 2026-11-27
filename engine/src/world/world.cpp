#include "sage/world/world.h"

#include <algorithm>
#include <cmath>

namespace sage {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::size_t kMaxParticles = 1024;

Color3 lerpColor(const Color3& a, const Color3& b, float t) {
    const float k = std::clamp(t, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * k,
        a.g + (b.g - a.g) * k,
        a.b + (b.b - a.b) * k,
    };
}

float nextRand01(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

// Exact same 4-wave ocean displacement formula as gl_renderer.cpp kWaterVertexShader!
float evaluateOceanWaveOffset(float x, float z, float t) {
    const float w1 = std::sin(x * 0.16f + z * 0.11f + t * 1.85f) * 0.22f;
    const float w2 = std::sin(x * -0.24f + z * 0.19f + t * 2.35f) * 0.14f;
    const float w3 = std::cos(x * 0.38f - z * 0.31f + t * 3.10f) * 0.07f;
    const float w4 = std::sin((x + z) * 0.55f - t * 2.75f) * 0.04f;
    return w1 + w2 + w3 + w4;
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
        existing->waterSeabedY = position.y - 0.85f;
        return existing->id;
    }

    Entity entity;
    entity.id = id;
    entity.shape = shape;
    entity.position = position;
    entity.scale = scale;
    entity.color = color;
    entity.waterSeabedY = position.y - 0.85f;

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

bool World::setWaterSeabed(const std::string& id, float seabedY) {
    if (Entity* e = findEntity(id)) {
        e->waterSeabedY = seabedY;
        return true;
    }
    return false;
}

bool World::playAnimation(const std::string& id, const std::string& clipName, bool loop, float speed, float blendSeconds) {
    if (Entity* e = findEntity(id)) {
        const bool clipChanged = (e->activeAnimation != clipName);
        if (clipChanged && blendSeconds > 0.005f && !e->activeAnimation.empty()) {
            e->prevAnimation = e->activeAnimation;
            e->prevAnimTime = e->animTime;
            e->prevAnimSpeed = e->animSpeed;
            e->prevAnimLoop = e->animLoop;
            e->blendDuration = std::clamp(blendSeconds, 0.02f, 1.5f);
            e->blendElapsed = 0.0f;
            e->animTime = 0.0f;
        } else if (clipChanged) {
            e->prevAnimation.clear();
            e->blendDuration = 0.0f;
            e->blendElapsed = 0.0f;
            e->animTime = 0.0f;
        }
        e->activeAnimation = clipName;
        e->animLoop = loop;
        e->animSpeed = speed;
        e->animPlaying = true;
        return true;
    }
    return false;
}

bool World::stopAnimation(const std::string& id) {
    if (Entity* e = findEntity(id)) {
        e->animPlaying = false;
        return true;
    }
    return false;
}

bool World::setAnimationSpeed(const std::string& id, float speed) {
    if (Entity* e = findEntity(id)) {
        e->animSpeed = speed;
        return true;
    }
    return false;
}

void World::updateAnimations(float dt) {
    for (Entity& e : m_entities) {
        if (e.shape == EntityShape::Model && e.animPlaying) {
            e.animTime += dt * e.animSpeed;
            if (e.blendDuration > 0.0f) {
                e.prevAnimTime += dt * e.prevAnimSpeed;
                e.blendElapsed += dt;
                if (e.blendElapsed >= e.blendDuration) {
                    e.blendDuration = 0.0f;
                    e.blendElapsed = 0.0f;
                    e.prevAnimation.clear();
                }
            }
        }
    }

    // Integrate & expire active 3D particles in-place
    std::size_t writeIdx = 0;
    for (std::size_t i = 0; i < m_particles.size(); ++i) {
        Particle3D& p = m_particles[i];
        p.age += dt;
        if (p.age >= p.maxLife) {
            continue;
        }
        p.velocity.y -= p.gravity * dt;
        const float drag = std::exp(-1.4f * dt);
        p.velocity.x *= drag;
        p.velocity.z *= drag;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        if (writeIdx != i) {
            m_particles[writeIdx] = p;
        }
        ++writeIdx;
    }
    m_particles.resize(writeIdx);
}

void World::setPointLight(const std::string& id, Vec3 pos, Color3 color, float radius, float intensity) {
    const float r = std::max(radius, 0.5f);
    const float inten = std::max(intensity, 0.0f);
    if (const auto it = m_lightIndexById.find(id); it != m_lightIndexById.end()) {
        PointLight& pl = m_pointLights[it->second];
        pl.position = pos;
        pl.color = color;
        pl.radius = r;
        pl.intensity = inten;
        return;
    }
    m_lightIndexById[id] = m_pointLights.size();
    m_pointLights.push_back({id, pos, color, r, inten});
}

bool World::removePointLight(const std::string& id) {
    const auto it = m_lightIndexById.find(id);
    if (it == m_lightIndexById.end()) {
        return false;
    }
    const std::size_t idx = it->second;
    const std::size_t lastIdx = m_pointLights.size() - 1;
    if (idx != lastIdx) {
        m_pointLights[idx] = std::move(m_pointLights[lastIdx]);
        m_lightIndexById[m_pointLights[idx].id] = idx;
    }
    m_pointLights.pop_back();
    m_lightIndexById.erase(it);
    return true;
}

void World::spawnParticles(
    Vec3 pos,
    Vec3 baseVel,
    Color3 color,
    int count,
    float size,
    float maxLife,
    bool emissive,
    float gravity) {
    const int n = std::clamp(count, 1, 64);
    for (int i = 0; i < n; ++i) {
        if (m_particles.size() >= kMaxParticles) {
            m_particles.erase(m_particles.begin());
        }
        const float rx = (nextRand01(m_rngState) - 0.5f) * 2.0f;
        const float ry = nextRand01(m_rngState);
        const float rz = (nextRand01(m_rngState) - 0.5f) * 2.0f;
        const float lifeScale = 0.72f + 0.55f * nextRand01(m_rngState);
        const float sizeScale = 0.75f + 0.55f * nextRand01(m_rngState);

        Particle3D p;
        p.position = {
            pos.x + rx * size * 1.4f,
            pos.y + ry * size * 0.6f,
            pos.z + rz * size * 1.4f,
        };
        p.velocity = {
            baseVel.x + rx * 3.8f,
            baseVel.y + ry * 2.6f,
            baseVel.z + rz * 3.8f,
        };
        p.color = color;
        p.size = std::clamp(size * sizeScale, 0.04f, 3.0f);
        p.age = 0.0f;
        p.maxLife = std::clamp(maxLife * lifeScale, 0.08f, 5.0f);
        p.gravity = gravity;
        p.emissive = emissive;
        m_particles.push_back(p);
    }
}

WaterSampleResult World::sampleWater(float x, float z, float timeSeconds) const {
    WaterSampleResult best{};
    for (const Entity& e : m_entities) {
        if (!e.visible || e.shape != EntityShape::Water) {
            continue;
        }
        const float hx = std::abs(e.scale.x) * 0.5f;
        const float hz = std::abs(e.scale.z) * 0.5f;
        const float dx = std::abs(x - e.position.x);
        const float dz = std::abs(z - e.position.z);
        if (dx <= hx && dz <= hz) {
            const float shoreDist = std::min(hx - dx, hz - dz);
            const float waveY = e.position.y + evaluateOceanWaveOffset(x, z, timeSeconds);

            // Smooth natural sloping beach bank over the first 4.2m from the shoreline!
            const float slopeT = std::clamp(shoreDist / 4.2f, 0.0f, 1.0f);
            const float smoothT = slopeT * slopeT * (3.0f - 2.0f * slopeT);
            const float effectiveSeabed = e.waterSeabedY * smoothT;
            const float depth = std::max(0.0f, waveY - effectiveSeabed);

            if (!best.inWater || waveY > best.surfaceY) {
                best.inWater = true;
                best.surfaceY = waveY;
                best.seabedY = effectiveSeabed;
                best.depth = depth;
                best.shoreDist = shoreDist;
            }
        }
    }
    return best;
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
    m_pointLights.clear();
    m_lightIndexById.clear();
    m_particles.clear();
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
