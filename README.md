# SAGE Engine

**SAGE** (**S**criptable **A**I **G**ame **E**ngine) is an open-source **C++20 + Lua** 3D game engine inspired by the open-world engineering of Rockstar's **RAGE** and the developer-friendly Lua resource ecosystem of **FiveM** — built from Day 1 to be **100% AI-Native** and effortless for any developer to build any 3D game.

> **Vision:** Clone the repo, drop in static or animated `.glb` 3D models (from Blender or AI), and build complete 3D games with physics, soft shadows, audio, and custom HUDs purely in hot-reloadable Lua + JSON — or by guiding an AI coding assistant in plain English.

---

## Documentation Hub

| Guide | Who It's For |
| :--- | :--- |
| [**Getting Started & Build Guide**](docs/getting_started.md) | Install compilers, build from source in 3 commands, CLI flags, and controls |
| [**Making Your First 3D Game (Zero C++)**](docs/making_a_game.md) | Step-by-step tutorial: JSON scenes, Blender `.glb` export, Jolt physics, animations, HUDs & asset catalog |
| [**Lua API Reference**](docs/lua_api.md) | Complete reference for `sage`, `input`, `world`, `camera`, `physics`, `audio`, and `ui` namespaces |
| [**Engine Architecture**](docs/architecture.md) | C++20 subsystem map, graphics/physics/audio pipelines, and design principles |
| [**Living Knowledge Base**](docs/knowledge-base/README.md) | Plain-English + deep-dive engineering journal covering every technique we research and build |
| [**AI Contributor Guide (`AGENTS.md`)**](AGENTS.md) | Rules and conventions for AI coding agents working on or building games with SAGE |

---

## Quick Start (Build & Play in 3 Commands)

All dependencies (**SDL3**, **Lua 5.4**, **sol2**, **nlohmann/json**, **GLM**, **cgltf**, **stb_image**, **Jolt Physics 5.7**, **Dear ImGui 1.92**, **miniaudio 0.11**) are fetched and built automatically via CMake `FetchContent`:

```powershell
git clone https://github.com/dasunsucharith/sage-engine.git
cd sage-engine
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target sage
.\build\sage.exe
```

See [**Getting Started**](docs/getting_started.md) for prerequisite installation and Linux/macOS commands.

---

## Core Capabilities (`v1.3.0`)

| Pillar | Technology | What You Get Out of the Box |
| :--- | :--- | :--- |
| **Native Performance Core & AI Telemetry** | **C++20** + **CMake** + **SDL3** | Deterministic 60 Hz fixed-timestep loop, cross-platform window/input, persistent `sage.saveData`/`loadData`, **automatic `logs/last_session_telemetry.json` + `docs/ai_learning_log.json` self-learning loop** |
| **AAA 3D Terrain & Skinned PBR Graphics** | **OpenGL 3.3 Core** + **cgltf** + **GLM** | **Synchronized 160×160 Multi-Biome 3D Heightmap Terrain** (Alpine Grass, Red Rock Strata, Cliff Granite & Snow Peaks), **Volumetric Sun Shafts (God-Rays)**, **Drifting Cloud Shadows**, **glTF 2.0 GPU Skeletal Skinning & Crossfade Blending**, **FXAA 3.11**, **Cook-Torrance GGX PBR + Normal/MR Maps**, **Screen-Space Contact AO (SSAO)**, **2-Cascade 2048px PCF Soft Shadows (CSM)**, 6-pass `RGBA16F` HDR Bloom & ACES grading, dynamic rainstorms + glossy wet-puddle roads & Nearest-16 point lights |
| **AAA 3D Physics, HeightField & 4-Wheel Suspension** | **Jolt Physics 5.7** | Multithreaded rigid-body simulation with **129×129 Jolt `HeightFieldShape` 3D Terrain Collider**, 4-wheel spring-damper vehicle suspension & slope alignment, boxes, spheres, capsules, ramps, drivable piers, Archimedes water buoyancy & drag, shockwaves, raycasts & JSON auto-colliders |
| **Live World Editor, Scripting & Minimap UI** | **Lua 5.4** (`sol2`) + **Dear ImGui 1.92** | FiveM-style `resources/` folders, auto hot-reload, **`F1` AAA World Editor** (3D Terrain / God-Rays / Cloud Shadows / FXAA / SSAO / Bloom / Weather controls, spawn any `.glb`/primitive/light + **Save Scene to JSON**!), Lua `ui.*` Main/Pause Menus & **2D vector canvas** for live GTA minimap radars |
| **3D Spatial & Procedural Audio** | **miniaudio 0.11** | Lock-free 48 kHz stereo audio thread with **8-slot 3D Spatial Stereo-Panned World Emitters** (Ocean Surf, Fountain, Overhead Helicopter Rotor, Alpine Wind), **4-profile combustion + exhaust formant synth**, gear-shift pops, **4-surface procedural footsteps**, drift screech, nitro roar, impacts & shockwave booms |
| **AI-Native & Blender Ready** | Plain-text **JSON** + 33 starter `.glb` models | Ships with a playable **880m Panoramic 3D Open World** (560m 4-Biome Island framed by 46m Snow Peaks & Red Rock Canyons), **22-bone rigged 5-clip Human (`Idle`/`Walk`/`Run`/`Jump`/`Swim`)**, **4-wheel suspension Aventador SVJ & Superbike**, **drivable Marina Speedboat**, AI traffic, patrolling citizens, seagulls & live GTA minimap |

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
- [ ] **v1.4+ — Path to Surpass AAA Open-World Engines:** Clustered PBR lighting & Screen-Space Reflections (SSR), GPU-instanced swaying grass/foliage (100k+ blades), AI NPC behavior trees & active-physics ragdoll blending, multi-kilometer streaming world chunks, multiplayer sync, and Vulkan/DX12 ray-traced renderer backend.

---

## Contributing

Contributions from both humans and AI-assisted developers are warmly welcome! Read [`CONTRIBUTING.md`](CONTRIBUTING.md) and explore [`docs/knowledge-base/`](docs/knowledge-base/README.md) to see how SAGE evolves every day.

## License

Released under the [MIT License](LICENSE) — free for anyone to build, modify, and ship commercial or open-source games.
