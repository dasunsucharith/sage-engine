# AGENTS.md — AI Developer & Contributor Guide for SAGE Engine

Welcome! **SAGE (Scriptable AI Game Engine)** is designed to be **AI-Native**. Whether you are an AI coding agent helping build the C++20 core engine or helping a game developer create a game on top of SAGE, follow these rules and conventions.

---

## 1. Two Modes of Working in This Repo

1. **Engine Development (`engine/`):** Writing clean, beginner-readable, well-commented **C++20** systems (platform, renderer, ECS, physics, audio, Lua bindings).
2. **Game Development (`scenes/`, `resources/`, `assets/`):** Creating games **without touching C++** — by editing plain-text JSON scenes, adding `.glb`/`.gltf` 3D models to `assets/models/`, and writing FiveM-style **Lua** scripts inside `resources/<resource_name>/`. Landscapes (hills, islands, mountains, lakes, roads, colours, grass) belong in the scene's `"terrain"` block; see [`docs/terrain.md`](docs/terrain.md). Never hard-code map-specific coordinates in engine C++.

---

## 2. Core Architectural Rules

* **Keep C++ clean & modular:** Every subsystem lives behind a clear interface under `engine/include/sage/` so backends (e.g., OpenGL → Vulkan/DX12) can evolve without rewriting game code.
* **Plain-text first in dev:** Scenes (`scenes/*.json`), resource manifests (`manifest.json`), and configs must stay valid, well-formatted JSON so AI agents and humans can diff and edit them cleanly.
* **Always update docs alongside code:**
  * Whenever you add or change a Lua-exposed function in C++, update [`docs/lua_api.md`](docs/lua_api.md) in the same commit.
  * Whenever a major architectural lesson or system is learned/built, record it in [`docs/knowledge-base/`](docs/knowledge-base/).
* **Prefer `FetchContent` in CMake:** Dependencies should build automatically from source via CMake so contributors never have to manually hunt for `.dll` or `.lib` files.
* **No silent magic:** Log clear, actionable messages (with file path and line numbers) when a `.glb` asset, `.json` scene, or `.lua` script fails to load so AI agents can self-correct immediately.

---

## 3. FiveM-Style Lua Resource Convention

Each gameplay feature or mod lives in its own folder under `resources/`:

```text
resources/
└── my_feature/
    ├── manifest.json    # Name, version, author, and ordered list of scripts
    └── main.lua         # Gameplay logic using SAGE's Lua API
```

Read [`docs/lua_api.md`](docs/lua_api.md) before writing Lua scripts to verify which APIs are currently implemented vs. planned.

---

## 4. Knowledge Base & AI Self-Learning Protocol

Before making big architectural decisions or starting a new engine version:
1. **Read past engineering memory first:** Check [`docs/ai_learning_log.json`](docs/ai_learning_log.json), [`logs/last_session_telemetry.json`](logs/last_session_telemetry.json) (auto-generated on every engine run), and [`docs/knowledge-base/README.md`](docs/knowledge-base/README.md) so you build on proven patterns, target real measured hardware metrics, and never repeat a past mistake.
2. **Verify visually & on real GPU hardware:** Always build and run `build/sage.exe --smoke-test` (and render multi-angle Blender previews whenever authoring hero 3D models/animations) before publishing a release.
   * **Performance claims need `--benchmark`, not `--smoke-test`.** Compare versions only with the `steadyState` p50/p95/p99 from a **Release** `--benchmark` run (VSync off). Run it at least twice per scene and treat differences smaller than the run-to-run spread as noise. A 12-frame smoke-test average mostly measures the first-frame hitch.
   * **Find the bottleneck before optimizing.** Read `steadyState.cpu.sections`, not just GPU totals. Time in `present` (and in `ui`, where the OpenGL driver can also block) is the CPU *waiting for the GPU*. If that dominates, the frame is GPU/driver-bound, so optimize the costliest pass in `steadyState.gpu.passes`. If real work (`renderSubmit`, `lua`, `physics`) dominates, it's CPU-bound, so check `steadyState.cpu.renderPasses` for which pass is expensive to submit. GPU timer totals exclude the ImGui overlay and desktop compositing, so `gpu.total` < frame time does **not** by itself mean CPU-bound. The F1 editor shows both breakdowns live.
3. **Record what you learned:** After solving any tricky bug or shipping a milestone, append the concrete lesson & benchmark entry to [`docs/ai_learning_log.json`](docs/ai_learning_log.json) and write a Knowledge Base entry in [`docs/knowledge-base/`](docs/knowledge-base/) so every future AI session gets measurably better.
