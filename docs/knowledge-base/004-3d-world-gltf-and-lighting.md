# 004 — 3D World, glTF 2.0 (`.glb`) Models, Lighting & Blender Workflow

**Milestone:** `v0.3 — 3D World`

---

## 1. Plain-English Summary

In `v0.2`, SAGE drew flat shapes from straight overhead. In **`v0.3 — 3D World`**, SAGE steps into full 3D:
* **Perspective Camera + Depth Buffer:** Objects farther away shrink naturally, and closer objects automatically hide objects behind them.
* **Sky & Sunlight:** A procedural sky gradient with a glowing sun disk, directional sunlight, sky/ground hemisphere bounce light, specular highlights, atmospheric fog, and ACES filmic tone mapping — all shifting smoothly when you change `timeOfDay`.
* **Drop-In `.glb` 3D Models:** SAGE reads industry-standard **glTF 2.0 (`.glb`)** files directly from `assets/models/` — including multi-material meshes, metallic/roughness values, emissive glow (headlights/neon), and embedded PNG/JPEG textures.
* **Plain-Text JSON Worlds (`scenes/*.json`):** Entire cities, skies, and placed `.glb` props live in readable JSON files that hot-reload live when saved.

---

## 2. Technical Deep Dive

### Rendering Pipeline (`engine/src/renderer/gl_renderer.cpp`)
1. **Pass 1 — Procedural Sky Dome:** Renders a single fullscreen triangle (`gl_VertexID`, no VBO) with depth writes disabled, reconstructing each pixel's world ray via `inverse(Proj * View)` to draw the horizon-to-zenith gradient and solar disk.
2. **Pass 2 — 3D Lit Geometry & `.glb` Cache:**
   * Uses `glm` (`1.0.3`) for perspective projection (`glm::perspective`), camera look-at (`glm::lookAt`), and entity `TRS` matrices (`Yaw -> Pitch -> Roll`).
   * Loads `.glb`/`.gltf` files via **`cgltf` (`1.15`)** and embedded/external textures via **`stb_image`**, caching each model once in GPU VRAM so 16 crates share 1 GPU buffer.
   * Shades surfaces with directional diffuse + hemisphere ambient fill + roughness-scaled Blinn-Phong specular + Fresnel rim + emissive glow + distance horizon fog + ACES Filmic Tone Mapping.

### Coordinate System Convention
SAGE uses standard **Right-Handed Y-Up** (matching glTF 2.0 and OpenGL):
* **`+Y`** = Up (height in meters)
* **`+Z`** = Forward (front of vehicles / ramps at `yaw = 0°`)
* **`+X`** = Left when looking along `+Z` (positive `yaw` turns Left)

---

## 3. Blender-to-SAGE Workflow (Drop In Your Own 3D Models!)

Whenever you create or download a 3D object in **Blender**:
1. Orient the front of your vehicle/object facing **`-Y` in Blender** (Blender's glTF exporter with `+Y Up` converts Blender `-Y` → glTF `+Z` forward and Blender `+Z` → glTF `+Y` up) and place the bottom of tires/base at `Z = 0`.
2. Go to **File → Export → glTF 2.0 (`.glb`/`.gltf`)**:
   * Format: **glTF Binary (`.glb`)**
   * Include: **Apply Modifiers** (checked)
   * Transform: **+Y Up** (checked — Blender default)
   * Data → Mesh: **Normals** & **UVs** (checked)
3. Save into `assets/models/your_model.glb`.
4. Drop it straight into [`scenes/stunt_city.json`](../../scenes/stunt_city.json) or spawn it in Lua with `world.spawnModel("my_car", "assets/models/your_model.glb", 0, 0, 0)` — zero C++ code required!
