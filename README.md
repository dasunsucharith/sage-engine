# SAGE Engine

[![CI](https://github.com/dasunsucharith/sage-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/dasunsucharith/sage-engine/actions/workflows/ci.yml)

**SAGE** (**S**criptable **A**I **G**ame **E**ngine) is an open-source **C++20 + Lua** 3D game engine inspired by the open-world engineering of Rockstar's **RAGE** and the developer-friendly Lua resource ecosystem of **FiveM** — built from Day 1 to be **100% AI-Native** and effortless for any developer to build any 3D game.

> **Vision:** Clone the repo, drop in static or animated `.glb` 3D models (from Blender or AI), and build complete 3D games with physics, soft shadows, audio, and custom HUDs purely in hot-reloadable Lua + JSON — or by guiding an AI coding assistant in plain English.

---

## Documentation Hub

| Guide | Who It's For |
| :--- | :--- |
| [**Getting Started & Build Guide**](docs/getting_started.md) | Install compilers, build from source in 3 commands, CLI flags, and controls |
| [**Making Your First 3D Game (Zero C++)**](docs/making_a_game.md) | Step-by-step tutorial: JSON scenes, Blender `.glb` export, Jolt physics, animations, HUDs & asset catalog |
| [**Data-Driven Terrain**](docs/terrain.md) | Shape islands, mountains, lakes, roads, colours & grass in scene JSON or from a heightmap PNG, with no C++ |
| [**Lua API Reference**](docs/lua_api.md) | Complete reference for `sage`, `input`, `world`, `camera`, `physics`, `audio`, and `ui` namespaces |
| [**Engine Architecture**](docs/architecture.md) | C++20 subsystem map, graphics/physics/audio pipelines, and design principles |
| [**Living Knowledge Base**](docs/knowledge-base/README.md) | Plain-English + deep-dive engineering journal covering every technique we research and build |
| [**AI Contributor Guide (`AGENTS.md`)**](AGENTS.md) | Rules and conventions for AI coding agents working on or building games with SAGE |

---

## Quick Start (Build & Play in 3 Commands)

All dependencies (**SDL3**, **Lua 5.4**, **sol2**, **nlohmann/json**, **GLM**, **cgltf**, **stb_image**, **Jolt Physics 5.7**, **Dear ImGui 1.92**, **miniaudio 0.11**) are fetched and built automatically via CMake `FetchContent`:

```powershell
git lfs install   # once per machine: models, audio & textures are stored with Git LFS
git clone https://github.com/dasunsucharith/sage-engine.git
cd sage-engine
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target sage
.\build\sage.exe             # Stunt City AAA Sandbox
.\build\sage.exe --survival  # Wild Frontier: Echo Island (Open-World Survival Showcase!)
```

See [**Getting Started**](docs/getting_started.md) for prerequisite installation and Linux/macOS commands.

---

## Core Capabilities (`v1.7.0`)

| Pillar | Technology | What You Get Out of the Box |
| :--- | :--- | :--- |
| **Native Performance Core & AI Telemetry** | **C++20** + **CMake** + **SDL3** | Full **Release (`/O2`)** deterministic 60 Hz fixed-timestep loop, **static `.glb` material submesh merging** (-40.7% draw calls), **zero-allocation per-frame skeletal pose cache**, **exact per-model bounding-sphere 6-plane frustum culling**, **startup `.glb` + GPU shader pipeline pre-warming**, **automatic `glDrawElementsInstanced` static prop & rain batching**, built-in GPU screenshot capture (`--screenshot` / `--cam`), persistent `sage.saveData`/`loadData`, and **self-learning `logs/last_session_telemetry.json` + `docs/ai_learning_log.json` optimization loop** |
| **AAA 3-CSM, Soft Water & Shadowed Foliage PBR** | **OpenGL 3.3 Core** + **cgltf** + **GLM** | **Time-Sliced 3-Split Tiered Cascaded Shadow Maps (`24m / 92m / 255m` 3-CSM)**, **Depth-Buffered Soft-Shore 160×160 Water** (translucent turquoise shallows, organic surf foam & zero harsh seams), **Shadowed 4-Blade GPU-Instanced Grass** (up to **216,000 animated blades** in 1 draw call + player/car parting), **Real-Time Screen-Space Reflections (SSR)**, **Multi-Biome 3D Heightmap Terrain** (City + Organic Wilderness Island modes with micro-normals & fractured granite), **Volumetric God-Rays**, **Drifting Cloud Shadows**, **Double-Sided GGX PBR + 8× Anisotropic Filtering**, **Entity-Space Normalized Look-At IK + Euphoria Ragdoll + Skeletal Blending**, **FXAA 3.11**, **SSAO**, 6-pass `RGBA16F` HDR Bloom & Nearest-16 point lights |
| **AAA 3D Physics, Ragdolls & 4-Wheel Suspension** | **Jolt Physics 5.7** | Multithreaded rigid-body simulation with **129×129 Jolt `HeightFieldShape` 3D Terrain Collider**, **5-point slope footprint auto-grounding**, **Euphoria-style active-physics ragdoll launches, terrain bounces & smooth get-up blending**, 4-wheel spring-damper vehicle suspension & slope alignment, boxes, spheres, capsules, ramps, drivable piers, Archimedes water buoyancy & drag, shockwaves, kinetic blasts, raycasts & JSON auto-colliders |
| **Live World Editor, Profiler & Minimap UI** | **Lua 5.4** (`sol2`) + **Dear ImGui 1.92** | FiveM-style `resources/` folders, auto hot-reload, **`F1` AAA World Editor & GPU Profiler** (live Draw Calls, Instanced Batches, Saved Draw Calls, Culled Entities, Active Ragdolls, Prop Instancing / 3-CSM / Grass / SSR / Weather controls + **Save Scene to JSON**!), native `world.evalSpline` 3D spline evaluator, Lua `ui.*` Menus & **2D vector minimap radar** |
| **3D Spatial, File SFX & Streaming Audio** | **miniaudio 0.11** | Lock-free 48 kHz stereo audio thread with **streaming 2D/3D `.wav`/`.mp3` wilderness ambient loops & spatial one-shot SFX (`audio.playSfx`, `audio.playSfx3D`, `audio.setAmbientLoop`)**, **8-slot 3D Spatial Stereo-Panned World Emitters**, **4-profile combustion + exhaust formant synth**, gear-shift pops, **4-surface procedural footsteps**, drift screech, nitro roar, impacts & shockwave booms |
| **Two Complete Playable Open-World Showcases** | Plain-text **JSON** + 43 `.glb` models | 1) **Wild Frontier: Echo Island (`--survival`)** — Organic coastal island survival game with seamless **3rd-Person ↔ 1st-Person (`V`)** camera, 6 custom Blender-rigged Meshy hero models (`Survivor`, `Timber Wolf`, `Island Deer`, `Forest Raider`, `Campfire`, `Crashed Plane`), organic Douglas-fir/palm/boulder/berry foliage, real CC0 wilderness ambience/foley, crafting, base building, vitals, storms & helicopter rescue! 2) **Stunt City (`sage.exe`)** — 880m 4-biome city with 10 spline AI cruisers, 10 reactive citizens, Aventador SVJ, Superbike & Speedboat |

