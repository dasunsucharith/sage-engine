# 007 — Real-Time Soft Shadows, glTF 2.0 Keyframe Animations & Lua Game HUDs

**Milestone:** `v0.5 — General-Purpose 3D Engine Expansion`

---

## 1. Plain-English Summary

To make SAGE a true **general-purpose game engine** where any developer can build racing games, platformers, action sandboxes, or physics games purely with JSON + Lua + Blender `.glb` assets, `v0.5` unlocks four universal capabilities:

1. **Real-Time Soft Shadows (2048×2048 Hardware PCF):** Every 3D model — your Aventador SVJ, skyscrapers, palm trees, spinning wind turbines, hovering drones, and airborne physics balls — now casts smooth, shimmer-free shadows grounded onto the city.
2. **Native glTF 2.0 Keyframe Animations:** Export any animated `.glb` from Blender (spinning turbines, bobbing quad-rotor drones, opening doors, moving platforms) and play it in SAGE with `world.playAnimation(...)` — including animated shadows!
3. **General-Purpose Jolt Physics Shapes:** Beyond boxes, Lua scripts can now spawn dynamic rolling **Spheres** (`physics.addDynamicSphere`), **Capsules** (`physics.addDynamicCapsule`), and static **3D Convex-Hull Ramps** (`physics.addStaticRamp`).
4. **Lua-Driven Game HUDs (`ui.*`):** Game scripts can now define `function onRenderUI()` to draw speedometers, RPM/Nitro bars, health bars, scoreboards, and clickable buttons directly inside the viewport without touching C++.

---

## 2. Technical Deep Dive

* **Texel-Snapped Shadow Map (`engine/src/renderer/gl_renderer.cpp`):** Fits an orthographic light frustum around the camera target, snaps its origin to exact shadow-texel increments to prevent edge crawling, applies hardware `glPolygonOffset` + Frostbite normal-offset bias, and filters with a 3×3 hardware `sampler2DShadow` PCF kernel.
* **Hierarchical glTF Node Evaluator:** For animated `.glb` models, `OpenGLRenderer` preserves parent-child `cgltf_node` hierarchies, interpolates translation/scale linearly and quaternion rotations via `glm::slerp`, and evaluates per-entity animation timestamps in both the Shadow Depth Pass and Lit 3D Pass.
* **Expanded Asset Kit (`tools/blender_build_v05_assets.py`):** Blender 5.3 headless pipeline now exports `wind_turbine.glb` (animated), `hover_drone.glb` (animated), `traffic_cone.glb`, and `stunt_ball.glb` alongside the existing city & vehicle assets.

---

## 3. How SAGE Uses This

Developers can now compose rich multi-genre 3D experiences 100% in `resources/<game>/main.lua`:
* Drop an animated `.glb` into `assets/models/` → `world.spawnModel` + `world.playAnimation`
* Build interactive sports/physics/platformer mechanics → `physics.addDynamicSphere`, `physics.addDynamicCapsule`, `physics.addStaticRamp`
* Ship a custom in-game HUD or main menu → `function onRenderUI()` using `ui.beginWindow`, `ui.progressBar`, `ui.text`, and `ui.button`!
