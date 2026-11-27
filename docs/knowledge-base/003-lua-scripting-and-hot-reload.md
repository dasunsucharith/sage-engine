# 003 — FiveM-Style Lua Scripting & Live Hot-Reload

**Milestone:** `v0.2 — Lua Brain`

---

## 1. Plain-English Summary

Recompiling C++ every time you want to tweak a car's top speed, move a checkpoint, or test a new mission is painfully slow. Just like Rockstar's **RAGE** separates its C++ engine from gameplay scripts — and **FiveM** organizes mods into self-contained `resources/` folders — SAGE embeds **Lua 5.4** directly inside the C++ binary.

With **Live Hot-Reload**, you can keep the game window open on one side of your screen, edit `resources/stunt_sandbox/main.lua` in your editor (or ask an AI agent to edit it), press **Ctrl+S**, and watch the game instantly reload with your new rules — zero C++ rebuilds, zero restarts.

---

## 2. Technical Deep Dive

### Embedded Stack (All Built Automatically via CMake `FetchContent`)
* **Lua 5.4.9:** Fast, tiny (~30 C files), battle-tested scripting VM compiled statically into `sage_core`.
* **`sol2` 3.5.0:** Modern C++20 header-only binding layer that maps C++ lambdas and types to clean Lua namespaces (`sage.*`, `input.*`, `world.*`, `camera.*`) with runtime safety checks (`SOL_ALL_SAFETIES_ON=1`).
* **`nlohmann/json` 3.12.0:** Parses each resource's `manifest.json` cleanly (and prepares SAGE for `scenes/*.json` in `v0.3`).

### Isolated Resource Environments
Each folder under `resources/` with a `manifest.json` gets its own `sol::environment` inheriting safe standard libraries (`base`, `math`, `string`, `table`, `utf8`) and SAGE APIs. That means two resources can both define `function onStart()` and `function onTick(dt)` without overwriting each other's globals.

### Crash-Proof Error Handling & Hot-Reload
1. Every Lua script load (`safe_script_file`) and lifecycle callback (`sol::protected_function`) runs in protected mode.
2. If a script has a syntax or runtime error, SAGE prints the exact file path and line number, pauses only that failing resource, and keeps the OS window running.
3. Twice per second (and immediately on **`F5`**), `LuaEngine::checkHotReload()` compares `std::filesystem::last_write_time` across all `.lua` and `manifest.json` files under `resources/`. When a save is detected, SAGE calls `onShutdown()`, clears the world, creates a fresh Lua state, reloads all enabled resources, and runs `onStart()`.

---

## 3. How SAGE Uses This (The Showcase Game)

Our first playable showcase game — **`resources/stunt_sandbox/main.lua` (Mini Stunt & Delivery Sandbox)** — is written **100% in Lua**:
* Arcade drift vehicle physics (forward velocity, lateral tire grip, handbrake power-slides, nitro boost, boost pads)
* City block collisions, smashable crates, glowing delivery checkpoints, and a dynamic speed-zoom follow camera
* Uses 3D `(x, y, z)` coordinates from Day 1 so Milestone **`v0.3`** can upgrade the exact same Lua game into a full 3D perspective world with `.glb` car models!
