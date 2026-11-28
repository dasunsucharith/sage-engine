# 025 — CPU Frame Timing & Why SAGE Is Actually GPU/Driver-Bound

## Plain-English Summary

Entry 024 timed the GPU and concluded "the GPU is idle a lot, so the CPU must be the slow one". That was a guess, and it was wrong. Now the CPU side is timed too, section by section, and it shows the CPU spends most of each frame **waiting in line** for the GPU to accept the next frame. Picture a cashier (CPU) who finishes each customer quickly, then stands waiting for the packer (GPU) to clear the counter. Making the cashier faster wouldn't help.

## Technical Deep Dive

**`CpuProfiler`** (`engine/include/sage/profiling/profiler.h`) is a tiny header-only profiler:
* `CpuScope s(profiler, "physics");` times a block using `std::chrono::steady_clock` (~20 ns per call).
* Scopes nest, and each section reports **exclusive** time (its own time minus its children's), so the sections add up to the frame. Anything not covered is reported as `other`.
* A section that runs several times per frame (e.g. two fixed ticks) is summed.

**Engine sections:** `events`, `hotReload`, `lua`, `physics`, `animation`, `interpolation`, `renderSubmit`, `luaUI`, `ui` (ImGui editor/HUD), `present` (buffer swap), `other`.

**Renderer submission per pass:** the same pass markers that drive the GPU timer queries also record CPU time, so `renderSubmit` splits into `shadows`, `opaque`, `grass`, `post`, … This works even when GPU timer queries aren't available.

**Per-frame means:** samples are stored per frame for every known section (0 when it didn't run), so a section that runs every 30th frame, like `hotReload`, reports its true per-frame average.

## What it revealed

Release, RTX 5050 Laptop GPU, 1280×720, VSync off, mean ms per frame:

| Section | Stunt City | Echo Island |
| :--- | :--- | :--- |
| renderSubmit | 0.90 (shadows 0.49, opaque 0.25) | 0.12 |
| lua | 0.06 | 0.01 |
| physics | 0.03 | 0.01 |
| luaUI + ui logic | ~0.1 | ~0.1 |
| **waiting for GPU/driver** (`present`, or GL calls in `ui`) | **1.61** (present) | **~1.7** (inside ui) |
| frame | 2.78 | 1.97 |

* **The wait dominates.** The driver blocks the CPU once its queue of in-flight frames is full. Which call blocks varies: it's `SwapBuffers` in Stunt City and ImGui's buffer uploads on Echo Island. So `present` and the `ui` spike are the same thing: GPU wait.
* **GPU timer totals (1.2–1.7 ms) are below frame time** because they don't include the ImGui overlay, clears or Windows desktop compositing.
* **Conclusion:** SAGE is GPU/driver-bound here. The biggest GPU passes (opaque, then shadows) are the right optimization targets. On the CPU, shadow submission (3 cascades) is the costliest item, and Lua and physics are negligible.

## How SAGE Uses This
* `--benchmark` prints `[CPU] mean per frame` and `[CPU] renderSubmit by pass` lines. Telemetry gains `steadyState.cpu.sections` and `steadyState.cpu.renderPasses`.
* The F1 editor shows a live "CPU Frame" breakdown next to "GPU Passes".
* `AGENTS.md` explains how to read the two together before optimizing.

## Lesson
Measure both sides before naming a bottleneck. A gap between GPU time and frame time can be *waiting*, not *working*.
