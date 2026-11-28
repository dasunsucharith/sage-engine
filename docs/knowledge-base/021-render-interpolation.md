# 021 — Render Interpolation (Smooth Motion at Any Refresh Rate)

## Plain-English Summary

SAGE simulates the world 60 times a second, but a gaming monitor may show 144 frames a second, and with VSync off SAGE can draw 400+. Without interpolation, the screen repeats the same snapshot for 2–7 frames and then jumps to the next one. The eye reads that as **judder**, even at a high FPS.

A flip-book with 60 pages per second shown on a 144 Hz screen has the same problem. The fix is to draw the *in-between* pictures: if we are 40% of the way to the next page, show something 40% between the two pages.

## Technical Deep Dive

The classic "Fix Your Timestep" approach (Glenn Fiedler), used in some form by most engines with a fixed-rate simulation:

```text
fixedTick():   world.captureTickSnapshot()      // remember "before"
               lua.tick(dt); physics.step(dt)   // compute "after"
render():      alpha = accumulator / dt          // 0..1, how far toward the next tick
               world.applyRenderInterpolation(alpha)   // draw before + (after - before) * alpha
               renderer.renderWorld(world)
               world.restoreFromRenderInterpolation()  // put the REAL state back
               editor / Lua UI                          // always see true simulation state
```

What gets blended:
* **Entities:** position, scale, Euler rotation (along the shortest arc, so 350° to 10° doesn't spin backwards), quaternion rotation (normalized lerp on the shortest hemisphere), animation clocks (`animTime`, crossfade `prevAnimTime`/`blendElapsed`), and ragdoll tumble.
* **Point lights:** headlights stay attached to the blended car instead of lagging up to half a metre at speed.
* **Camera:** position, target and FOV. Skipped in free-fly mode and with `--cam`, because those cameras already update every frame.

Rules that prevent artefacts:
* **Spawns and respawns snap:** a new or respawned entity has no "before" state yet.
* **Teleports snap:** `world.teleport()`, or any move of more than 25 m in one tick (1,500 m/s), is drawn at the new spot immediately.
* **Camera cuts snap:** `camera.cut()` marks a hard cut.
* **Animation restarts snap:** a clip restart (time going backwards) is drawn at the new time.
* **Static objects stay exact:** an entity that didn't change is left bit-identical, so the renderer's static-prop batching and caches see the same values.

**Trade-off:** we draw up to one tick (16.7 ms) in the past. Every engine that interpolates pays this, and it's far less noticeable than judder. `--no-interp` turns it off for A/B comparison.

## How SAGE Uses This

* **Gameplay code is unaffected:** Lua `onTick`/`onRenderUI` and the F1 editor always read the true end-of-tick state. A probe resource checked this over 62 ticks and 419 UI frames and found 0 mismatches.
* **Use `world.teleport(id, x, y, z)`** for respawns, checkpoints and entering/leaving vehicles, and **`camera.cut()`** for hard camera switches. `stunt_sandbox` now uses both for car respawns and stepping out of the car or boat.
* **Cost:** not measurable. Release `--benchmark` p50 was 2.35–2.39 ms with interpolation and 2.36–2.40 ms without (RTX 5050 Laptop GPU, Stunt City).
* **Test coverage:** a standalone 13-check test of the blending math (shortest-arc yaw, teleports, restore, static bit-identity, animation restart) passes. It isn't in CMake yet; a test target is a good next step.
