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

// ---------------------------------------------------------------------------
// Data-driven terrain (mode 2): every number comes from the scene JSON.
// ---------------------------------------------------------------------------
float fbmOctaves(float x, float z, int octaves) {
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    float norm = 0.0f;
    const int n = std::clamp(octaves, 1, 8);
    for (int i = 0; i < n; ++i) {
        sum += amp * valueNoise2D(x * freq, z * freq);
        norm += amp;
        freq *= 2.03f;
        amp *= 0.5f;
    }
    return sum / std::max(norm, 1e-5f); // 0..1
}

// Bilinear heightmap lookup over the terrain square; returns 0..1.
float sampleHeightmap01(const TerrainSpec& spec, float halfSize, float x, float z) {
    if (spec.heightmap.empty() || spec.heightmapWidth < 2 || spec.heightmapHeight < 2) {
        return 0.0f;
    }
    const float u = std::clamp((x + halfSize) / (2.0f * halfSize), 0.0f, 1.0f) * static_cast<float>(spec.heightmapWidth - 1);
    const float v = std::clamp((z + halfSize) / (2.0f * halfSize), 0.0f, 1.0f) * static_cast<float>(spec.heightmapHeight - 1);
    const int x0 = static_cast<int>(u);
    const int z0 = static_cast<int>(v);
    const int x1 = std::min(x0 + 1, spec.heightmapWidth - 1);
    const int z1 = std::min(z0 + 1, spec.heightmapHeight - 1);
    const float fx = u - static_cast<float>(x0);
    const float fz = v - static_cast<float>(z0);
    const auto at = [&spec](int ix, int iz) {
        return spec.heightmap[static_cast<std::size_t>(iz) * static_cast<std::size_t>(spec.heightmapWidth) + static_cast<std::size_t>(ix)];
    };
    const float a = at(x0, z0) + (at(x1, z0) - at(x0, z0)) * fx;
    const float b = at(x0, z1) + (at(x1, z1) - at(x0, z1)) * fx;
    return a + (b - a) * fz;
}

// Shortest distance from (x, z) to a road polyline stored as x, z, height triplets.
// Also returns the road height at the closest point (interpolated along that segment).
float distanceToPolyline(const std::vector<float>& pts, float x, float z, float& outHeight) {
    outHeight = 0.0f;
    if (pts.size() < 3) {
        return 1.0e9f;
    }
    if (pts.size() < 6) {
        outHeight = pts[2];
        return std::hypot(x - pts[0], z - pts[1]);
    }
    float best = 1.0e9f;
    for (std::size_t i = 0; i + 5 < pts.size(); i += 3) {
        const float ax = pts[i], az = pts[i + 1], ah = pts[i + 2];
        const float bx = pts[i + 3], bz = pts[i + 4], bh = pts[i + 5];
        const float abx = bx - ax, abz = bz - az;
        const float lenSq = abx * abx + abz * abz;
        const float t = lenSq > 1e-6f ? std::clamp(((x - ax) * abx + (z - az) * abz) / lenSq, 0.0f, 1.0f) : 0.0f;
        const float d = std::hypot(x - (ax + abx * t), z - (az + abz * t));
        if (d < best) {
            best = d;
            outHeight = ah + (bh - ah) * t;
        }
    }
    return best;
}

