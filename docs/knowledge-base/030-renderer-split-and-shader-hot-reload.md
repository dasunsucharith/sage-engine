# 030 — Renderer Split & Shader Hot-Reload

## Plain-English Summary

The renderer used to be one 4,500-line file, with every shader written as a text string inside the C++. Changing one colour in a shader meant recompiling the engine and restarting the game. Now:
* **Shaders are normal files** in `engine/shaders/` (`lit.frag`, `water.frag`, …). Edit one, save it, and the running game recompiles it within a fraction of a second.
* **Mistakes don't break the game.** A typo prints the file and line of the error, and the game keeps drawing with the last working shaders.
* **The C++ is split into focused files**, so people and AI agents can find things.

The picture and performance are unchanged.

## Technical Deep Dive

### Files
| File | What's in it |
| :--- | :--- |
| `engine/shaders/*.vert` / `*.frag` | The 14 GLSL sources (shadow, sky, lit, grass, water, fullscreen, bloom extract/blur, composite) |
| `gl_shaders.h/.cpp` | `ShaderLibrary`: finds the shader folder, reads, compiles and links with file-labelled errors, and polls timestamps |
| `gl_model.h/.cpp` | GPU mesh/model types (`GpuSubMesh`, `GpuModel`, skins, clips), primitive meshes, glTF textures, entity matrices |
| `gl_animation.h/.cpp` | Clip sampling with crossfades, node world matrices, look-at IK and the ragdoll layer |
| `gl_profiler.h` | `GpuPassProfiler` (non-blocking GPU timer queries plus per-pass CPU timing) |
| `gl_renderer.cpp` | The `OpenGLRenderer` class: passes, culling, light clusters, model loading (4,469 → 2,473 lines) |

The moved code lives in the internal namespace `sage::render`. The `cgltf` and `stb` single-header implementations are still compiled once, in `gl_renderer.cpp`.

### How hot reload works
* **Startup:** `ShaderLibrary::findShaderDirectory` looks for `lit.frag` under `<project>/engine/shaders`, then `./engine/shaders`, then `../engine/shaders`. If it's not found, startup fails with every path it tried. (`Engine::initialize` now gives the renderer the project root *before* `initialize()`.)
* **Polling:** every 0.5 s (`beginFrame`), timestamps of all shader files read so far are compared. On a change, **all** programs are rebuilt into temporaries and swapped in only if every one succeeds; the old programs are deleted and the 125 uniform locations re-queried (`fetchUniformLocations`). On failure, nothing is swapped. Timestamps are re-armed so a broken file doesn't retrigger every poll.
* **Error line numbers:** sources are passed to the driver verbatim (the leading blank line of the old C++ raw strings was dropped), so error lines like NVIDIA's `0(359)` are line numbers in the file.

## Verification
* **Same image:** screenshot diffs against `main` from 4 cameras across all scenes were at the noise floor (0.04–0.12% versus 0.03–0.11% between two runs of `main`).
* **Same behaviour:** the per-tick entity behaviour log (entry 029) still matches the pre-refactor baseline on every tick, and all four scenes pass smoke tests.
* **Same speed:** interleaved Release A/B, p50 2.11–2.18 ms new versus 2.09–2.14 ms on `main`; GPU 1.45–1.50 ms on both.
* **Hot reload, live test on a running game:** a harmless edit to `lit.frag` reloaded in 106 ms. An injected typo printed `lit.frag … 0(359) : error C1503: undefined variable "uSunDirTypo"` (line 359 is the edited line) and the game kept running on the previous shaders.

## How SAGE Uses This
* **Shader work:** run the game, edit `engine/shaders/*.frag`, save, and see it live.
* **Shipping a build:** copy `engine/shaders/` alongside `sage.exe` (it's searched relative to the project root and the working directory).
* **Next steps:** split the remaining `OpenGLRenderer` passes (shadows, opaque, water, post) into their own files; add `#include` support for shared GLSL (noise, PBR, shadows are duplicated across shaders today).
