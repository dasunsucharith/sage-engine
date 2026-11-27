# 009 — Eliminating Road Z-Fighting, HDR Bloom Post-Processing & Animated 3D Ocean Water

> **Milestone:** `v0.7.0 — HDR Graphics, Animated 3D Water & Flicker-Free Roads`

---

## 1. Plain-English Summary

In `v0.6.0`, scaling the open world up to `320m × 320m` exposed three classic real-time 3D graphics challenges:
1. **Road Flickering (Z-Fighting):** Two flat road polygons at the exact same height (`y = 0.000`) — plus the buried physics ground slab whose top face also sat at `y = 0.000` — forced the GPU depth buffer to guess which triangle was closer on every pixel, creating shimmering striped artifacts at intersections and along highways.
2. **Flat Ocean Water:** A single static cyan quad looks lifeless compared to moving waves that reflect the sky and sparkle in the sunlight.
3. **Flat Low-Dynamic-Range (LDR) Lighting:** Without an HDR floating-point render target and Bloom pass, neon arches, headlights, pier lanterns, and sun glints on water simply clip at `1.0` instead of radiating a soft cinematic glow.

In **`v0.7.0`**, SAGE fixes all three at the engine level with a 6-pass **HDR `RGBA16F` Rendering Pipeline**, a dedicated **Animated 3D Ocean Water Shader**, **Drifting Procedural Sky Clouds**, **Procedural Surface Micro-Grain**, and **clean vertical layering + doubled depth precision**.

---

## 2. Technical Deep Dive

### A. Why Roads Flicker (and How AAA Engines Stop It Permanently)
A 24-bit perspective depth buffer stores depth non-linearly: roughly **90% of all precision is spent near the camera's `nearPlane`**, leaving tiny precision far away.
* **Fix 1 — Raise `nearPlane`:** Moving `CameraState::nearPlane` from `0.25f` to `0.50f` immediately **doubles depth precision** across the entire open world with zero visual downside for a third-person vehicle camera.
* **Fix 2 — Never Render Invisible Physics Colliders:** Adding `"visible": false` in `SceneLoader` lets `world_ground_collider` keep its solid Jolt Physics floor at `y = 0.0` without ever drawing a competing top face.
* **Fix 3 — Deterministic Vertical Layering:** Every horizontal layer now has guaranteed non-overlapping height separation:
  * Lagoon seabed: `y = -1.15m`
  * Non-overlapping biome ground tiles: `y = 0.00m`
  * North-South asphalt highways: `y = 0.04m`
  * East-West asphalt highways: `y = 0.07m`
  * Yellow road markings: `y = 0.06m` (NS) / `y = 0.09m` (EW)

### B. Multi-Wave 3D Animated Ocean Water (`EntityShape::Water`)
Instead of a flat 4-vertex quad, `"type": "water"` (or `world.spawnWater(...)`) renders a subdivided **`56 × 56` mesh grid** (`3,136` quads) in a dedicated alpha-blended pass (`Pass 3`):
* **Vertex Displacement:** Superimposes 4 directional travelling sine/cosine wave trains in world space (`vWorldPos.xz`) so adjacent ocean patches align seamlessly.
* **Analytical + Micro-Ripple Normals:** Computes the spatial derivatives `(dH/dx, dH/dz)` per fragment plus high-frequency ripple detail so specular sun glints dance across individual ripples.
* **Schlick Fresnel Reflections:** Blends shallow-to-deep water absorption with a physically inspired Fresnel reflection (`F = 0.04 + 0.96 * (1 - N·V)^4.5`) of the live dynamic sky gradient (golden at sunset, azure at noon, indigo at night).
* **Shoreline Surf Foam & Carved Cove Seabed:** Uses edge distance (`vUV`) to drive animated rolling surf foam along the shoreline while a carved-out seabed at `y = -1.15m` guarantees wave troughs never clip dry beach sand.

### C. 6-Pass HDR `RGBA16F` Bloom & Tone-Mapping Pipeline
1. **Pass 0 — Shadow Map (`2048×2048` PCF):** Texel-snapped directional depth map centered on the camera target.
2. **Pass 1 — HDR Sky Dome + Drifting FBM Clouds:** Renders atmospheric scattering, an HDR sun disk (`> 3.5` luminance), and 4-octave FBM procedural clouds projected onto an overhead dome that drift with `uTime` and pick up sunset/noon/night lighting.
3. **Pass 2 — HDR Lit 3D Geometry + Surface Micro-Grain:** Evaluates hemisphere ambient, PCF shadows, PBR-style metallic/roughness specular, Fresnel rim, and world-space procedural micro-grain on `Plane` terrain/asphalt. Emissive surfaces (neon portal arches, headlights, pier lanterns, delivery beacons) output linear HDR radiance `> 1.0`.
4. **Pass 3 — Animated 3D Ocean Water:** Alpha-blended waves with HDR sun glints (`> 2.5`) directly into the `RGBA16F` color buffer.
5. **Pass 4 — Half-Res Bloom Extract & Ping-Pong Separable Gaussian Blur:** Extracts pixels above `0.85` luminance into half-resolution `RGBA16F` ping-pong framebuffers and runs 2 horizontal/vertical Gaussian blur iterations.
6. **Pass 5 — Final ACES Filmic Composite:** Adds `bloom * uBloomIntensity`, applies a subtle cinematic edge vignette, and converts linear HDR to display sRGB via ACES Filmic tone mapping.

---

## 3. How SAGE Uses This

* **Zero C++ required for developers:** Drop `"type": "water"` into any JSON scene or call `world.spawnWater(id, x, y, z)` in Lua to get instant animated 3D ocean waves.
* **Live tuning:** Toggle HDR Bloom and adjust `Bloom Intensity` live in the **`F1` Inspector** under *Sky & Lighting*, or from Lua via `world.setBloom(enabled, intensity)`.
* **Sample World Upgrade (`scenes/stunt_city.json`):** Features zero-flicker highways, 4 glowing Neon Downtown Portal Arches, a carved Turquoise Palm Lagoon with a wooden pier & lanterns, and an endless 3D animated ocean surrounding all 4 sides of the 320m island.