---

## Project Layout

```text
sage-engine/
├── engine/                 # C++20 core engine (platform, renderer, world, physics, audio, editor, Lua VM)
│   ├── include/sage/       # Clean public C++ subsystem interfaces
│   └── src/                # Engine implementation
├── assets/models/          # 32 ready-to-use static & animated .glb 3D models
├── scenes/                 # Human- & AI-readable JSON 3D worlds (hot-reloadable!)
├── resources/              # FiveM-style Lua game resources (hot-reloadable!)
├── tools/                  # Blender 5.3 procedural asset & glTF export scripts
├── docs/                   # Developer guides, Lua API reference, Architecture & Knowledge Base
├── AGENTS.md               # Guide for AI coding agents working on or with SAGE
├── CONTRIBUTING.md         # Guide for human & AI contributors
└── CMakeLists.txt          # Zero-hassle cross-platform C++20 build blueprint
```

---

## Roadmap

We follow the **Table Tennis Rule** (learned from RAGE's history): prove every core system in a real, playable 3D showcase before scaling to massive open worlds.

- [x] **v0.0 — Foundation:** Open-source repository, C++20/CMake skeleton, AI-Native docs (`AGENTS.md`), and Living Knowledge Base.
- [x] **v0.1 — Alive:** OS Window (`SDL3`), keyboard/mouse input, fixed-timestep game loop, and first GPU triangle.
- [x] **v0.2 — Lua Brain:** Embedded Lua VM (`sol2`), FiveM-style `resources/` loader, and live hot-reloading.
- [x] **v0.3 — 3D World:** Free-fly 3D camera, `.glb`/`.gltf` model loader, PBR lighting, and JSON scene loader.
- [x] **v0.4 — Editor & Physics:** Live `Dear ImGui` viewport/inspector + `Jolt Physics` (gravity, collisions, drivable car) + 3D audio (`miniaudio`).
- [x] **v0.5 — General-Purpose 3D Expansion:** Real-time soft Shadow Maps (2048px PCF), native glTF 2.0 keyframe animations, Jolt spheres/capsules/convex ramps, Blender animated asset kit, and Lua `ui.*` game HUDs.
- [x] **v0.6 — Multi-Biome Open World & Game Menus:** Declarative JSON auto-colliders on all world objects (trees, turbines, lamps, rocks, towers), Main/Pause Menu with auto-hidden Inspector (`F1`), and a 320m × 320m 4-biome world with 15 Blender `.glb` assets.
- [x] **v0.7 — HDR Bloom, Animated 3D Ocean Water & Flicker-Free Roads:** 6-pass `RGBA16F` HDR Bloom + ACES post-processing, multi-wave 3D animated ocean water shader (Fresnel reflections, sun glints & shoreline foam), drifting procedural sky clouds, procedural terrain micro-grain, and zero-flicker layered roads.
- [x] **v0.8 — Realistic Water Physics, Dynamic Point Lights & 3D Particles:** Synchronized CPU/GPU water volume queries (`world.sampleWater`), Archimedes Jolt buoyancy & drag for floating props, realistic car water submersion & engine flooding (no driving on water!), drivable pier decks, up to 16 real-time dynamic point lights reflecting on water, 3D HDR particle FX (splashes, drift smoke, nitro sparks), and 3 new Blender assets (18 total `.glb` models).
- [x] **v0.9 — 560m Grand Open World, 2-Cascade CSM, Walk/Drive & AI Traffic:** Dual `2048×2048` Cascaded Shadow Maps (32m Near + 135m Far), starry night sky, metallic clearcoat reflections, contact AO, distance-sorted nearest-16 point lights, 560m × 560m 4-biome island (3× larger!), GTA-style Exit/Enter Car (`F`) with walking/swimming 3rd-person character, 6 autonomous AI traffic cruisers, patrol helicopter, 24m spinning Ferris wheel, and 6 new Blender assets (24 total `.glb` models).
- [x] **v1.0.0 — Production AI-Native 3D Engine:** Dynamic Weather (3D falling rain + glossy wet-puddle road reflections + wet-road tire physics), Live `F1` World Builder & JSON Scene Save/Export (`SceneLoader::saveToFile`), 2D Vector Minimap/Radar Canvas API (`ui.drawRect`/`Circle`/`Line`), persistent game save API (`sage.saveData`/`loadData`), live GTA-style corner minimap radar, and 4 new landmark Blender models (28 total `.glb` models).
- [x] **v1.1.0 — GGX PBR, SSAO, Multi-Clip Hero & Land/Sea Vehicles:** Upgraded core shader to Cook-Torrance GGX Microfacet PBR with glTF Normal & Metallic-Roughness map support, sampleable HDR depth-texture Screen-Space Contact AO (SSAO) & Exposure/Vibrance grading, sculpted 4-clip Hero character (`Idle`/`Walk`/`Run`/`Swim`), drivable Racing Speedboat at the Marina Pier, selectable Cyberpunk Superbike, animated Plaza Fountains, flapping Seagulls, Supersonic Sky Jet (**32 total `.glb` models**).
- [x] **v1.2.0 — GPU Skeletal Skinning, Crossfade Animation Blending, Biomechanical 5-Clip Rig, FXAA 3.11 & Footstep Synth:** Full glTF 2.0 4-bone GPU Linear Blend Skinning (`JOINTS_0`/`WEIGHTS_0`/`inverseBindMatrices`, up to 64 bones across PBR & 2-Cascade Shadow passes), smooth bone-level TRS/quaternion `slerp` crossfade blending (`world.playAnimation(..., blendSeconds)`), Blender-sculpted seamless 22-bone human (`character_rigged.glb` + editable `human_rigged.blend`) with biomechanical 8-phase `Walk`, high-knee `Run`, airborne `Jump`, `Idle` & `Swim` clips, GPU FXAA 3.11 edge anti-aliasing, atmospheric Mie solar haze & character rim-lighting, and 4-surface procedural footstep synth (`audio.playFootstep`).
- [x] **v1.3.0 — AAA 3D Heightmap Terrain, 4-Wheel Suspension, Volumetric God-Rays, Spatial 3D Audio & Self-Learning AI Telemetry:** Synchronized 160×160 multi-biome 3D heightmap terrain (road-flattened Alpine Hills, Red Rock Mesas & 46m Snow-Capped Mountain Ranges) with auto-grounded scene models and 129×129 Jolt `HeightFieldShape` collision, 4-wheel spring-damper vehicle suspension & slope pitch/roll alignment, Screen-Space Volumetric Sun Shafts (God-Rays) & drifting FBM cloud shadows, 8-slot 3D spatial stereo-panned ambient audio emitters (`audio.setListener` / `audio.setSpatialEmitter`), and automatic session performance telemetry (`logs/last_session_telemetry.json`) + persistent AI engineering memory (`docs/ai_learning_log.json`).
- [x] **v1.4.0 — GPU-Instanced Interactive 3D Grass, Real-Time Screen-Space Reflections (SSR), Frustum Culling, Asset Preloading & Reactive AI Crowd:** Single-draw-call GPU-instanced wind-swaying 3D grass & wildflowers (~57,290 animated blades via `glDrawArraysInstanced`) with real-time player/vehicle bending (`world.setGrassInteractPos`) and vertex LOD fade, zero-extra-RT alpha-packed 12-step Screen-Space Reflections (`world.setSSR`), 6-plane Gribb-Hartmann camera frustum culling, startup `.glb` + terrain + foliage pre-warming (slashing RTX 5050 cold-start average frame time by 42%), and 6 autonomous reactive rigged 3D citizens (`WALK`/`IDLE`/`FLEE` from speeding vehicles) across all 4 biomes.
- [x] **v1.5.0 — GPU-Instanced Prop Batching, Tiered 3-CSM, Euphoria Active-Physics Ragdoll, Procedural Look-At IK & Smart Spline AI:** Automatic `glDrawElementsInstanced` batching for repeated static `.glb` props across both Shadow & PBR passes (saving **3,315 draw calls/frame**), exact per-model bounding-sphere frustum culling (**52.8% culled**), 1×1 GPU shader pipeline warmup (new record-low `179.4 ms` peak & `0.945 s` startup), 3-Split Tiered Cascaded Shadow Maps (`24m / 92m / 255m` with size-LOD culling), Euphoria-style active-physics ragdoll launches/terrain bounces/get-up blending (`world.triggerRagdoll`), real-time Head/Neck/Chest Look-At IK (`world.setLookAt`), native Catmull-Rom 3D splines (`world.evalSpline`) driving **10 smart-yielding AI cruisers** with brake lights, **10 reactive citizens**, and the **`Q` Kinetic Repulsor Blast**.
- [x] **v1.6.0 — Material-Merged Instancing, Soft Depth Water, Shadowed 4-Blade Grass, Entity-Space IK & Wild Frontier: Echo Island Survival:** Full Release (`/O2`) optimization with static `.glb` material submesh merging (-40.7% Stunt City draw calls, -34.6% session time), zero-allocation per-frame skeletal pose caching, time-sliced Far 3-CSM shadow cascade, GPU-instanced 3D rain streaks, 8× anisotropic filtering, double-sided PBR normals (`if (!gl_FrontFacing) N = -N;`), entity-space normalized quadruped/biped Look-At IK, depth-buffered 160×160 soft-shore water with organic surf foam, 54,000 shadowed 4-blade grass tufts (`216,000` blades in 1 draw call), 5-point slope footprint auto-grounding, streaming CC0 `.wav`/`.mp3` wilderness audio (`audio.playSfx`/`playSfx3D`/`setAmbientLoop`), built-in `--screenshot`/`--cam` GPU capture, and the complete **Wild Frontier: Echo Island** open-world survival game (`--survival`) with seamless 3rd/1st-person camera and 10 new Blender-rigged Meshy/wilderness `.glb` models.
- [x] **v1.7.0 — Foundations: Measurement, Correctness & Hot-Reloadable Everything:** Data-driven terrain in scene JSON (procedural or heightmap PNG), GLSL shaders as hot-reloadable files with safe fallback, render interpolation for smooth >60 Hz motion, deterministic simulation clock, hardened Lua sandbox with runaway-script watchdog, per-mod crash-safe saves, `--benchmark` with p50/p95/p99 plus per-pass GPU and CPU timing, early-Z draw ordering (−8 to −15% GPU), clustered point-light culling, a missing-shadow fix, generational entity handles and components, a split renderer, an automated test suite with GitHub Actions CI, and Git LFS for assets. See [`CHANGELOG.md`](CHANGELOG.md).
- [ ] **v1.8+ — Path to Surpass AAA Open-World Engines:** Streaming multi-kilometer open-world tile chunks, real-time volumetric clouds, dynamic global illumination (LPV/DDGI), multiplayer sync foundation, and Vulkan/DX12 hardware ray-traced renderer backend.

---

## Contributing

Contributions from both humans and AI-assisted developers are warmly welcome! Read [`CONTRIBUTING.md`](CONTRIBUTING.md) and explore [`docs/knowledge-base/`](docs/knowledge-base/README.md) to see how SAGE evolves every day.

## License

Released under the [MIT License](LICENSE) — free for anyone to build, modify, and ship commercial or open-source games.
