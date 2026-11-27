# 002 — OS Window, Fixed-Timestep Game Loop & First GPU Triangle

**Milestone:** `v0.1 — Alive`

---

## 1. Plain-English Summary

Before a game engine can draw open worlds or run Lua scripts, it needs three vital signs — like giving a body eyes, a heartbeat, and a voice:

1. **OS Window (`SDL3`):** Opens a real desktop window, listens for your keyboard and mouse, and asks the GPU driver for a drawing canvas (an **OpenGL 3.3 Core** context).
2. **Fixed-Timestep Game Loop:** A steady heartbeat (`60 Hz` — once every `16.67 ms`). Even if your monitor runs at 144 FPS or briefly drops to 30 FPS, gameplay movement and physics advance in identical, predictable steps.
3. **First GPU Triangle:** Uploads three vertices (positions + RGB colors) into VRAM and runs two tiny GPU programs (**Vertex Shader** & **Fragment Shader**) to paint pixels on screen at full hardware speed.

---

## 2. Technical Deep Dive

### Why Fixed-Timestep (`accumulator`)?
If you move an object using raw frame delta time directly in physics calculations, a frame hitch (e.g., dragging a window for 1 second) produces a giant `dt = 1.0s` that teleports cars through walls!

SAGE uses the classic **Fixed-Timestep Accumulator**:
1. Measure real elapsed frame time (`frameDelta`), clamped to `0.25s` max to prevent a "spiral of death".
2. Add `frameDelta` into an `accumulator`.
3. While `accumulator >= 1/60s`, run `fixedTick(1/60s)` and subtract `1/60s`.
4. Render the latest state to the GPU and swap the double-buffered display (`SDL_GL_SwapWindow`).

### How the GPU Draws Our First Triangle
1. **VBO (Vertex Buffer Object):** Stores raw vertex bytes (`x, y, r, g, b`) in GPU memory.
2. **VAO (Vertex Array Object):** Describes how to slice those bytes into shader inputs (`location 0 = vec2 aPos`, `location 1 = vec3 aColor`).
3. **Vertex Shader:** Runs once per vertex — rotates, scales, offsets, and corrects for window aspect ratio so resizing the window never stretches shapes.
4. **Fragment Shader:** Runs once per covered pixel — smoothly blends vertex colors across the triangle surface and applies a gentle time-based pulse.

---

## 3. How SAGE Uses This

* **Clean Backend Separation:** `Engine` only talks to `sage::Window`, `sage::InputState`, and the abstract `sage::Renderer` interface (`engine/include/sage/`). No raw SDL3 or OpenGL headers leak into gameplay code, keeping the path clean for future Vulkan/DX12 backends.
* **Lua-Ready Input Layer:** `sage::InputState` + `sage::keyFromName("W")` are already built so Milestone `v0.2` can bind `input.isKeyDown("W")` directly to Lua.
* **AI-Native Verification (`--smoke-test`):** Running `sage.exe --smoke-test` initializes the real GPU context, compiles shaders, renders 5 real frames, prints GPU diagnostics, and exits with code `0` — letting AI coding agents automatically verify engine builds end-to-end.
