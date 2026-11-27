# 011 — 2-Cascade Soft Shadow Maps (CSM), 560m Open World, Walk/Drive Mode & AI Traffic

> **Milestone:** `v0.9.0 — 560m Grand Open World, 2-Cascade CSM, Walk/Drive & 24 Blender Models`

---

## 1. Plain-English Summary

Expanding a 3D open world from `320m × 320m` to a massive **`560m × 560m` island** (more than **3× the total playable area**) creates two classic AAA engine bottlenecks:
1. **Shadow Stretching:** A single shadow map forced to cover `135m` looks blurry around the player's car; a tight `32m` shadow map cuts off skyscraper and mountain shadows in the distance.
2. **Dynamic Light Overflow:** With 24+ lights across Neon Arches, Gas Stations, Alpine Cabins, Ferris Wheel, Pier Lanterns, Lighthouse Beam, and AI Traffic Headlights, passing only the *first* 16 lights in the vector would leave distant districts completely dark!

In **`v0.9.0`**, SAGE solves both at the C++20 renderer level — while adding **GTA-style Exit/Enter Vehicle (`F`)** with a walking & swimming 3rd-person character, **6 autonomous AI Traffic cruisers**, and **6 new Blender `.glb` models** (24 models total!).

---

## 2. Technical Deep Dive

### A. Real-Time 2-Cascade Shadow Maps (`gl_renderer.cpp`)
SAGE now renders **two texel-snapped `2048 × 2048` hardware-PCF depth cascades** every frame:
- **Cascade 0 (Near — `32m` radius, `~3.1 cm/texel`):** Ultra-sharp shadows for the Aventador SVJ, walking driver character, crates, traffic cones, and nearby trees.
- **Cascade 1 (Far — `135m` radius):** Sweeping skyline shadows from 14 Downtown skyscrapers, the 24m Coastal Ferris Wheel, wind turbines, alpine cabins, and desert mesas.
- **Smooth Blending + Frustum Distance Culling:** Entities outside each cascade's radius are skipped on the CPU before drawing, and `computeShadowVisibility` smoothly blends Cascade 0 into Cascade 1 along the outer 10% border so the transition is invisible while driving at 230 km/h!

### B. Graphics Polish: Starry Night Sky, Metallic Clearcoat & Contact AO
- **Procedural Night Starfield:** As `uSkyZenith` darkens into Neon Night, a crisp twinkling procedural starfield fades in across the sky dome above the horizon.
- **Sky-Dome Environment Clearcoat Reflection:** Metallic & smooth surfaces (`uMetallic` / low `uRoughness`, such as the Aventador SVJ and AI traffic sedans) reflect the live dynamic sky gradient (`reflect(-V, N)`).
- **Ground Contact AO:** Subtle near-ground ambient darkening (`y < 0.65m`) anchors buildings, vehicles, and props solidly onto asphalt, sand, and forest floors.
- **Nearest-16 Dynamic Point Lights:** Every frame, `OpenGLRenderer` runs `std::partial_sort` by squared XZ distance to `camTarget` so the **16 closest active point lights** always illuminate roads, buildings, and water ripples anywhere across the `560m` island.

---

## 3. How SAGE Uses This

- **24 Built-in Blender `.glb` Models:** Added animated `character.glb`, spinning 24m `ferris_wheel.glb`, `ai_sedan.glb`, spinning-rotor `helicopter.glb`, desert `cactus.glb`, and warm-lit `alpine_cabin.glb` via [tools/build_v09_assets.py](../../tools/build_v09_assets.py).
- **GTA-Style Dual-Mode Gameplay (`resources/stunt_sandbox/main.lua`):** Press **`F`** anytime to step out of your supercar, walk/sprint/jump around the boardwalk, climb onto the 32m pier, **swim** in the 72m Palm Lagoon (while cars still realistically sink & flood!), and press **`F`** to hop back into the driver's seat.
