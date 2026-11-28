# 024 — GPU Pass Timing & the CPU-Bound Discovery

## Plain-English Summary

The benchmark told us *how long* a frame took, but not *why*. A frame is a relay race between the CPU (deciding what to draw) and the GPU (drawing it), and the GPU part has several legs: shadows, the main scene, grass, water, post-processing. SAGE now times each leg on the GPU itself, so we know which one to speed up, or whether the GPU is even the slow runner.

## Technical Deep Dive

**Timer queries.** OpenGL 3.3 has `GL_TIME_ELAPSED` queries: wrap GPU work in `glBeginQuery` / `glEndQuery` and the GPU reports how many nanoseconds it spent on it.

**Never stall.** Asking for a result right away makes the CPU wait for the GPU to finish, which ruins the very timing we want. `GpuPassProfiler` keeps a **ring of 4 frames** of queries (16 passes each). When a ring slot comes around again (~3 frames later), it checks `GL_QUERY_RESULT_AVAILABLE` on that frame's last query. If the GPU isn't done, the sample is dropped rather than waited for. In practice nothing is dropped: 601 of 601 frames were collected.

**No nesting.** `GL_TIME_ELAPSED` queries can't overlap, so `begin("opaque")` automatically ends the previous pass. An RAII `GpuProfilerFrameScope` closes the frame even if `renderWorld` returns early. The timer functions are loaded as *optional*, so a driver without them just disables profiling.

**Passes timed:** `setup`, `shadows`, `sky`, `opaque`, `grass`, `water`, `particles`, `rain`, `post`. The ImGui overlay and buffer swap aren't included.

## What it revealed

Release build, RTX 5050 Laptop GPU, 1280×720, VSync off:

| Scene | CPU frame p50 | GPU total p50 | Biggest GPU passes (mean) |
| :--- | :--- | :--- | :--- |
| Stunt City | 2.71 ms | 1.76 ms | opaque 1.18, shadows 0.41, post 0.17, grass 0.11 |
| Echo Island | 2.27 ms | 1.13 ms | opaque 0.56, shadows 0.30, grass 0.20, post 0.18 |

**SAGE is CPU-bound.** The GPU finishes its work in roughly half to two-thirds of the frame and then waits. Faster shaders would barely change the frame rate. The CPU-side work is what matters: about 1,900 draw calls per frame in Stunt City, per-entity uniform setup, Lua and physics.

## How SAGE Uses This
* `--benchmark` prints a GPU line, and the telemetry gains `steadyState.gpu` (`total` plus per-pass mean/p50/p95/max).
* The F1 editor shows a live, smoothed "GPU Passes" breakdown with bars.
* `AGENTS.md` now says to compare GPU total with CPU frame time before choosing what to optimize.

## Next steps suggested by the data
1. Cut draw calls in the opaque and shadow passes: wider instancing, merging static scenery, fewer state changes.
2. Add CPU-side timing (Lua tick, physics step, render submission) to split the CPU frame the same way.
3. Later: multithreaded command building, or a modern API backend (SDL_GPU / Vulkan) with cheaper draw submission.
