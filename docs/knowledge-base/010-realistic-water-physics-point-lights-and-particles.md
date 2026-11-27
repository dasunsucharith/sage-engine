# 010 — Realistic Water Physics, Archimedes Buoyancy, Dynamic Point Lights & 3D Particles

> **Milestone:** `v0.8.0 — Dynamic HDR Lighting, 3D Particle FX & Realistic Water Interaction`

---

## 1. Plain-English Summary

In `v0.7.0`, our open world gained animated 3D ocean waves — but because the physics ground floor was still one unbroken flat slab at `y = 0`, cars could unrealistically drive straight across the water as if it were solid ice!

In **`v0.8.0`**, SAGE makes water, night lighting, and vehicle feedback truly realistic at the engine level:
1. **No More Driving on Water:** The engine now tracks real **3D Water Volumes** with sloped sandy shorelines and sunken seabeds (`y = -1.25m`), exposes `world.sampleWater(x, z)` matching the GPU wave equation, and applies **real Archimedes Buoyancy + Water Drag** to every dynamic Jolt rigid body (crates, stunt balls, cones float & bob on the waves!). Vehicles that enter water slow sharply under hydrodynamic drag, spray 3D water splashes, sink onto the sandy seabed, and **flood & stall the engine** in deep water before being rescued to dry shore!
2. **Drivable Elevated Decks / Piers (`"type": "deck"`):** Developers can declare piers, docks, and bridges in JSON or Lua (`physics.addStaticDeck`) so vehicles drive realistically *over* water on solid wooden decks — and splash/sink if they drive off the edge!
3. **Real-Time Dynamic Point Lights (`world.setPointLight`):** Up to 16 live point lights illuminate 3D geometry, roads, and cast colored shimmering reflections across the animated water ripples (powering live car headlights, neon arches, roadside gas stations, pier lanterns, and a sweeping lighthouse beam!).
4. **3D HDR Particle System (`world.spawnParticles`):** Lightweight built-in 3D particle bursts drive water splashes, tire drift smoke, cyan nitro sparks, and crate smash impacts.

---

## 2. Technical Deep Dive

### A. CPU–GPU Synchronized Wave Sampling & Sloped Shorelines
`World::sampleWater(x, z, timeSeconds)` evaluates the exact same 4-wave displacement formula as `kWaterVertexShader` in [gl_renderer.cpp](../../engine/src/renderer/gl_renderer.cpp):
- Computes `shoreDist = min(hx - |x - cx|, hz - |z - cz|)` inside each water volume.
- Uses a smooth Hermite ramp over the first `4.2m` from shore (`smoothT = 3t^2 - 2t^3`) so the effective seabed slopes naturally from dry sand (`y = 0.0`) down to `waterSeabedY` (`-1.25m`).
- Returns `(inWater, waveSurfaceY, effectiveSeabedY, waterDepth, shoreDist)`.

### B. Jolt Physics Archimedes Buoyancy (`PhysicsWorld::step`)
In [scenes/stunt_city.json](../../scenes/stunt_city.json), the Jolt ground floor is carved into 4 dry-land slabs framing the lagoon plus a sunken seabed collider at `y = -1.25m`. Every fixed physics tick, `PhysicsWorld::step()` checks every active dynamic rigid body against `World::sampleWater`:
- Submersion fraction `s = clamp((waveY + 0.50 - bodyY) / 1.05, 0, 1)`.
- Upward buoyant acceleration `a_buoy = |g| * 1.42 * s` plus exponential linear & angular water drag.
- Dynamic crates, stunt balls, and cones splash in, bob with the moving waves, and drift realistically!

### C. Forward Multi-Light HDR Shading + Water Reflections
Both `kLitFragmentShader` and `kWaterFragmentShader` receive packed `vec4 uPointLightPosRadius[16]` and `vec4 uPointLightColorInt[16]` uniform arrays:
- Smooth inverse-square distance falloff `att = (1 - d / radius)^2` cleanly reaches `0.0` at `radius` with zero popping.
- Evaluates Blinn-Phong diffuse + metallic/roughness specular on meshes, and sharp glossy ripple reflections on animated ocean waves.

---

## 3. How SAGE Uses This

- **18 Built-in Blender `.glb` Models:** Added animated wave-rocking `sailboat.glb`, glowing neon `gas_station.glb`, and coastal `lifeguard_tower.glb` via [tools/build_v08_assets.py](../../tools/build_v08_assets.py).
- **Zero-C++ Scene Lights:** Any JSON scene can now include a `"lights": [...]` array alongside `"entities": [...]`, and any Lua script can drive moving headlights, spells, explosions, or beacons with `world.setPointLight` and `world.spawnParticles`.
