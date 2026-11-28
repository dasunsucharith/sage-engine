#pragma once

#include "sage/world/component_store.h"

#include <cstdint>
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

// ---------------------------------------------------------------------------
// Entity handles
// ---------------------------------------------------------------------------
// A small, copyable ticket for an entity: `slot` is a stable index that never moves while
// the entity lives, and `generation` increases every time a slot is reused. Looking an
// entity up by handle is O(1), and a handle to a destroyed entity is detected as stale
// (World::get returns nullptr) instead of silently pointing at whatever reused the slot.
// String names (Entity::id) remain the public, human-friendly way to refer to entities.
struct EntityHandle {
    static constexpr std::uint32_t kInvalidSlot = 0xFFFFFFFFu;
    std::uint32_t slot = kInvalidSlot;
    std::uint32_t generation = 0;

    bool isNull() const { return slot == kInvalidSlot; }
    bool operator==(const EntityHandle& o) const { return slot == o.slot && generation == o.generation; }
    bool operator!=(const EntityHandle& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// Components: optional data that only some entities have (see ComponentStore)
// ---------------------------------------------------------------------------
// Procedural head/upper-body look-at IK. Present only while look-at is enabled.
struct LookAtComponent {
    Vec3 target{0.0f, 0.0f, 0.0f};
    float weight = 0.0f;
};

// Whole-body tumble ragdoll with get-up blending. Added by World::triggerRagdoll and kept
// afterwards (its recovered state is the starting point of the next ragdoll).
struct RagdollComponent {
    bool active = false;
    float blend = 0.0f;
    float timer = 0.0f;
    float elapsed = 0.0f;
    Vec3 vel{0.0f, 0.0f, 0.0f};
    Vec3 angVel{0.0f, 0.0f, 0.0f};
    Vec3 angle{0.0f, 0.0f, 0.0f};

    // Render interpolation (values at the start of the latest fixed tick / real values while blended)
    float prevBlend = 0.0f;
    Vec3 prevAngle{0.0f, 0.0f, 0.0f};
    float backupBlend = 0.0f;
    Vec3 backupAngle{0.0f, 0.0f, 0.0f};
};

// Scene entity rendered in 3D perspective (primitive, animated water, or .glb/.gltf model).
// Data every entity needs lives here; optional features live in components.
struct Entity {
    std::string id;
    EntityHandle handle; // Assigned by World; stable for the entity's lifetime
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

    // Water volume metadata (for EntityShape::Water buoyancy & realistic submersion)
    float waterSeabedY = -1.15f;

    // glTF 2.0 animation playback & real-time skeletal crossfade blending state (v1.2.0)
    std::string activeAnimation; // Empty = auto-play all animation tracks
    float animTime = 0.0f;
    float animSpeed = 1.0f;
    bool animPlaying = true;
    bool animLoop = true;

    std::string prevAnimation;
    float prevAnimTime = 0.0f;
    float prevAnimSpeed = 1.0f;
    bool prevAnimLoop = true;
    float blendDuration = 0.0f;
    float blendElapsed = 0.0f;

    // Look-at IK and ragdoll state are components: see LookAtComponent / RagdollComponent.

    // Render interpolation: state captured at the START of the latest fixed tick.
    // The renderer draws a blend between this and the current (end-of-tick) state so
    // motion is smooth on 120/144 Hz displays while gameplay stays at a fixed 60 Hz.
    struct TickSnapshot {
        Vec3 position{};
        Vec3 rotation{};
        Quat rotationQuat{};
        Vec3 scale{1.0f, 1.0f, 1.0f};
        bool useQuaternion = false;
        float animTime = 0.0f;
        float prevAnimTime = 0.0f;
        float blendElapsed = 0.0f;
    };
    TickSnapshot interpPrev{};
    bool interpValid = false; // false = just spawned/teleported -> render current state as-is
};

// Real-time dynamic point light (headlights, street lamps, neon arches, lighthouse beam!)
struct PointLight {
    std::string id;
    Vec3 position{0.0f, 2.0f, 0.0f};
    Color3 color{1.0f, 0.9f, 0.7f};
    float radius = 18.0f;
    float intensity = 2.0f;

    Vec3 interpPrevPosition{};  // Position at the start of the latest fixed tick
    bool interpValid = false;
};

// Lightweight 3D HDR particle (water splash spray, tire drift smoke, nitro sparks, debris!)
struct Particle3D {
    Vec3 position{};
    Vec3 velocity{};
    Color3 color{1.0f, 1.0f, 1.0f};
    float size = 0.25f;
    float age = 0.0f;
    float maxLife = 0.75f;
    float gravity = 14.0f;
    bool emissive = false;
};

// Exact water surface & seabed depth query at (x, z) matching the GPU 4-wave ocean shader!
struct WaterSampleResult {
    bool inWater = false;
    float surfaceY = -10.0f;
    float seabedY = 0.0f;
    float depth = 0.0f;      // Water column depth (surfaceY - effectiveSeabedY)
    float shoreDist = 0.0f;  // Horizontal distance inside the water boundary (meters)
};

// ---------------------------------------------------------------------------
// Data-driven terrain (scene JSON "terrain": { "type": "procedural" | "heightmap" })
// ---------------------------------------------------------------------------
// Everything a game needs to shape its own landscape WITHOUT touching C++:
// final height = baseHeight + heightmap + noise -> island mask -> features (in order).
// See docs/terrain.md for the JSON format.

// A sculpting operation applied on top of the base height, in the order listed.
struct TerrainFeature {
    enum class Type {
        Peak,     // Adds a smooth mountain/hill: +height at the centre, fading to 0 at radius
        Basin,    // Carves a lake bed / crater: flattens toward the centre, then lowers by depth
        Flatten,  // Levels a round plateau/plaza to an exact height (building sites, towns)
        Road      // Levels a strip along a polyline to an exact height (road beds, runways)
    };
    Type type = Type::Peak;
    float x = 0.0f;
    float z = 0.0f;
    float radius = 50.0f;        // Outer edge of the influence (Peak/Basin/Flatten)
    float innerRadius = 0.0f;    // Fully-affected core radius (Basin/Flatten); Peak uses 0
    float height = 10.0f;        // Peak: added height. Flatten/Road: target height (meters)
    float depth = 3.0f;          // Basin: how far below the flattened floor to carve
    float strength = 0.85f;      // Basin: how strongly the floor is flattened (0..1)
    float sharpness = 2.0f;      // Peak: exponent on the falloff (1 = cone-like, 2+ = rounder)
    float width = 12.0f;         // Road: fully-flat strip width
    float blend = 10.0f;         // Road: extra distance over which it blends back into the terrain
    // Road: polyline stored as x, z, height triplets. A point given as [x, z] in JSON
    // uses the feature's "height"; [x, z, h] lets the road climb (interpolated per segment).
    std::vector<float> points;
};

// Height-banded surface material (lowest band first). Colors are linear RGB 0..1,
// the same convention as entity "color" values in scene JSON.
struct TerrainLayer {
    std::string name;            // e.g. "sand", "grass" (reported by world.sampleTerrain as biome index)
    Color3 color{0.3f, 0.3f, 0.3f};
    float startHeight = 0.0f;    // Height (m) where this layer takes over from the one below
    float blend = 2.0f;          // Transition width (m)
    float roughness = 0.85f;
};

struct TerrainSpec {
    // --- Base shape ---
    float baseHeight = 0.0f;

    // Optional grayscale heightmap (8- or 16-bit PNG) stretched over [-halfSize, +halfSize].
    std::string heightmapPath;
    std::vector<float> heightmap;   // Normalized 0..1 samples, row-major (row 0 = most negative Z)
    int heightmapWidth = 0;
    int heightmapHeight = 0;
    float heightmapMin = 0.0f;      // Height (m) of pure black
    float heightmapMax = 40.0f;     // Height (m) of pure white

    // Fractal (FBM) noise for natural hills.
    float noiseAmplitude = 0.0f;    // Meters added at noise == 1
    float noiseFrequency = 0.01f;   // Features per meter (0.01 = ~100 m hills)
    int noiseOctaves = 4;
    float noiseRidged = 0.0f;       // 0 = rolling hills, 1 = sharp ridges
    float noiseWarp = 0.0f;         // Domain-warp distance (m) for organic, non-grid shapes
    float noiseSeed = 0.0f;         // Shifts the noise pattern

    // Optional island mask: land inside radius, sloping down to seabedHeight outside.
    bool island = false;
    float islandCenterX = 0.0f;
    float islandCenterZ = 0.0f;
    float islandRadius = 200.0f;
    float islandFalloff = 80.0f;    // Width of the beach/shelf transition
    float islandCoastNoise = 0.0f;  // Meters of coastline wobble (bays & peninsulas)
    float seabedHeight = -6.0f;

    std::vector<TerrainFeature> features;

    // --- Look ---
    std::vector<TerrainLayer> layers; // Max 6 used by the renderer
    Color3 cliffColor{0.22f, 0.21f, 0.20f};
    float cliffSlopeStart = 0.14f;    // 0 = flat, 1 = vertical
    float cliffSlopeEnd = 0.34f;
    bool snowEnabled = false;
    Color3 snowColor{0.84f, 0.88f, 0.94f};
    float snowStartHeight = 30.0f;
    float snowEndHeight = 38.0f;

    // --- Grass ---
    bool grassEnabled = true;
    int grassCount = 30000;           // Target tufts (max 60,000)
    float grassArea = 1.0f;           // Fraction of halfSize to scatter over
    float grassMinHeight = 0.5f;
    float grassMaxHeight = 30.0f;
    float grassMinNormalY = 0.74f;    // 1 = only flat ground
    bool grassAvoidFeatures = true;   // Skip Flatten/Road areas

    // --- Collision & mesh ---
    float collisionMinHeight = -1.0e9f; // Below this, no Jolt collision (lets props sink into carved lakes)
    float hideBelowHeight = -1.0e9f;    // Skip mesh triangles entirely below this (deep seabed under water)
};

// Synchronized CPU/GPU 3D Heightmap Terrain configuration & query result.
// mode 0 / 1 are the built-in legacy presets used by the shipped showcase games;
// mode 2 is fully described by TerrainSpec (any new game should use this).
struct TerrainSettings {
    static constexpr int kModeStuntCityPreset = 0;
    static constexpr int kModeEchoIslandPreset = 1;
    static constexpr int kModeDataDriven = 2;

    bool enabled = true;
    int mode = kModeStuntCityPreset;
    float halfSize = 440.0f;
    float maxHeight = 46.0f;
    int gridResolution = 160;
    TerrainSpec spec;
    // Bumped whenever any terrain setting changes, so the renderer & physics know
    // to rebuild the mesh, grass and collider (also after scene hot-reload).
    std::uint32_t revision = 1;
    std::string sourceJson; // Original scene "terrain" block, written back by scene export
};

struct TerrainSample {
    float height = 0.0f;
    Vec3 normal{0.0f, 1.0f, 0.0f};
    // Data-driven terrain: index of the dominant height layer (0 = lowest).
    // Legacy presets: 0=Downtown, 1=Alpine Highlands, 2=Palm Coast, 3=Red Rock Desert quadrants.
    int biomeId = 0;
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
    bool ssaoEnabled = true;
    bool fxaaEnabled = true;
    bool godRaysEnabled = true;
    float godRayIntensity = 0.42f;
    bool cloudShadowsEnabled = true;
    bool ssrEnabled = true;
    float ssrIntensity = 0.62f;
    bool grassEnabled = true;
    bool instancedPropsEnabled = true;
    Vec3 grassInteractPos{0.0f, 0.0f, 0.0f};
    float exposure = 1.08f;
    float rainIntensity = 0.0f;
    float wetness = 0.0f;
};

struct CameraState {
    Vec3 position{0.0f, 8.0f, -18.0f};
    Vec3 target{0.0f, 1.5f, 0.0f};
    float fovDegrees = 60.0f;
    float nearPlane = 0.50f;
    float farPlane = 1250.0f;
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

    // --- Handles (O(1), stale-safe). Names stay the public API; handles are for hot paths. ---
    EntityHandle handleOf(const std::string& id) const;     // Null handle if no such entity
    Entity* get(EntityHandle h);                             // nullptr if stale or null
    const Entity* get(EntityHandle h) const;
    bool isValid(EntityHandle h) const { return get(h) != nullptr; }
    bool setPosition(EntityHandle h, float x, float y, float z);
    bool setRotationQuat(EntityHandle h, float x, float y, float z, float w);

    // --- Components (nullptr when the entity doesn't have one) ---
    const LookAtComponent* lookAt(EntityHandle h) const;
    const RagdollComponent* ragdoll(EntityHandle h) const;

    bool setPosition(const std::string& id, float x, float y, float z);
    // Moves an entity WITHOUT render interpolation (respawns, checkpoints, cut-scenes),
    // so it appears at the new spot instantly instead of sliding there over one tick.
    bool teleport(const std::string& id, float x, float y, float z);
    bool setRotation(const std::string& id, float pitch, float yaw, float roll);
    bool setRotationQuat(const std::string& id, float x, float y, float z, float w);
    bool setScale(const std::string& id, float x, float y, float z);
    bool setColor(const std::string& id, float r, float g, float b);
    bool setEmissive(const std::string& id, bool emissive);
    bool setVisible(const std::string& id, bool visible);
    bool setWaterSeabed(const std::string& id, float seabedY);

    bool playAnimation(const std::string& id, const std::string& clipName = "", bool loop = true, float speed = 1.0f, float blendSeconds = 0.18f);
    bool stopAnimation(const std::string& id);
    bool setAnimationSpeed(const std::string& id, float speed);
    bool setLookAt(const std::string& id, Vec3 target, float weight = 0.85f);
    bool clearLookAt(const std::string& id);
    bool triggerRagdoll(const std::string& id, Vec3 impulse, float duration = 2.4f);
    bool isRagdollActive(const std::string& id) const;
    float getRagdollBlend(const std::string& id) const;
    std::size_t activeRagdollCount() const;
    void updateAnimations(float dt);

    // Dynamic point lights (up to 16 active in HDR shader & water specular reflections)
    void setPointLight(const std::string& id, Vec3 pos, Color3 color, float radius = 18.0f, float intensity = 2.0f);
    bool removePointLight(const std::string& id);
    const std::vector<PointLight>& pointLights() const { return m_pointLights; }

    // 3D particle bursts (water splashes, tire drift smoke, nitro sparks, impact debris)
    void spawnParticles(
        Vec3 pos,
        Vec3 baseVel,
        Color3 color,
        int count,
        float size = 0.25f,
        float maxLife = 0.7f,
        bool emissive = false,
        float gravity = 14.0f);
    const std::vector<Particle3D>& particles() const { return m_particles; }

    // Exact water volume + animated wave height query (matches GPU ocean vertex shader!)
    WaterSampleResult sampleWater(float x, float z, float timeSeconds) const;

    // Synchronized 3D rolling heightmap terrain query (matches GPU 160x160 terrain mesh & Jolt HeightField!)
    TerrainSample sampleTerrain(float x, float z) const;
    TerrainSettings& terrain() { return m_terrain; }
    const TerrainSettings& terrain() const { return m_terrain; }

    bool hasEntity(const std::string& id) const;
    Vec3 getPosition(const std::string& id) const;
    Entity* findEntity(const std::string& id);
    const Entity* findEntity(const std::string& id) const;
    bool destroy(const std::string& id);
    void clear();

    void setTimeOfDay(float hours);

    // --- Render interpolation (fixed 60 Hz simulation -> smooth any-Hz rendering) ---
    // Call at the start of every fixed tick: remembers the pre-tick state.
    void captureTickSnapshot();
    // Temporarily replaces transforms/camera/lights with prev + (current - prev) * alpha
    // (alpha in [0,1] = accumulator / fixedTimeStep). MUST be paired with
    // restoreFromRenderInterpolation() before anything else reads or edits the world.
    void applyRenderInterpolation(float alpha, bool includeCamera);
    void restoreFromRenderInterpolation();
    // Skips camera interpolation for the current tick (camera cuts).
    void snapCamera() { m_cameraInterpValid = false; }

    std::vector<Entity>& entitiesMutable() { return m_entities; }
    const std::vector<Entity>& entities() const { return m_entities; }
    std::size_t entityCount() const { return m_entities.size(); }

    CameraState& camera() { return m_camera; }
    const CameraState& camera() const { return m_camera; }

    SkySettings& sky() { return m_sky; }
    const SkySettings& sky() const { return m_sky; }

private:
    EntityHandle allocateSlot(std::size_t denseIndex);

    std::vector<Entity> m_entities;                            // Dense, iteration order
    std::unordered_map<std::string, std::uint32_t> m_slotById; // name -> stable slot
    std::vector<std::uint32_t> m_slotToDense;                  // slot -> index in m_entities
    std::vector<std::uint32_t> m_slotGeneration;               // slot -> current generation
    std::vector<std::uint32_t> m_freeSlots;
    ComponentStore<LookAtComponent> m_lookAts;
    ComponentStore<RagdollComponent> m_ragdolls;
    std::vector<PointLight> m_pointLights;
    std::unordered_map<std::string, std::size_t> m_lightIndexById;
    std::vector<Particle3D> m_particles;
    std::uint32_t m_rngState = 0x5A6E0800u;
    CameraState m_camera{};
    SkySettings m_sky{};
    TerrainSettings m_terrain{};

    // Render interpolation bookkeeping
    CameraState m_cameraPrev{};
    bool m_cameraInterpValid = false;
    bool m_interpApplied = false;
    std::vector<Entity::TickSnapshot> m_interpBackup;      // Real end-of-tick entity state while blended
    std::vector<Vec3> m_lightBackup;
    CameraState m_cameraBackup{};
    bool m_cameraBackedUp = false;
};

} // namespace sage
