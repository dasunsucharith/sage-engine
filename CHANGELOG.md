# Changelog

All notable changes to SAGE Engine. Each entry links to the knowledge-base article that explains the change in depth.

## Unreleased

### For game makers & AI assistants
- **MCP bridge:** AI assistants (Claude Code, Claude Desktop, any MCP client) can work with a *running* game: list and inspect entities, spawn (with `snap_to_ground`), move, hide or destroy them, steer the camera, take screenshots they can see (from any viewpoint, without moving the game camera), read the engine log, check live performance, run sandboxed Lua and hot-reload. Start the game with `--remote` (localhost only), or let the assistant call `launch_game`. A zero-dependency Python MCP server (`tools/sage_mcp/server.py`) is registered in `.mcp.json`. See [`docs/mcp.md`](docs/mcp.md) and [KB 032](docs/knowledge-base/032-mcp-bridge.md).
- **Screenshots:** can now be saved as downscaled JPEG (`Renderer::captureScreenshotScaled`).

## v1.7.0 — Foundations: Measurement, Correctness & Hot-Reloadable Everything

v1.7 is a foundation release. It makes the engine measurable, safer, faster where measured, and easier to build games on. There are no changes to existing Lua scripts or scenes.

### For game makers
- **Data-driven terrain:** shape whole landscapes in a scene's `"terrain"` block (noise, island coastlines, peaks, lakes, plateaus, climbing roads, colour layers, snow, grass, collision) or from a 16-bit heightmap PNG, with no C++. Clear errors for every typo. See [`docs/terrain.md`](docs/terrain.md) and [KB 022](docs/knowledge-base/022-data-driven-terrain.md).
- **Shader hot reload:** shaders now live in `engine/shaders/`. Edit and save while the game runs; a shader with an error keeps the last working version running and reports the file and line ([KB 030](docs/knowledge-base/030-renderer-split-and-shader-hot-reload.md)).
- **Smooth motion above 60 Hz:** render interpolation between fixed simulation ticks. New `world.teleport()` and `camera.cut()` for instant moves ([KB 021](docs/knowledge-base/021-render-interpolation.md)).
- **Per-mod saves:** each resource saves into `saves/<resource>/`, with crash-safe writes, automatic migration of old saves, and a new `sage.deleteData()` ([KB 023](docs/knowledge-base/023-per-resource-saves.md)).
- **Safer scripts:** no `dofile`/`loadfile`/binary bytecode, project-relative `fileExists`, and a watchdog that pauses a runaway script instead of freezing the engine. `sage.getTime()` is now deterministic simulation time; the new `sage.getRealTime()` gives wall-clock time ([KB 020](docs/knowledge-base/020-honest-benchmarks-simulation-clock-and-lua-sandbox.md)).
- **New demos:** `scenes/terrain_demo.json` and `scenes/heightmap_demo.json` with the `terrain_demo` resource.

### Performance & profiling
- **`--benchmark`:** warm-up, VSync off, mean/p50/p95/p99 frame times in telemetry ([KB 020](docs/knowledge-base/020-honest-benchmarks-simulation-clock-and-lua-sandbox.md)).
- **Per-pass GPU timing** (non-blocking timer queries, per shadow cascade) and **CPU frame sections** in `--benchmark`, telemetry and the F1 editor ([KB 024](docs/knowledge-base/024-gpu-pass-timing.md), [KB 025](docs/knowledge-base/025-cpu-frame-timing.md)).
- **Opaque pass draw order for early-Z:** front-to-back with terrain last. Same image; GPU −14.8% in Stunt City and −8.2% on Echo Island at 720p ([KB 027](docs/knowledge-base/027-opaque-pass-draw-order.md)).
- **Clustered point-light culling** (exact; groundwork for more than 16 lights) ([KB 028](docs/knowledge-base/028-point-light-cost-analysis.md)).
- **New flags:** `--resolution WxH`, `--no-vsync`, `--no-interp`, `--no-light-tiles`.

### Fixes
- **Missing shadows:** objects lying along the sun direction dropped out of the mid and far cascades. Cascades now cull against the exact light-space box ([KB 026](docs/knowledge-base/026-shadow-pass-analysis.md)).
- **Determinism:** physics and water used wall-clock time; they now use the simulation clock.
- **Hot reload:** terrain edits now rebuild the mesh, grass and collider; scene export keeps the terrain block.
- **Misc:** version strings are single-sourced from CMake, and `stb` is pinned to a commit.

### Engine internals
- **Entities:** generational `EntityHandle`s (stale-safe, O(1)) and `ComponentStore<T>` components; ragdoll and look-at moved out of `Entity`, which shrank from 416 to 336 bytes ([KB 029](docs/knowledge-base/029-entity-handles-and-components.md)).
- **Renderer split:** `gl_renderer.cpp` went from 4,469 to 2,473 lines, split into `gl_model`, `gl_animation`, `gl_shaders` and `gl_profiler`.
- **Tests & CI:** an automated headless test suite (`tests/`, doctest + CTest, mutation-checked) and GitHub Actions CI on every push and pull request ([KB 031](docs/knowledge-base/031-automated-tests-and-ci.md)).
- **Git LFS:** large assets (`.glb`, `.blend`, audio, textures under `assets/`) are stored with Git LFS from this release on.

### Upgrading from v1.6
- Run `git lfs install` once, then `git lfs pull`, to get the asset files.
- Run `sage.exe` from the project folder (it needs `engine/shaders/`).
- Old saves are migrated automatically the first time each game loads them.

## v1.6.0 and earlier
See the roadmap in [`README.md`](README.md) and knowledge-base entries 001–019.
