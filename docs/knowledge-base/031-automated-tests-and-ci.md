# 031 — Automated Tests & Continuous Integration

## Plain-English Summary

Until now, every check that SAGE still worked was done by hand: run the game, compare screenshots, write a one-off test program, then delete it. Now there's a permanent **test suite** that checks the engine's core rules in about 2 seconds, and **GitHub runs it automatically** on every push and pull request. If a change breaks something it guards, you find out immediately, before it reaches `main`.

## Technical Deep Dive

### The suite (`tests/`, CTest target `sage_tests`)
Built with [doctest](https://github.com/doctest/doctest) (single header, fetched by CMake like every other dependency). It links `sage_core` and uses only headless subsystems (`World`, `PhysicsWorld`, `LuaEngine`, `SceneLoader`, silent `AudioEngine`), so it needs no window or GPU.

| File | What it guards |
| :--- | :--- |
| `test_world_handles.cpp` | Generational handles (stale after destroy, survive array shuffles, slot reuse, `clear`), components not inherited by reused slots, `ComponentStore` packing |
| `test_render_interpolation.cpp` | Blending, shortest-arc yaw, teleports, camera exclusion, restore, bit-identical static values, animation snap, ragdoll component interpolation |
| `test_terrain.cpp` | Hand-computed heights for peak, plateau and a climbing road; layer sorting and `biomeId`; invalid blocks falling back to flat; legacy presets; export round trip |
| `test_lua_sandbox.cpp` | No `dofile`/`loadfile`/`io`/`os`/`require`, text-only `load`, `fileExists` path rules, runaway-script watchdog, simulation clock, per-resource saves, migration, `deleteData` |
| `test_determinism.cpp` | A scripted scenario (destroy/respawn, look-at, ragdoll, Jolt physics) run twice from scratch gives **bit-identical** positions |

### The tests were proven able to fail
A test that can't fail is worse than none. Four bugs were planted, one at a time, in the real engine code, and each was caught by the intended test:

| Planted bug | Caught by |
| :--- | :--- |
| `destroy()` doesn't bump the slot generation | handle test |
| Sandbox leaves `dofile` available | sandbox test |
| Teleport threshold raised to 25 km | interpolation test |
| Roads ignore per-point heights | terrain test + export round trip |

Gotcha found while doing this: Git restores files with CRLF line endings on Windows, so a search-and-replace that assumes `\n` silently does nothing. The mutation script normalises line endings and **refuses to continue if the bug didn't apply**, and the rebuild is checked too.

### CI (`.github/workflows/ci.yml`)
`windows-latest`: configure with CMake's default (newest) Visual Studio generator, build `sage` and `sage_tests` in Release, then run `ctest`. LFS files aren't downloaded (the tests don't need them). The game is compiled to catch build breaks, but not run, since GitHub's standard runners have no GPU. The same commands were run locally in a clean build folder before committing.

## How SAGE Uses This
* **Before committing engine changes:** `ctest --test-dir build --output-on-failure`.
* **New engine behaviour:** add a headless test, and break the code once to see the test fail.
* **Next steps:**
  * A Linux CI job, once the engine is confirmed to build there.
  * Screenshot regression tests on a GPU runner.
  * Caching FetchContent dependencies to speed up CI.
