# AGENTS.md — AI Developer & Contributor Guide for SAGE Engine

Welcome! **SAGE (Scriptable AI Game Engine)** is designed to be **AI-Native**. Whether you are an AI coding agent helping build the C++20 core engine or helping a game developer create a game on top of SAGE, follow these rules and conventions.

---

## 1. Two Modes of Working in This Repo

1. **Engine Development (`engine/`):** Writing clean, beginner-readable, well-commented **C++20** systems (platform, renderer, ECS, physics, audio, Lua bindings).
2. **Game Development (`scenes/`, `resources/`, `assets/`):** Creating games **without touching C++** — by editing plain-text JSON scenes, adding `.glb`/`.gltf` 3D models to `assets/models/`, and writing FiveM-style **Lua** scripts inside `resources/<resource_name>/`.

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

## 4. Knowledge Base

Before making big architectural decisions, read [`docs/knowledge-base/README.md`](docs/knowledge-base/README.md) to stay aligned with SAGE's RAGE-inspired design philosophy and current milestone.
