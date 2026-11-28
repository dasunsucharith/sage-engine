# 020 — Honest Benchmarks, a Simulation Clock & a Safer Lua Sandbox

## Plain-English Summary

Three foundation fixes that make every later improvement trustworthy:

1. **Measuring speed properly.** Timing a car only over its first 10 metres, including the moment it starts the engine, says nothing about its top speed. The old `--smoke-test` did exactly that: 12 frames, one of which was a 200–250 ms cold-start hitch. The new `--benchmark` warms up first, then measures 600 frames with VSync off and reports percentiles.
2. **One clock for the whole game world.** Physics, water waves and Lua scripts used to read the wall clock. They now share a *simulation clock* that advances by exactly 1/60 s per tick, like the frames of a flip-book.
3. **Sandboxing mods.** A FiveM-style ecosystem means running other people's scripts. Scripts can no longer read arbitrary files or load unchecked bytecode, and an infinite loop now pauses only its own resource instead of freezing the engine.

## Technical Deep Dive

### Steady-state frame statistics
* `--benchmark [N]` sets `warmupFrames = 120`, `maxFrames = 120 + N + 1` (frame 0 has no previous frame to measure against) and turns VSync off.
* Telemetry now records the **raw** frame delta. The 250 ms clamp still protects the fixed-step accumulator (no "spiral of death"), but it no longer hides hitches: before, every long frame read as exactly `250.0`.
* `logs/last_session_telemetry.json` gains `buildConfig`, `runMode`, `vsync` and a `steadyState` block with mean/p50/p95/p99/max. p99 is the number players feel as stutter.

**Measured on an RTX 5050 Laptop GPU (Release, 1280×720, 2 runs each):**

| Scene | 12-frame smoke avg (old metric) | Steady-state p50 | p95 | p99 |
| :--- | :--- | :--- | :--- | :--- |
| Stunt City | 20–26 ms | 2.29–2.42 ms | 4.24–4.33 ms | 5.07–5.55 ms |
| Echo Island | 21–26 ms | 1.93–2.59 ms | 3.38–6.25 ms | 4.51–10.82 ms |

The engine is roughly **10× faster than the old numbers suggested**. Run-to-run spread is 10–25%, so any "X% faster" claim smaller than that is noise.

### Simulation clock
```text
per frame:  accumulator += frameDelta (clamped)
            while accumulator >= dt:  lua.tick(dt); physics.step(dt, simTime); simTime += dt
            renderTime = simTime + accumulator   // shaders: smooth AND in sync with physics
```
* `sage.getTime()` returns `simTime`. Two catch-up ticks in one frame now see two different times; before, the second tick saw the same value, so `getTime()`-driven objects (drones, helicopter, seagulls) stalled.
* Physics water buoyancy samples `world.sampleWater()` at `simTime`, and the GPU ocean renders at `simTime + accumulator`, so CPU and GPU waves stay in lock-step.
* `sage.getRealTime()` exposes wall-clock time for UI and profiling only.

### Lua sandbox & watchdog
* `dofile` and `loadfile` are removed. `load` is wrapped to force mode `"t"`, because PUC Lua does not verify bytecode and malformed bytecode can corrupt memory.
* `sage.fileExists` rejects absolute paths and `..` segments.
* A `LUA_MASKCOUNT` hook (every 100k VM instructions) checks a deadline armed by an RAII `WatchdogScope` around every host→Lua call. The budget is 8 s for load + `onStart` and 1 s for each tick/UI/shutdown call. Going over budget raises a normal Lua error, so the existing protected-call path pauses the resource and hot-reload recovers it.

## How SAGE Uses This
* **AI agents and humans should benchmark with `--benchmark` on a Release build**, compare p50/p95/p99, and run each scene twice (see `AGENTS.md`).
* The version string now comes from `project(VERSION ...)` in `CMakeLists.txt` via `sage/version.h`. Bump it in one place.
* `stb` is pinned to an exact commit for reproducible builds.

## Still To Do (next steps)
* ~~**Render interpolation**~~: done, see [021](021-render-interpolation.md).
* **GPU timer queries** (`GL_TIME_ELAPSED`) per pass: shadows, opaque, water, post.
* **Data-driven terrain/foliage/biomes:** world-specific shapes still live in `world.cpp` and `gl_renderer.cpp`.
* **Per-resource save namespaces**, and an inter-resource event bus.
