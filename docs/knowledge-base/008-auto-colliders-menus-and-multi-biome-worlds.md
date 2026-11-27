# 008 — Declarative JSON Auto-Colliders, Game Menus & Multi-Biome Worlds

**Milestone:** `v0.6 — Multi-Biome Open World & Game Menu System`

---

## 1. Plain-English Summary

Before `v0.6`, every building or ramp collider had to be duplicated in Lua, decorative props like trees, wind turbines, and street lamps could be driven straight through, and the engine launched directly into the debug inspector.

In **`v0.6.0`**, SAGE takes a huge step toward shipping complete 3D games:
1. **Declarative JSON Auto-Colliders:** Any object placed in `scenes/*.json` can include a `"collider"` block (`"box"`, `"cylinder"`, or `"ramp"`). `SceneLoader` automatically registers it in Jolt Physics **and** in SAGE's fast obstacle/ramp query layer — so **every skyscraper, warehouse, palm tree, pine tree, wind turbine mast, street lamp pole, desert rock mesa, and coastal lighthouse is automatically solid** for both your vehicle and tumbling physics props!
2. **Main Menu, Pause Menu & Clean Fullscreen Gameplay:** The developer inspector (`F1`) now starts hidden during play, `Esc` is handed to Lua for in-game Pause Menus, and new `ui.*` helpers (`ui.beginCenteredWindow`, `ui.getViewportSize`, `ui.textCentered`, `ui.setFontScale`, `ui.sameLine`, `sage.quit`) let any developer build polished game menus purely in Lua.
3. **320m × 320m 4-Biome Open World:** Expanded the showcase nearly 4× in size with **Downtown Metro**, **Pine Highlands & Wind Ridge**, **Palm Coast Beach & Animated Lighthouse**, and **Red Rock Desert Canyon**, connected by sweeping expressways and a perimeter ring highway.

---

## 2. Technical Deep Dive

* **Unified Collision Pipeline (`engine/src/world/scene_loader.cpp` & `engine/src/physics/physics_world.cpp`):**
  * `"collider": { "type": "cylinder", "radius": 0.65, "height": 9.0 }` creates a Jolt `CylinderShape` static rigid body *and* adds a cylinder record to `PhysicsWorld::staticObstacles`.
  * `physics.resolveCircleXZ(x, y, z, radius, vx, vz, bounce)` performs exact circle-vs-oriented-box and circle-vs-cylinder push-out + velocity reflection across every static obstacle in one C++ call.
  * `physics.sampleRampSurface(x, z)` queries every JSON-registered `"ramp"` collider automatically — adding a new ramp in JSON immediately makes it jumpable without editing Lua.
* **4 New Blender 5.3 Biome Assets (`tools/blender_build_v06_biome_assets.py`):** Added `pine_tree.glb`, `desert_rock.glb`, `warehouse.glb`, and an animated `lighthouse.glb` with a 360° rotating glowing searchlight beam.

---

## 3. How SAGE Uses This

Game developers can now design entire multi-biome worlds declaratively in `scenes/*.json` with guaranteed solid collisions on every prop, ship a real Title Screen & Pause Menu in `resources/<game>/main.lua`, and press **`F1`** only when they want the developer inspector!