float evaluateDataDrivenHeight(float x, float z, const TerrainSettings& cfg) {
    const TerrainSpec& spec = cfg.spec;
    float h = spec.baseHeight;

    if (!spec.heightmap.empty()) {
        h += spec.heightmapMin + sampleHeightmap01(spec, cfg.halfSize, x, z) * (spec.heightmapMax - spec.heightmapMin);
    }

    if (spec.noiseAmplitude != 0.0f) {
        float nx = x * spec.noiseFrequency + spec.noiseSeed * 17.31f;
        float nz = z * spec.noiseFrequency - spec.noiseSeed * 9.73f;
        if (spec.noiseWarp > 0.0f) {
            // Domain warping bends the noise grid so hills never look tiled or axis-aligned.
            const float wf = spec.noiseFrequency * 0.5f;
            nx += (fbmOctaves(x * wf + 1.7f, z * wf - 2.9f, 3) - 0.5f) * spec.noiseWarp * spec.noiseFrequency;
            nz += (fbmOctaves(x * wf - 3.8f, z * wf + 2.1f, 3) - 0.5f) * spec.noiseWarp * spec.noiseFrequency;
        }
        float n = fbmOctaves(nx, nz, spec.noiseOctaves);
        if (spec.noiseRidged > 0.0f) {
            const float ridge = 1.0f - std::abs(n * 2.0f - 1.0f);
            n = n + (ridge * ridge - n) * std::clamp(spec.noiseRidged, 0.0f, 1.0f);
        }
        h += n * spec.noiseAmplitude;
    }

    if (spec.island) {
        const float dx = x - spec.islandCenterX;
        const float dz = z - spec.islandCenterZ;
        float coast = spec.islandRadius;
        if (spec.islandCoastNoise > 0.0f) {
            // Sample noise along the coastline direction (bays & peninsulas), smooth around the circle.
            const float ang = std::atan2(dz, dx);
            coast += (fbmOctaves(std::cos(ang) * 2.2f + 5.0f, std::sin(ang) * 2.2f + 5.0f, 3) - 0.5f) * 2.0f * spec.islandCoastNoise;
        }
        const float r = std::sqrt(dx * dx + dz * dz);
        const float land = 1.0f - smoothstepf(coast - spec.islandFalloff, coast + spec.islandFalloff * 0.5f, r);
        h = h * land + spec.seabedHeight * (1.0f - land);
    }

    for (const TerrainFeature& f : spec.features) {
        switch (f.type) {
        case TerrainFeature::Type::Peak: {
            const float d = std::hypot(x - f.x, z - f.z);
            if (d >= f.radius) break;
            const float m = 1.0f - smoothstepf(0.0f, f.radius, d);
            h += std::pow(m, std::max(f.sharpness, 0.1f)) * f.height;
            break;
        }
        case TerrainFeature::Type::Basin: {
            const float d = std::hypot(x - f.x, z - f.z);
            if (d >= f.radius) break;
            const float m = 1.0f - smoothstepf(f.innerRadius, f.radius, d);
            h = h * (1.0f - m * std::clamp(f.strength, 0.0f, 1.0f)) - m * f.depth;
            break;
        }
        case TerrainFeature::Type::Flatten: {
            const float d = std::hypot(x - f.x, z - f.z);
            if (d >= f.radius) break;
            const float m = 1.0f - smoothstepf(f.innerRadius, f.radius, d);
            h += (f.height - h) * m;
            break;
        }
        case TerrainFeature::Type::Road: {
            const float half = f.width * 0.5f;
            float roadH = 0.0f;
            const float d = distanceToPolyline(f.points, x, z, roadH);
            if (d >= half + f.blend) break;
            const float m = 1.0f - smoothstepf(half, half + std::max(f.blend, 0.01f), d);
            h += (roadH - h) * m;
            break;
        }
        }
    }
    return h;
}

