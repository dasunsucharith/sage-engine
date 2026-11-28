# 019 — Material-Merged Instancing, Soft Depth Water, Entity-Space Look-At IK & Wild Frontier: Echo Island (`v1.6.0`)

## 1. Plain-English Summary

In **v1.6.0**, we used real in-engine GPU screenshot inspection on an **NVIDIA RTX 5050** to find and solve the exact visual, animation, and performance bottlenecks that separate hobbyist prototypes from AAA open-world games:

* **Why Meshy clothes & fur had black/white speckles:** Single-sided cloth flaps and fur cards (`doubleSided: true`) flip half their normals inward if blindly recalculated in Blender. Without flipping `N = -N` on back-faces in the fragment shader, those triangles reflect 100% sky mirror glare (`dot(N, V) <= 0`) and receive zero sunlight.
* **Why water had harsh polygon seams:** Flat water planes fade cleanly onto beaches only when the water shader knows the real distance from the water surface down to the underwater terrain (`waterDepth = sceneDepth - waterDepth`). Copying the HDR depth buffer right before rendering water unlocks silky translucent turquoise shallows and organic lapping surf foam around every cove, boulder, and inland lake.
* **Why multi-bone quadrupeds twisted their necks during Look-At IK:** Applying fixed local-Y rotation to every bone named `neck` over-rotated a 5-neck-bone Timber Wolf by `216%` along an arbitrary bone roll axis. Dividing the share by exact bone counts and converting entity-space yaw/pitch into each bone's rest-pose local space fixes both bipeds and quadrupeds permanently.
* **How we cut draw calls by 40.7% and startup time by 34.6%:** Merging identical-material static `.glb` submeshes on load, caching animated skeletal poses once per frame across shadow & main passes (zero heap allocations), time-slicing the 255m Far Shadow Cascade, GPU-instancing falling rain streaks, and building in full MSVC **Release (`/O2`)** mode.

---

## 2. Technical Deep Dive

### A. Static `.glb` Submesh Material Merging
Multi-part Blender props (`aventador_svj.glb`, `alpine_cabin.glb`, `city_tower.glb`) often export dozens of scene nodes referencing the same PBR material. In `OpenGLRenderer::getOrLoadModel()`, static models (`!hasAnimations && !hasSkins`) now bucket all node triangles by their exact `const cgltf_material*` pointer, bake world transforms into `Vertex3D`, and upload **one merged VBO/EBO per unique material**:
* `aventador_svj.glb`: **128 → 22 submeshes**
* `alpine_cabin.glb`: **14+ → 4 submeshes**
* `survival_pine.glb` / `survival_palm.glb` / `survival_bush.glb`: **2 submeshes**; `survival_boulder.glb`: **1 submesh**

### B. Zero-Allocation Per-Frame Skeletal Pose Cache & Entity-Space Look-At IK
* On `.glb` load, each `ModelNode` precomputes its orthonormalized rest-pose world rotation `restWorldRot`, and `GpuModel` counts exact `headBoneCount`, `neckBoneCount`, and `chestBoneCount`.
* Look-At IK computes entity-space `qEntity = angleAxis(yaw, +Y) * angleAxis(pitch, +X)` scaled by `roleShare / roleCount` and converts it cleanly into bone-local space via:
  ```cpp
  glm::quat qLocal = glm::normalize(glm::conjugate(restW) * qEntity * restW);
  ```
* `OpenGLRenderer::getOrEvaluatePose()` evaluates each animated entity **once per frame** using reusable `AnimEvalScratch` buffers (zero `std::vector` heap allocations) and shares the cached matrices across Shadow Pass 0 and Main Pass 2.

### C. Depth-Buffered Soft-Shore Water & Shadowed 4-Blade Grass
* Right before Pass 3 (Water), `glCopyTexSubImage2D` copies the bound HDR FBO's `GL_DEPTH_COMPONENT24` attachment into `m_waterDepthCopyTex` (avoiding FBO read/write feedback loops). `kWaterFragmentShader` linearizes both depths to drive soft beach alpha (`0.0m–0.55m`), turquoise-to-navy absorption, and animated FBM shoreline surf foam over a `160×160` wave grid.
* `ensureGrassField()` generates **54,000 4-blade curved slender tufts (`216,000` blades)** rendered in **1 `glDrawArraysInstanced` call**, sampling the Near & Mid CSM shadow cascades (`uShadowMap`, `uShadowMapMid`) so grass blends naturally under Douglas-fir pines and cliffs.

### D. 5-Point Slope Grounding & Wild Frontier: Echo Island
* `SceneLoader` samples 5 terrain points (`center`, `±dx`, `±dz`) in island mode and anchors buildings, trees, and boulders at `minH * 0.78 + centerH * 0.22 - 0.14`, eliminating floating corners on hillsides.
* **Wild Frontier: Echo Island (`--survival`)** showcases all `v1.6.0` systems: seamless 3rd-person ↔ 1st-person camera (`V`), streaming CC0 `.wav`/`.mp3` wilderness ambience & 3D spatial foley (`miniaudio`), 6 custom Blender-rigged Meshy hero `.glb` models, 4 procedural organic wilderness foliage models, smooth `lerpAngle` + terrain-slope pitch wildlife AI, crafting, base building, vitals, and rescue extraction.

---

## 3. Verified RTX 5050 Hardware Benchmarks (`v1.5.0` → `v1.6.0`)

| Benchmark | `v1.5.0` (Debug) | `v1.6.0` (Release `/O2`) | Delta |
| :--- | :---: | :---: | :---: |
| Stunt City Draw Calls | `1,932` | **`1,145`** | **-40.7%** |
| Stunt City 12-Frame Session | `0.945 s` | **`0.618 s`** | **-34.6%** |
| Stunt City Cold-Start Avg Frame | `29.81 ms` | **`26.79 ms`** | **-10.1%** |
| Stunt City Grass Blades | `57,290` | **`130,100`** (shadowed) | **+127%** |
| Echo Island Survival Draw Calls | — | **`147`** (`349` saved) | **147 DC** |
| Echo Island Grass Blades | `76,000` | **`216,000`** (shadowed) | **+184%** |
| Echo Island Steady-State Frame | — | **`~5.1 ms` (~196 FPS)** | **RTX 5050** |
