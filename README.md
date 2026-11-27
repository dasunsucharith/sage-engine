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

## Core Capabilities (`v0.5.0`)

| Pillar | Technology | What You Get Out of the Box |
| :--- | :--- | :--- |
| **Native Performance Core** | **C++20** + **CMake** + **SDL3** | Deterministic 60 Hz fixed-timestep loop, cross-platform window/input, standalone binary |
| **3D Graphics & Animations** | **OpenGL 3.3 Core** + **cgltf** + **GLM** | 2048×2048 texel-snapped PCF soft shadows, procedural dynamic sky (sunset/noon/night), PBR-lit shading + ACES tone mapping, drop-in `.glb` models & native glTF 2.0 keyframe animations |
| **AAA 3D Physics** | **Jolt Physics 5.7** | Multithreaded rigid-body simulation (boxes, rolling spheres, capsules, convex-hull ramps, kinematic collisions, shockwaves, raycasts) |
| **Live Scripting & HUDs** | **Lua 5.4** (`sol2`) + **Dear ImGui 1.92** | FiveM-style `resources/` folders, automatic file-watch & `F5` hot-reload, built-in `F1` Live Scene Inspector, and Lua `ui.*` custom game HUDs |
| **Real-Time Audio** | **miniaudio 0.11** | Lock-free 48 kHz stereo audio thread with procedural V12 engine synth, drift screech, nitro roar, impacts, chimes & shockwave booms |
| **AI-Native & Blender Ready** | Plain-text **JSON** + 11 starter `.glb` models | Ships with a playable **3D Lamborghini Aventador SVJ Stunt & Delivery Sandbox** + Blender 5.3 asset scripts |

---

## Project Layout

```text
sage-engine/
├── engine/                 # C++20 core engine (platform, renderer, world, physics, audio, editor, Lua VM)
│   ├── include/sage/       # Clean public C++ subsystem interfaces
│   └── src/                # Engine implementation
├── assets/models/          # 11 ready-to-use static & animated .glb 3D models
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
- [ ] **v0.6+ — RAGE-Scale Tech:** Deferred G-Buffer PBR, Multi-Cascade CSM, Jolt 4-Wheel Raycast Suspension & `CharacterVirtual`, skeletal skinning, open-world chunk streaming, and Vulkan/DX12 backend.

---

## Contributing

Contributions from both humans and AI-assisted developers are warmly welcome! Read [`CONTRIBUTING.md`](CONTRIBUTING.md) and explore [`docs/knowledge-base/`](docs/knowledge-base/README.md) to see how SAGE evolves every day.

## License

Released under the [MIT License](LICENSE) — free for anyone to build, modify, and ship commercial or open-source games.
