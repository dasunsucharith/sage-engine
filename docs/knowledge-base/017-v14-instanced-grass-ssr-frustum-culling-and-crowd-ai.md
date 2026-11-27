# 017 — GPU-Instanced Interactive 3D Grass, Screen-Space Reflections (SSR), Frustum Culling, Preloading & Reactive AI Crowd (`v1.4.0`)

## 1. Plain-English Summary

Imagine driving an Aventador through an Alpine valley in a rainstorm:
* **GPU-Instanced 3D Grass & Wildflowers:** Instead of painting flat green ground or issuing 57,000 separate CPU draw commands (which would crawl at 2 FPS), we upload **one tiny crossed-blade clump** and a compact list of 28,645 meadow positions once at startup. The GPU stamps out **57,290 wind-swaying 3D blades in a single draw call**, and whenever you walk or drive through a meadow, the blades physically bend away from your tires and boots!
* **Real-Time Screen-Space Reflections (SSR):** When rain wets the asphalt or sunlight hits glossy car paint, the post-processing shader shoots tiny reflection rays across the already-rendered HDR frame + depth buffer — mirroring skyscrapers, streetlamps, trees, and neon rings directly onto wet streets without rendering the entire scene twice.
* **Frustum Culling & Startup Pre-Warming:** Why waste GPU cycles drawing buildings behind the camera, or freeze Frame 1 while reading 31 `.glb` files from disk? SAGE now extracts the camera's 6 viewing planes each frame to skip off-screen entities (~43% culled on startup!) and preloads all `.glb` models, terrain, and grass before Frame 1 begins.
* **Autonomous Reactive AI Crowd:** Cities feel dead when NPCs stand still. Six rigged 3D citizens now patrol sidewalks and meadows across all four biomes, pause to look around, and **sprint away in panic (`FLEE`)** with smooth skeletal crossfade blending whenever you speed toward them.

---

## 2. Technical Deep Dive

### A. Single-Draw-Call GPU-Instanced Foliage (`glDrawArraysInstanced`)
* **Loader Upgrade:** Added `glVertexAttribDivisor` and `glDrawArraysInstanced` to `engine/src/renderer/gl_functions.{h,cpp}`.
* **Geometry & Instance Buffer:** Each grass clump uses a crossed 2-blade tapered quad mesh (`12` vertices). A per-instance VBO (`divisor = 1`) stores `vec4(x, y, z, scale)` + `vec4(yaw, colorSeed, flowerChance, slope)` generated deterministically from `Terrain::sample(x, z)`.
* **Placement Rules:** Scatters across Alpine Highlands & NW Parkland meadows while automatically excluding highways, ring roads, Downtown plazas, Lagoon water, steep granite cliffs (`normal.y < 0.78`), and snow peaks (`height > 17m`). Result on `stunt_city.json`: **28,645 clumps = 57,290 3D blades in 1 draw call**.
* **Vertex Shader Simulation (`kGrassVertexShader`):**
  1. **Distance LOD Fade:** Smoothly scales distant blades to zero between `105m` and `155m` so distant meadows blend seamlessly without popping.
  2. **Rolling Gusts:** Dual-frequency directional wind waves bend blade tips (`tipWeight = v.y^2`) while roots stay anchored to terrain.
  3. **Interactive Parting (`uInteractPos`):** Pushes grass tips radially outward and downward within a `3.2m` radius around the player or active vehicle.
* **Fragment Shader (`kGrassFragmentShader`):** Root-to-tip ambient occlusion darkening, backlit solar translucency, drifting FBM cloud shadows, subtle gold/lavender/rose wildflowers, and `FragColor.a = 0.0` (so matte foliage never triggers false SSR reflections).