// Deterministic 3D open-world heightmap:
//   mode == 0 (StuntCity): flat highways/ring roads/plaza + rolling hills + surrounding snow mountains
//   mode == 1 (WildernessIsland): true organic domain-warped coastal island rising out of the ocean
//             with sandy coves, inland freshwater lake, rolling pine valleys, cliffs & 56m Mount Echo summit!
float evaluateTerrainHeightRaw(float x, float z, const TerrainSettings& cfg) {
    if (!cfg.enabled) {
        return 0.0f;
    }

    if (cfg.mode == TerrainSettings::kModeDataDriven) {
        return evaluateDataDrivenHeight(x, z, cfg);
    }

    // ----- Legacy built-in presets (the shipped showcase maps). New games should use mode 2. -----
    if (cfg.mode == 1) {
        // Domain-warp coordinates with multi-octave FBM so coastlines & ridges are 100% organic (zero blocks!)
        const float warpX = (fbmTerrain(x * 0.0046f + 1.7f, z * 0.0046f - 2.9f) - 0.5f) * 84.0f;
        const float warpZ = (fbmTerrain(x * 0.0046f - 3.8f, z * 0.0046f + 2.1f) - 0.5f) * 84.0f;
        const float wx = x + warpX;
        const float wz = z + warpZ;
        const float r = std::sqrt(wx * wx + wz * wz);
        const float ang = std::atan2(wz, wx);

        // Organic coastline radius with natural peninsulas, coves & bays (~235m avg radius)
        const float coastRad = 228.0f
            + std::sin(ang * 3.0f + 0.65f) * 26.0f
            + std::cos(ang * 5.0f - 1.20f) * 15.0f
            + std::sin(ang * 2.0f - 0.40f) * 18.0f;

        // Smooth shoreline transition from deep ocean seabed (-6.5m) up onto island landmass
        const float landMask = 1.0f - smoothstepf(coastRad - 68.0f, coastRad + 34.0f, r);
        const float seabed = -6.2f * (1.0f - landMask);

        const float nBroad = fbmTerrain(x * 0.0075f + 5.1f, z * 0.0075f + 3.4f);
        const float nMid   = fbmTerrain(x * 0.0170f - 2.3f, z * 0.0170f + 8.1f);
        const float nRidge = 1.0f - std::abs(fbmTerrain(x * 0.0125f + 1.1f, z * 0.0125f - 4.7f) * 2.0f - 1.0f);

        // Base gently rolling island hills (2m beaches -> 18m lush wooded ridges)
        float h = (1.6f + nBroad * 13.5f + nMid * 4.5f + (nRidge * nRidge) * 7.5f) * landMask;

        // Dramatic Northern Volcanic Peak: Mount Echo (centered at -26, 108, rising up to ~56m snow summit!)
        const float dPeak = std::sqrt((x + 26.0f) * (x + 26.0f) + (z - 108.0f) * (z - 108.0f));
        const float peakMask = 1.0f - smoothstepf(14.0f, 152.0f, dPeak);
        const float peakShape = peakMask * peakMask * (0.58f + 0.42f * nRidge) * cfg.maxHeight;
        h += peakShape;

        // Secondary Eastern Highland Ridge (centered at 112, 74, ~26m alpine overlooks)
        const float dEast = std::sqrt((x - 112.0f) * (x - 112.0f) + (z - 74.0f) * (z - 74.0f));
        const float eastMask = 1.0f - smoothstepf(12.0f, 118.0f, dEast);
        h += eastMask * eastMask * (0.55f + 0.45f * nBroad) * 20.0f;

        // Western Coastal Cliff Bluff (centered at -132, -46, ~19m rocky promontory for ancient ruins!)
        const float dWest = std::sqrt((x + 132.0f) * (x + 132.0f) + (z + 46.0f) * (z + 46.0f));
        const float westMask = 1.0f - smoothstepf(18.0f, 92.0f, dWest);
        h += westMask * (0.62f + 0.38f * nRidge) * 14.0f;

        // Carve Crescent Crash Cove Beach (South-East bay around 84, -134 -> gentle sandy shoreline!)
        const float dCove = std::sqrt((x - 96.0f) * (x - 96.0f) + (z + 148.0f) * (z + 148.0f));
        const float coveCarve = 1.0f - smoothstepf(24.0f, 94.0f, dCove);
        h = h * (1.0f - coveCarve * 0.78f) - coveCarve * 3.8f;

        // Carve Inland Freshwater Emerald Lake (Central valley around 22, -4 -> scenic drinking & swimming lake!)
        const float dLake = std::sqrt((x - 22.0f) * (x - 22.0f) + (z + 4.0f) * (z + 4.0f));
        const float lakeCarve = 1.0f - smoothstepf(22.0f, 62.0f, dLake);
        h = h * (1.0f - lakeCarve * 0.85f) - lakeCarve * 3.2f;

        // Flatten a small natural alpine plateau for Echo Ridge Radio Tower (-96, 128)
        const float dTower = std::sqrt((x + 96.0f) * (x + 96.0f) + (z - 128.0f) * (z - 128.0f));
        const float towerFlat = 1.0f - smoothstepf(10.0f, 26.0f, dTower);
        h = h + (18.5f - h) * towerFlat;

        return h + seabed;
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
        existing->interpValid = false; // Respawn = teleport, never slide from the old spot
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
        existing->interpValid = false; // Respawn = teleport, never slide from the old spot
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

bool World::teleport(const std::string& id, float x, float y, float z) {
    if (Entity* e = findEntity(id)) {
        e->position = {x, y, z};
        e->interpValid = false;
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
    // Natural terrains (island preset & data-driven) use the real terrain surface as the water bed.
    const bool islandMode = (m_terrain.enabled && m_terrain.mode != TerrainSettings::kModeStuntCityPreset);
    const float islandTerrainY = islandMode ? evaluateTerrainHeightRaw(x, z, m_terrain) : 0.0f;

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

            if (islandMode) {
                // Organic island mode: water exists wherever animated sea/lake level rises above sculpted terrain!
                if (waveY <= islandTerrainY + 0.03f) {
                    continue;
                }
                const float depth = waveY - islandTerrainY;
                if (!best.inWater || waveY > best.surfaceY) {
                    best.inWater = true;
                    best.surfaceY = waveY;
                    best.seabedY = islandTerrainY;
                    best.depth = depth;
                    best.shoreDist = depth * 3.5f;
                }
                continue;
            }

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
    const bool dataDriven = (m_terrain.mode == TerrainSettings::kModeDataDriven);
    if (!dataDriven) {
        if (x < 0.0f && z >= 0.0f) s.biomeId = 0;       // Downtown Metro
        else if (x >= 0.0f && z >= 0.0f) s.biomeId = 1; // Alpine Highlands
        else if (x >= 0.0f && z < 0.0f) s.biomeId = 2;  // Palm Coast Marina
        else s.biomeId = 3;                             // Red Rock Desert
    }

    if (!m_terrain.enabled) {
        return s;
    }

    s.height = evaluateTerrainHeightRaw(x, z, m_terrain);
    if (dataDriven) {
        // Biome = the highest layer this height has reached (matches the shader's banding).
        const auto& layers = m_terrain.spec.layers;
        for (std::size_t i = 1; i < layers.size(); ++i) {
            if (s.height >= layers[i].startHeight) {
                s.biomeId = static_cast<int>(i);
            }
        }
    }
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
    m_cameraInterpValid = false;
    m_interpApplied = false;
    m_cameraBackedUp = false;
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

// =============================================================================
// Render interpolation
// =============================================================================
// Gameplay & physics run at a fixed 60 Hz. Without interpolation a 144 Hz display
// shows each simulated position for 2-3 frames and then jumps ("judder"). Instead we
// render one tick in the past, blended by alpha = accumulator / dt between the state
// before and after the latest tick. Cost: <= 1 tick (16.7 ms) of visual latency.
namespace {

// Anything that moved further than this in ONE 60 Hz tick (= 1,500 m/s) was
// teleported by a script, so it is drawn at its new spot instead of sliding there.
constexpr float kTeleportDistance = 25.0f;

float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec3 lerpVec3(const Vec3& a, const Vec3& b, float t) {
    return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)};
}

bool sameVec3(const Vec3& a, const Vec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

float distSq(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// Euler angles in DEGREES: blend along the shortest arc so 359 -> 1 doesn't spin backwards.
float lerpAngleDeg(float a, float b, float t) {
    if (a == b) {
        return b;
    }
    const float delta = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return a + delta * t;
}

Vec3 lerpEulerDeg(const Vec3& a, const Vec3& b, float t) {
    return {lerpAngleDeg(a.x, b.x, t), lerpAngleDeg(a.y, b.y, t), lerpAngleDeg(a.z, b.z, t)};
}

// Normalized lerp along the shortest hemisphere (plenty accurate for one tick of rotation).
Quat nlerpQuat(const Quat& a, const Quat& b, float t) {
    if (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w) {
        return b;
    }
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    Quat q{
        lerpf(a.x, b.x * sign, t),
        lerpf(a.y, b.y * sign, t),
        lerpf(a.z, b.z * sign, t),
        lerpf(a.w, b.w * sign, t),
    };
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len < 1e-6f) {
        return b;
    }
    q.x /= len;
    q.y /= len;
    q.z /= len;
    q.w /= len;
    return q;
}

// Monotonic clocks (animation time) only blend forward; a reset/restart snaps.
float lerpForward(float prev, float cur, float t) {
    return cur >= prev ? lerpf(prev, cur, t) : cur;
}

Entity::TickSnapshot captureEntity(const Entity& e) {
    Entity::TickSnapshot s;
    s.position = e.position;
    s.rotation = e.rotation;
    s.rotationQuat = e.rotationQuat;
    s.scale = e.scale;
    s.useQuaternion = e.useQuaternion;
    s.animTime = e.animTime;
    s.prevAnimTime = e.prevAnimTime;
    s.blendElapsed = e.blendElapsed;
    s.ragdollBlend = e.ragdollBlend;
    s.ragdollAngle = e.ragdollAngle;
    return s;
}

void restoreEntity(Entity& e, const Entity::TickSnapshot& s) {
    e.position = s.position;
    e.rotation = s.rotation;
    e.rotationQuat = s.rotationQuat;
    e.scale = s.scale;
    e.animTime = s.animTime;
    e.prevAnimTime = s.prevAnimTime;
    e.blendElapsed = s.blendElapsed;
    e.ragdollBlend = s.ragdollBlend;
    e.ragdollAngle = s.ragdollAngle;
}

} // namespace

void World::captureTickSnapshot() {
    for (Entity& e : m_entities) {
        e.interpPrev = captureEntity(e);
        e.interpValid = true;
    }
    for (PointLight& pl : m_pointLights) {
        pl.interpPrevPosition = pl.position;
        pl.interpValid = true;
    }
    m_cameraPrev = m_camera;
    m_cameraInterpValid = true;
}

void World::applyRenderInterpolation(float alpha, bool includeCamera) {
    if (m_interpApplied) {
        return; // Never blend twice without restoring (would compound).
    }
    const float t = std::clamp(alpha, 0.0f, 1.0f);
    constexpr float kTeleportSq = kTeleportDistance * kTeleportDistance;

    m_interpBackup.resize(m_entities.size());
    for (std::size_t i = 0; i < m_entities.size(); ++i) {
        Entity& e = m_entities[i];
        m_interpBackup[i] = captureEntity(e);
        if (!e.interpValid) {
            continue;
        }
        const Entity::TickSnapshot& p = e.interpPrev;
        if (distSq(p.position, e.position) > kTeleportSq) {
            continue; // Scripted teleport: show the new spot immediately.
        }
        if (!sameVec3(p.position, e.position)) {
            e.position = lerpVec3(p.position, e.position, t);
        }
        if (p.useQuaternion == e.useQuaternion) {
            if (e.useQuaternion) {
                e.rotationQuat = nlerpQuat(p.rotationQuat, e.rotationQuat, t);
            } else if (!sameVec3(p.rotation, e.rotation)) {
                e.rotation = lerpEulerDeg(p.rotation, e.rotation, t);
            }
        }
        if (!sameVec3(p.scale, e.scale)) {
            e.scale = lerpVec3(p.scale, e.scale, t);
        }
        e.animTime = lerpForward(p.animTime, e.animTime, t);
        e.prevAnimTime = lerpForward(p.prevAnimTime, e.prevAnimTime, t);
        e.blendElapsed = lerpForward(p.blendElapsed, e.blendElapsed, t);
        if (p.ragdollBlend != e.ragdollBlend) {
            e.ragdollBlend = lerpf(p.ragdollBlend, e.ragdollBlend, t);
        }
        if (!sameVec3(p.ragdollAngle, e.ragdollAngle)) {
            e.ragdollAngle = lerpVec3(p.ragdollAngle, e.ragdollAngle, t);
        }
    }

    m_lightBackup.resize(m_pointLights.size());
    for (std::size_t i = 0; i < m_pointLights.size(); ++i) {
        PointLight& pl = m_pointLights[i];
        m_lightBackup[i] = pl.position;
        if (pl.interpValid && !sameVec3(pl.interpPrevPosition, pl.position) &&
            distSq(pl.interpPrevPosition, pl.position) <= kTeleportSq) {
            pl.position = lerpVec3(pl.interpPrevPosition, pl.position, t);
        }
    }

    m_cameraBackedUp = false;
    if (includeCamera && m_cameraInterpValid &&
        distSq(m_cameraPrev.position, m_camera.position) <= kTeleportSq) {
        m_cameraBackup = m_camera;
        m_cameraBackedUp = true;
        m_camera.position = lerpVec3(m_cameraPrev.position, m_camera.position, t);
        m_camera.target = lerpVec3(m_cameraPrev.target, m_camera.target, t);
        m_camera.fovDegrees = lerpf(m_cameraPrev.fovDegrees, m_camera.fovDegrees, t);
    }
    m_interpApplied = true;
}

void World::restoreFromRenderInterpolation() {
    if (!m_interpApplied) {
        return;
    }
    const std::size_t n = std::min(m_entities.size(), m_interpBackup.size());
    for (std::size_t i = 0; i < n; ++i) {
        restoreEntity(m_entities[i], m_interpBackup[i]);
    }
    const std::size_t nl = std::min(m_pointLights.size(), m_lightBackup.size());
    for (std::size_t i = 0; i < nl; ++i) {
        m_pointLights[i].position = m_lightBackup[i];
    }
    if (m_cameraBackedUp) {
        m_camera = m_cameraBackup;
        m_cameraBackedUp = false;
    }
    m_interpApplied = false;
}

} // namespace sage
