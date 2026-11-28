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

float smoothstepf(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / std::max(e1 - e0, 1e-5f), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float hash21Terrain(float x, float z) {
    const float h = std::sin(x * 127.1f + z * 311.7f) * 43758.5453123f;
    return h - std::floor(h);
}

float valueNoise2D(float x, float z) {
    const float ix = std::floor(x);
    const float iz = std::floor(z);
    const float fx = x - ix;
    const float fz = z - iz;
    const float ux = fx * fx * (3.0f - 2.0f * fx);
    const float uz = fz * fz * (3.0f - 2.0f * fz);
    const float a = hash21Terrain(ix, iz);
    const float b = hash21Terrain(ix + 1.0f, iz);
    const float c = hash21Terrain(ix, iz + 1.0f);
    const float d = hash21Terrain(ix + 1.0f, iz + 1.0f);
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz;
}

float fbmTerrain(float x, float z) {
    float sum = 0.0f;
    float amp = 0.52f;
    float freq = 1.0f;
    for (int i = 0; i < 4; ++i) {
        sum += amp * valueNoise2D(x * freq, z * freq);
        freq *= 2.04f;
        amp *= 0.48f;
    }
    return sum;
}

// Deterministic 3D open-world heightmap: flat on all highways, ring roads, Downtown Plaza & Marina Lagoon,
// rolling drivable hills in Alpine Highlands & Red Rock Desert, and panoramic mountain ranges framing the island!
float evaluateTerrainHeightRaw(float x, float z, const TerrainSettings& cfg) {
    if (!cfg.enabled) {
        return 0.0f;
    }

    const float ax = std::abs(x);
    const float az = std::abs(z);
    const float cheb = std::max(ax, az);

    // 1. Keep Downtown Core, all Highway corridors, Ring Roads (±64, ±148, ±236) & Marina Lagoon cleanly flat!
    float roadMask = 1.0f;
    roadMask *= smoothstepf(12.0f, 24.0f, ax);
    roadMask *= smoothstepf(12.0f, 24.0f, az);
    roadMask *= smoothstepf(11.0f, 22.0f, std::abs(cheb - 64.0f));
    roadMask *= smoothstepf(12.0f, 25.0f, std::abs(cheb - 148.0f));
    roadMask *= smoothstepf(13.0f, 26.0f, std::abs(cheb - 236.0f));

    // Keep Downtown Metro Plaza & Stadium zone flat for clean urban streets
    const float urbanMask = smoothstepf(58.0f, 88.0f, cheb);
    // Keep entire Palm Coast Marina Lagoon & Beach Pier zone flat & open to the ocean horizon!
    const float lagoonFlatX = smoothstepf(120.0f, 150.0f, x);
    const float lagoonFlatZ = smoothstepf(-120.0f, -150.0f, z);
    const float lagoonMask = 1.0f - lagoonFlatX * lagoonFlatZ;

    // 2. Rolling Drivable Interior Biome Hills (Alpine Highlands NE + Red Rock Desert SW!)
    const float nBroad = fbmTerrain(x * 0.011f + 4.3f, z * 0.011f + 1.9f);
    const float nRidge = 1.0f - std::abs(fbmTerrain(x * 0.018f - 2.1f, z * 0.018f + 6.4f) * 2.0f - 1.0f);

    float interiorH = 0.0f;
    if (x > 0.0f && z > 0.0f) {
        // NE Pine Highlands: smooth rolling emerald alpine hills up to ~11m!
        const float neBlend = smoothstepf(16.0f, 55.0f, x) * smoothstepf(16.0f, 55.0f, z);
        interiorH = (nBroad * 9.5f + nRidge * 4.5f) * neBlend * roadMask * urbanMask;
    } else if (x < 0.0f && z < 0.0f) {
        // SW Red Rock Desert: sculpted canyon dunes & terraced red-rock mesas up to ~10m!
        const float swBlend = smoothstepf(16.0f, 55.0f, -x) * smoothstepf(16.0f, 55.0f, -z);
        const float rawMesa = nBroad * 10.5f + nRidge * 3.5f;
        // Subtle mesa terrace stepping for authentic Red Rock canyon silhouette!
        const float terraced = rawMesa + std::sin(rawMesa * 1.4f) * 0.35f;
        interiorH = std::max(0.0f, terraced) * swBlend * roadMask * urbanMask;
    } else if (x < 0.0f && z > 0.0f) {
        // NW Outer Metro Parkland gentle green hills outside core
        const float nwBlend = smoothstepf(155.0f, 200.0f, cheb);
        interiorH = nBroad * 6.5f * nwBlend * roadMask;
    }

    // 3. Panoramic Surrounding Mountain Ring outside the 560m island (cheb > 258m -> 440m), leaving SE Marina Bay open!
    float outerRing = smoothstepf(256.0f, 345.0f, cheb);
    const float outerFalloff = 1.0f - smoothstepf(415.0f, cfg.halfSize, cheb);
    // Open ocean view in SE quadrant (x > 95 and z < -95)
    const float bayOpen = 1.0f - smoothstepf(85.0f, 155.0f, x) * smoothstepf(-85.0f, -155.0f, z);
    const float mtnShape = (0.35f + 0.65f * nBroad) * 0.60f + (nRidge * nRidge) * 0.55f;
    // Taller snow-capped Alpine peaks in NE & North, terraced canyon ridges in SW/West!
    const float peakScale = (z > 0.0f) ? cfg.maxHeight : (cfg.maxHeight * 0.78f);
    const float mountainH = outerRing * outerFalloff * bayOpen * mtnShape * peakScale;

    return std::max(interiorH, mountainH) * lagoonMask;
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

bool World::setLookAt(const std::string& id, Vec3 target, float weight) {
    if (Entity* e = findEntity(id)) {
        e->lookAtEnabled = true;
        e->lookAtTarget = target;
        e->lookAtWeight = std::clamp(weight, 0.0f, 1.0f);
        return true;
    }
    return false;
}

bool World::clearLookAt(const std::string& id) {
    if (Entity* e = findEntity(id)) {
        e->lookAtEnabled = false;
        e->lookAtWeight = 0.0f;
        return true;
    }
    return false;
}

bool World::triggerRagdoll(const std::string& id, Vec3 impulse, float duration) {
    if (Entity* e = findEntity(id)) {
        e->ragdollActive = true;
        e->ragdollTimer = std::max(e->ragdollTimer, std::clamp(duration, 0.6f, 8.0f));
        if (e->ragdollBlend < 0.05f) {
            e->ragdollElapsed = 0.0f;
            e->ragdollAngle = {0.0f, 0.0f, 0.0f};
        }
        e->ragdollVel.x = std::clamp(e->ragdollVel.x + impulse.x, -38.0f, 38.0f);
        e->ragdollVel.y = std::clamp(std::max(e->ragdollVel.y, 0.0f) + std::max(impulse.y, 4.5f), 2.5f, 24.0f);
        e->ragdollVel.z = std::clamp(e->ragdollVel.z + impulse.z, -38.0f, 38.0f);

        // Derive realistic tumble angular velocity from horizontal impact vector!
        const float yawRad = e->rotation.y * (kPi / 180.0f);
        const float fwdX = std::sin(yawRad);
        const float fwdZ = std::cos(yawRad);
        const float rightX = std::cos(yawRad);
        const float rightZ = -std::sin(yawRad);
        const float hitFwd = impulse.x * fwdX + impulse.z * fwdZ;
        const float hitSide = impulse.x * rightX + impulse.z * rightZ;
        e->ragdollAngVel.x = std::clamp(hitFwd * 0.45f + 2.8f, -11.0f, 11.0f);
        e->ragdollAngVel.z = std::clamp(-hitSide * 0.45f, -10.0f, 10.0f);
        e->ragdollAngVel.y = std::clamp((hitSide - hitFwd) * 0.18f, -6.0f, 6.0f);
        return true;
    }
    return false;
}

bool World::isRagdollActive(const std::string& id) const {
    if (const Entity* e = findEntity(id)) {
        return e->ragdollActive || e->ragdollBlend > 0.02f;
    }
    return false;
}

float World::getRagdollBlend(const std::string& id) const {
    if (const Entity* e = findEntity(id)) {
        return e->ragdollBlend;
    }
    return 0.0f;
}

std::size_t World::activeRagdollCount() const {
    std::size_t count = 0;
    for (const Entity& e : m_entities) {
        if (e.ragdollActive || e.ragdollBlend > 0.02f) {
            ++count;
        }
    }
    return count;
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

        // Euphoria-Style Active-Physics Ragdoll Simulation & Smooth Get-Up Recovery (v1.5.0)
        if (e.ragdollActive || e.ragdollBlend > 0.001f) {
            e.ragdollElapsed += dt;
            if (e.ragdollActive) {
                e.ragdollBlend = std::min(1.0f, e.ragdollBlend + dt * 9.5f);
                e.ragdollTimer -= dt;

                // Ballistic gravity + aerodynamic drag
                e.ragdollVel.y -= 24.0f * dt;
                const float airDrag = std::exp(-0.45f * dt);
                e.ragdollVel.x *= airDrag;
                e.ragdollVel.z *= airDrag;

                e.position.x = std::clamp(e.position.x + e.ragdollVel.x * dt, -270.0f, 270.0f);
                e.position.y += e.ragdollVel.y * dt;
                e.position.z = std::clamp(e.position.z + e.ragdollVel.z * dt, -270.0f, 270.0f);

                e.ragdollAngle.x += e.ragdollAngVel.x * dt;
                e.ragdollAngle.y += e.ragdollAngVel.y * dt;
                e.ragdollAngle.z += e.ragdollAngVel.z * dt;

                const TerrainSample ts = sampleTerrain(e.position.x, e.position.z);
                const float floorY = ts.height;
                if (e.position.y <= floorY) {
                    e.position.y = floorY;
                    if (e.ragdollVel.y < -3.2f) {
                        const float impactSpd = -e.ragdollVel.y;
                        e.ragdollVel.y = impactSpd * 0.32f;
                        e.ragdollVel.x *= 0.62f;
                        e.ragdollVel.z *= 0.62f;
                        e.ragdollAngVel.x *= 0.68f;
                        e.ragdollAngVel.z *= 0.68f;
                        spawnParticles(
                            {e.position.x, floorY + 0.15f, e.position.z},
                            {e.ragdollVel.x * 0.25f, 1.8f, e.ragdollVel.z * 0.25f},
                            {0.78f, 0.72f, 0.64f}, 5, 0.16f, 0.42f, false, 8.0f);
                    } else {
                        e.ragdollVel.y = 0.0f;
                        const float slideDrag = std::exp(-5.8f * dt);
                        e.ragdollVel.x *= slideDrag;
                        e.ragdollVel.z *= slideDrag;
                        const float angDrag = std::exp(-5.0f * dt);
                        e.ragdollAngVel.x *= angDrag;
                        e.ragdollAngVel.y *= angDrag;
                        e.ragdollAngVel.z *= angDrag;
                        // Settle torso flat on back or front (~90 deg pitch) while grounded
                        const float targetPitch = (e.ragdollAngle.x >= 0.0f) ? 1.48f : -1.48f;
                        e.ragdollAngle.x += (targetPitch - e.ragdollAngle.x) * std::min(1.0f, 5.5f * dt);
                        e.ragdollAngle.z *= std::exp(-4.0f * dt);
                    }
                }

                const float horizSpdSq = e.ragdollVel.x * e.ragdollVel.x + e.ragdollVel.z * e.ragdollVel.z;
                if (e.ragdollTimer <= 0.0f && e.position.y <= floorY + 0.08f && horizSpdSq < 1.2f) {
                    e.ragdollActive = false;
                }
            } else {
                // Smooth Euphoria get-up recovery blend (1.0 -> 0.0 over ~0.65s)
                e.ragdollBlend = std::max(0.0f, e.ragdollBlend - dt * 1.55f);
                const float settle = std::exp(-6.0f * dt);
                e.ragdollAngle.x *= settle;
                e.ragdollAngle.y *= settle;
                e.ragdollAngle.z *= settle;
                e.ragdollVel = {0.0f, 0.0f, 0.0f};
                e.ragdollAngVel = {0.0f, 0.0f, 0.0f};
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

TerrainSample World::sampleTerrain(float x, float z) const {
    TerrainSample s{};
    if (x < 0.0f && z >= 0.0f) s.biomeId = 0;       // Downtown Metro
    else if (x >= 0.0f && z >= 0.0f) s.biomeId = 1; // Alpine Highlands
    else if (x >= 0.0f && z < 0.0f) s.biomeId = 2;  // Palm Coast Marina
    else s.biomeId = 3;                             // Red Rock Desert

    if (!m_terrain.enabled) {
        return s;
    }

    s.height = evaluateTerrainHeightRaw(x, z, m_terrain);
    constexpr float kEps = 1.25f;
    const float hL = evaluateTerrainHeightRaw(x - kEps, z, m_terrain);
    const float hR = evaluateTerrainHeightRaw(x + kEps, z, m_terrain);
    const float hD = evaluateTerrainHeightRaw(x, z - kEps, m_terrain);
    const float hU = evaluateTerrainHeightRaw(x, z + kEps, m_terrain);
    const float nx = hL - hR;
    const float ny = 2.0f * kEps;
    const float nz = hD - hU;
    const float invLen = 1.0f / std::max(std::sqrt(nx * nx + ny * ny + nz * nz), 1e-5f);
    s.normal = {nx * invLen, ny * invLen, nz * invLen};
    return s;
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
