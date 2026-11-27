# SAGE Engine

**SAGE** (**S**criptable **A**I **G**ame **E**ngine) is a long-term, open-source **C++20 + Lua** 3D game engine inspired by the open-world engineering of Rockstar's **RAGE** and the developer-friendly Lua resource ecosystem of **FiveM** — designed from Day 1 to be **100% AI-Native**.

> **Vision:** Clone the repo, drop in your `.glb` / `.gltf` 3D models (from Blender or AI), and build entire games by writing Lua scripts — or simply giving plain-English instructions to an AI coding agent.

---

## Core Pillars

| Pillar | Technology | Why |
| :--- | :--- | :--- |
| **Native Performance Core** | **C++20** + **CMake** | Full control over CPU, GPU, memory, and multithreading (just like AAA engines). |
| **FiveM-Style Scripting** | **Lua** *(via `sol2`)* | Clean, hot-reloadable `resources/` folders for gameplay, missions, vehicles, and world events. |
| **AI-Native Design** | Plain-text **JSON** scenes + [`AGENTS.md`](AGENTS.md) | No opaque binary lock-in during development. AI agents can read, understand, and edit scenes, configs, and Lua scripts cleanly. |
| **Drop-In 3D Assets** | **glTF 2.0 / `.glb`** | Seamless pipeline from Blender or AI 3D generators straight into `assets/models/`. |
| **RAGE-Inspired Scalability** | Deferred PBR, Jolt Physics, Open-World Streaming | Starting small with a solid sandbox and growing step-by-step into a full open-world engine. |

---

## Project Layout

```text
sage-engine/
├── engine/                 # C++20 core engine (platform, renderer, physics, audio, Lua VM)
│   ├── include/sage/       # Public C++ headers
│   └── src/                # Engine implementation
├── assets/models/          # Drop .glb / .gltf 3D models here (Blender or AI-generated)
├── scenes/                 # Human- & AI-readable JSON scene files
├── resources/              # FiveM-style Lua gameplay resources (hot-reloadable!)
├── docs/
│   ├── architecture.md     # Engine layer design & graphics pipeline
│   ├── lua_api.md          # Lua scripting API reference
│   └── knowledge-base/     # Living engineering journal & RAGE/AAA research notes
├── AGENTS.md               # Guide for AI coding agents working on or with SAGE
├── CONTRIBUTING.md         # Guide for human & AI contributors
└── CMakeLists.txt          # Cross-platform C++20 build blueprint
```

---

## Roadmap

We follow the **Table Tennis Rule** (learned from RAGE's history): prove every core system in a small, fun sandbox first before scaling to massive open worlds.

- [x] **v0.0 — Foundation:** Open-source repository, C++20/CMake skeleton, AI-Native docs (`AGENTS.md`), and Living Knowledge Base.
- [x] **v0.1 — Alive:** OS Window (`SDL3`), keyboard/mouse input, fixed-timestep game loop, and first GPU triangle.
- [x] **v0.2 — Lua Brain:** Embedded Lua VM (`sol2`), FiveM-style `resources/` loader, and live hot-reloading.
- [x] **v0.3 — 3D World:** Free-fly 3D camera, `.glb`/`.gltf` model loader, PBR lighting, and JSON scene loader.
- [x] **v0.4 — Editor & Physics:** Live `Dear ImGui` viewport/inspector + `Jolt Physics` (gravity, collisions, drivable car) + 3D audio (`miniaudio`).
- [ ] **v0.5+ — RAGE-Scale Tech:** Deferred G-Buffer renderer, Cascaded Shadow Maps, open-world chunk streaming, active ragdolls, and Vulkan/DX12 backend.

---

## Using SAGE With AI

SAGE is built so you can work with an AI coding assistant from Day 1:
1. Clone this repository.
2. Point your AI assistant at [`AGENTS.md`](AGENTS.md) and [`docs/lua_api.md`](docs/lua_api.md).
3. Drop a `.glb` model into `assets/models/` and ask your AI:
   > *"Spawn `assets/models/car.glb` in `scenes/sandbox.json` and create a new Lua resource in `resources/my_car/` that lets me drive it with WASD."*

---

## Contributing

Contributions from both humans and AI-assisted developers are warmly welcome! Read [`CONTRIBUTING.md`](CONTRIBUTING.md) and explore [`docs/knowledge-base/`](docs/knowledge-base/) to understand how SAGE is being designed and built.

## License

Released under the [MIT License](LICENSE) — free for anyone to build, modify, and ship commercial or open-source games.