### B. Zero-Extra-RT Screen-Space Reflections (SSR)
* **Alpha-Channel Reflectivity Packing:** Rather than adding a second G-buffer MRT pass, `kLitFragmentShader` computes physical surface reflectivity from `(1 - roughness)`, `metallic`, and `uWetness` and stores it in `FragColor.a` of our existing `RGBA16F` HDR buffer (sky and grass write `0.0`).
* **12-Step Linear Ray March (`kCompositeFrag`):**
  1. Reconstructs world position from `uDepthTex` + inverse `ViewProjection`.
  2. Estimates world-space surface normal via screen-space derivatives `normalize(cross(dFdx(worldPos), dFdy(worldPos)))`.
  3. Computes reflection vector `R = reflect(V, N)` and marches 12 progressive steps in world/clip space against `uDepthTex`.
  4. On a valid depth intersection (within thickness tolerance), samples `uHdrTex`, applies grazing-angle Fresnel + soft screen-edge vignette fade, and blends seamlessly into the HDR color before Bloom, ACES tone-mapping, and FXAA 3.11.

### C. 6-Plane Gribb-Hartmann Camera Frustum Culling & Startup Preload
* **Frustum Extraction:** Extracts the 6 normalized clip planes `(Left, Right, Bottom, Top, Near, Far)` directly from the `View * Projection` matrix each frame.
* **Bounding Sphere Rejection:** Computes a conservative world-space bounding sphere per entity (accounting for model scale and non-uniform bounds) and skips Pass-2 opaque rendering for off-screen entities, recording `m_lastCulledCount` for live Editor stats and AI telemetry.
* **Startup Pre-Warming (`Renderer::preloadModels`):** Invoked immediately after Lua initialization (and on `F5` hot-reload) to decode & upload all referenced `.glb` meshes, build the 3D terrain VBO, and generate the GPU-instanced grass field before Frame 1.

### D. Reactive GTA-Style Citizen AI (Lua State Machine)
* Six rigged `character_rigged.glb` citizens in `resources/stunt_sandbox/main.lua` patrol district waypoints (`Downtown`, `Alpine`, `Boardwalk`, `Desert`), grounded accurately via `world.sampleTerrain(x, z)`.
* **3-State Machine:**
  * `WALK`: Patrols between district bounds using `Walk` clip (`blend = 0.22s`).
  * `IDLE`: Pauses naturally at intervals using `Idle` clip (`blend = 0.24s`).
  * `FLEE`: Triggers when the player's driven vehicle enters within `11.5m` at speed `> 5 m/s`, immediately pivoting away from the vehicle trajectory and sprinting (`Run` clip, `speed = 1.35x`, `blend = 0.14s`) until safe.

---

## 3. How SAGE Uses This & Verified RTX 5050 Benchmark

Measured directly via `logs/last_session_telemetry.json` on **NVIDIA GeForce RTX 5050 Laptop GPU** (`build\sage.exe --smoke-test`):

| Metric | `v1.3.0` | `v1.4.0` | Delta |
| :--- | :---: | :---: | :---: |
| Entities | 374 | **378** | +4 reactive rigged NPCs |
| GPU Grass Blades | 0 | **57,290** (28,645 clumps, 1 draw call) | New |
| Screen-Space Reflections | Off | **On (12-step SSR)** | New |
| Frustum-Culled Entities | 0 | **162 / 378 (42.9%)** | New |
| 12-Frame Cold-Start Avg | 49.8 ms | **28.8 ms** | **-42.1% faster** |
| Cold-Start Peak Frame | 226.7 ms | **190.9 ms** | **-15.8% lower** |

**Lua APIs Added:**
* `world.setSSR(enabled [, intensity])`
* `world.setGrassEnabled(enabled)`
* `world.setGrassInteractPos(x, y, z)`

**Next Self-Learning Target (`v1.5.0`):** Batch repeated static props (`pine_tree`, `palm_tree`, `street_lamp`, `cactus`) via `glDrawArraysInstanced` across both shadow and main passes, and add a dummy warmup draw during `preloadModels` to eliminate NVIDIA first-frame shader pipeline compilation stalls.
