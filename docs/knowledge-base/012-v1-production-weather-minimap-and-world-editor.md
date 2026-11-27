# 012 — SAGE Engine v1.0.0: Dynamic Weather & Wet Roads, 2D Minimap Canvas, Save Data & Live World Editor

> **Milestone:** `v1.0.0 — Production AI-Native 3D Game Engine`

---

## 1. Plain-English Summary

Reaching **`v1.0.0`** means SAGE is no longer just a tech demo — it is a complete, self-contained **C++20 + Lua + Blender** 3D game engine that any developer (or AI coding agent) can use to build, visually edit, save, and ship full 3D games without touching C++.

In **`v1.0.0`**, we completed the final production pillars:
1. **Dynamic Weather & Rain-Slicked Reflective Roads:** Toggle Clear Sky, Light Drizzle, or Neon Rainstorm live (`G`, Main Menu, or `F1`). Falling 3D rain streaks fill the air around the camera while procedural puddles darken asphalt and turn wet streets into glossy mirrors reflecting headlights, neon arches, billboards, and the skyline!
2. **Live `F1` 3D World Editor & JSON Scene Export (`SceneLoader::saveToFile`):** Developers can spawn any of the **28 built-in `.glb` models**, primitives, water volumes, or dynamic point lights straight into the live 3D world, tweak transforms/colors, delete objects, and click **"Save Scene"** to export clean, diffable JSON (`scenes/exported_scene.json`)!
3. **2D Vector HUD & Minimap Canvas (`ui.drawRect`, `ui.drawCircle`, `ui.drawLine`):** Lets any Lua game draw live GTA-style radar minimaps, compasses, crosshairs, or telemetry graphs.
4. **Persistent Save Data (`sage.saveData` / `sage.loadData`):** Safely saves high scores, unlocks, or player progress to disk under `saves/`.

---

## 2. Technical Deep Dive

### A. Procedural Wet-Road Puddles (`gl_renderer.cpp`)
In `kLitFragmentShader`, when `uIsGround == 1 && uWetness > 0.01`:
- Samples low-frequency world-space puddle noise (`groundNoise(vWorldPos.xz * 0.11)`).
- Darkens dry albedo by up to `34%` (mimicking real wet porous asphalt & sand absorption).
- Drops surface roughness smoothly down to `0.06` and raises mirror reflectivity in puddle regions, causing dynamic point lights (`car_headlight`, neon arches, billboards, gas stations) and the sky dome (`reflect(-V, N)`) to shimmer realistically across rain-soaked roads!

### B. 3D Looping Rain Volume (`Pass 3C`)
Instead of allocating thousands of CPU particles for rain, `OpenGLRenderer` hashes deterministic raindrop coordinates inside a `38m × 22m × 38m` camera-following volume and shifts `ry` continuously downward by `timeSeconds * 28.0f`, rendering crisp HDR-lit vertical streaks with almost zero CPU overhead.

### C. Live World Editor & Safe Persistence
- `EditorUI` scans `assets/models/*.glb` dynamically so dropping a brand-new Blender export into `assets/models/` immediately makes it spawnable in the `F1` World Builder!
- `sage.saveData(key, content)` strips non-alphanumeric characters from `key` before writing under `<projectRoot>/saves/<safeKey>.dat`, guaranteeing safe sandboxed persistence.

---

## 3. How SAGE Uses This

- **28 Ready-to-Use `.glb` 3D Models:** Added grand `suspension_bridge.glb`, illuminated `stadium_arena.glb`, animated rocking superyacht `yacht.glb`, and glowing `billboard.glb` via [tools/build_v10_assets.py](../../tools/build_v10_assets.py).
- **Flagship Showcase (`resources/stunt_sandbox/main.lua`):** Now features a **live GTA-style corner minimap radar**, **wet-road rain physics** (lowered tire grip for rain drifting!), and **persistent all-time Best Score tracking**.
