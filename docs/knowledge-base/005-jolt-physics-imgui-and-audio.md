# 005 — Jolt Physics, Dear ImGui Live Editor & miniaudio Synth

**Milestone:** `v0.4 — Editor, Physics & Audio`

---

## 1. Plain-English Summary

A 3D world starts feeling truly **alive** when objects have real weight, you can tweak the engine live while playing, and you can *hear* the engine roar as you drift through a city:

1. **Jolt Physics 5.7:** Multithreaded AAA 3D rigid-body physics (the same open-source solver used in *Horizon Forbidden West* and *Death Stranding 2*). Stacked crates rest stably on top of each other, tumble end-over-end in full 3D quaternions when your Aventador SVJ slams through them, and bounce realistically off skyscrapers and arena walls.
2. **Dear ImGui 1.92 Live Inspector (`F1`):** A real-time viewport overlay showing FPS graphs, GPU/model/physics counts, live Time-of-Day & Gravity sliders (*Earth / Moon / Zero-G*), audio controls, and a live 3D Scene Entity Inspector.
3. **miniaudio 0.11 Procedural Vehicle & SFX Synth:** Lock-free real-time 48 kHz stereo audio thread synthesizing a Lamborghini V12 harmonic engine note (driven by 6-gear RPM & throttle), tire drift screech, nitro jet roar, crate smash impacts, shockwave booms, and delivery chimes — zero external `.wav` dependencies required!

---

## 2. Technical Deep Dive

### Quaternion Sync & Kinematic Momentum Transfer (`engine/src/physics/physics_world.cpp`)
* Dynamic Jolt bodies update `Entity::position` and `Entity::rotationQuat` (`useQuaternion = true`) every fixed `1/60s` step, eliminating Euler gimbal lock when crates tumble and flip in 3D.
* The player's Aventador SVJ uses a **Kinematic Jolt Box** updated via `BodyInterface::MoveKinematic(pos, quat, dt)`. Because Jolt computes velocity from the kinematic target delta, colliding with a 3-tier crate stack transfers true linear and angular momentum into every crate.
* All Jolt bodies are cleared cleanly inside `LuaEngine::reloadAll()` before re-running `onStart()`, making physics setups **100% hot-reloadable**.

### Lock-Free Real-Time Audio (`engine/src/audio/audio_engine.cpp`)
* The audio callback runs on a high-priority OS audio thread (`WASAPI` on Windows). To prevent audio crackles or deadlocks, `AudioEngine` never locks a mutex inside `dataCallback` — it communicates strictly through `std::atomic` floats/booleans and smooths RPM/drift/boost changes per sample.

### Input Capture Isolation (`engine/src/editor/editor_ui.cpp`)
* `EditorUI` hooks into `Window::setRawEventCallback` so Dear ImGui receives SDL3 events cleanly, and `LuaEngine` checks `WantCaptureKeyboard` / `WantCaptureMouse` so dragging sliders in the Inspector never accidentally drives the car or spins the camera.

---

## 3. How SAGE Uses This

* **Zero-C++ Gameplay Expansion:** Lua scripts can now spawn stacked rigid bodies (`physics.addDynamicBox`), trigger shockwaves (`physics.applyExplosion`), run 3D raycasts (`physics.raycast`), and drive live soundscapes (`audio.*`) without recompiling C++.
* **Interactive Sandbox Tooling:** Press **`F1`** to tweak gravity to Moon (`-5.0`) or Zero-G (`0.0`), press **`B`** to drop a fresh crate pyramid in front of your Aventador SVJ, and press **`X`** to launch every crate into orbit!
