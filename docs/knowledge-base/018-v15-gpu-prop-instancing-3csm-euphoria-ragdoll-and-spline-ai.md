# 018 — GPU-Instanced Static Prop Batching, Tiered 3-CSM, Euphoria Active-Physics Ragdoll, Look-At IK & Spline AI (`v1.5.0`)

## 1. Plain-English Summary

When you scale an open-world engine toward GTA VI / RAGE quality, two things matter most: **drawing hundreds of objects cheaply** and **making every person and vehicle in the city feel genuinely alive**.
* **GPU-Instanced Prop Batching (`glDrawElementsInstanced`):** Instead of telling the GPU 40 separate times "draw a pine tree here… now draw another pine tree there", SAGE automatically groups every visible static `.glb` prop sharing the same model (trees, street lamps, skyscrapers, cacti, rocks, crates) into a single GPU batch and stamps them all in **1 draw call per submesh** across both Shadow and PBR passes — saving **3,315 draw calls every frame**!
* **3-Split Tiered Cascaded Shadow Maps (3-CSM):** Shadows right next to your character/car stay razor-sharp (`24m @ 2048px`), city blocks cast crisp mid-range shadows (`92m @ 1024px`), and the 46m Alpine mountain range casts panoramic valley shadows out to **`255m`** — while automatically skipping tiny props and distant characters in the far mountain cascade.
* **Euphoria-Style Active-Physics Ragdoll & Look-At IK:** Citizens turn their heads & shoulders naturally (`Look-At IK`) as your car approaches, and when clipped by a vehicle or hit by a **`Q` Kinetic Repulsor Blast** or **`X` Shockwave**, they transition seamlessly into a velocity-driven mid-air flail & terrain-bouncing ragdoll tumble, then smoothly blend back into standing animation and sprint away!
* **Smart Catmull-Rom Spline AI Traffic:** Ten autonomous cruisers glide smoothly along closed-loop Catmull-Rom highway splines (`world.evalSpline`), scan ahead for your car or walking character, flash bright red brake lights while yielding, and accelerate smoothly once the lane clears.

---

## 2. Technical Deep Dive

### A. Automatic GPU-Instanced Static Prop Batching + Exact Bounding Spheres
* **Loader & VAO Layout:** Added `glDrawElementsInstanced` (`GL_DYNAMIC_DRAW`, `GL_TEXTURE6`) to `gl_functions.{h,cpp}`. A shared dynamic `m_propInstVbo` holds `PropInstanceData { mat4 model; vec4 colorEmissive; }` (80 bytes) bound once at vertex locations `5..9` (`divisor = 1`) on every uploaded `GpuSubMesh`.
* **Exact Per-Model Bounding Spheres:** During `getOrLoadModel()`, SAGE computes each `.glb` model's exact local `bbMin`/`bbMax` -> `(boundCenter, boundRadius)` once. This tightened 6-plane Gribb-Hartmann frustum culling from **162 -> 204 culled entities (52.8% of the 386-entity scene)** and enabled exact world-size shadow LOD culling without hardcoded filenames!
* **1×1 GPU Shader Pipeline Warmup:** Inside `preloadModels()`, SAGE executes a 1×1 warmup pass across Shadow (standard & instanced), Sky, Lit PBR (standard & instanced), Grass, Water, Bloom, and HDR Composite shaders followed by `glFinish()`, pushing NVIDIA shader JIT compilation out of Frame 1.

### B. Real-Time Telemetry Self-Learning Catch (Tiered 3-CSM Optimization)
Our `v1.5.0` self-learning loop caught a textbook AAA graphics bottleneck immediately during our first smoke test:
* **Pass 1 (`v1.5.0-draft`):** Adding a 3rd `2048×2048` 255m cascade without size-LOD culling caused `lastDrawCalls = 2,712` and `avgFrameMs = 45.0 ms` (`peakFrameMs = 241.0 ms`).
* **Pass 2 (`v1.5.0-final`):** Switching Mid/Far cascades to `1024×1024`, using exact `boundRadius` for frustum & shadow LOD (`minWorldRadius = 3.2m` in Far Cascade 2, skipping distant skinned characters), and tightening bounding spheres slashed `avgFrameMs` by **33.8%** (`45.0 ms -> 29.8 ms`), dropped `peakFrameMs` to a **new all-time record low (`179.4 ms`)**, and cut total session runtime to **`0.945 s`**!

### C. Procedural Look-At IK & Euphoria Active-Physics Ragdoll Bone Layer
* On `.glb` load, `classifyBoneRole()` tags semantic humanoid bones (`Hips`, `Spine`, `Chest`, `Neck`, `Head`, `UpperArmL/R`, `ForearmL/R`, `ThighL/R`, `ShinL/R`) so the system works on SAGE's 22-bone Blender rig and standard Mixamo/Rigify skeletons alike.
* **Look-At IK (`world.setLookAt`):** Transforms target vector into entity local space, clamps anatomically (`±64°` yaw, `±26°` pitch), and distributes smooth quaternion rotations across `Head` (56%), `Neck` (24%), and `Chest` (20%).
* **Active-Physics Ragdoll (`world.triggerRagdoll`):** `World::updateAnimations()` simulates ballistic gravity (`-24 m/s²`), 3D terrain bounce (`sampleTerrain`), surface friction, impact dust bursts, and tumble angular velocity; `evaluateModelNodeMatrices()` blends (`smoothstep(ragdollBlend)`) `Hips` tumble + floor drop, inertial `Spine`/`Head` whiplash, and velocity-scaled limb flailing, then smoothly blends `ragdollBlend: 1 -> 0` over `0.65s` during recovery.

### D. Native Catmull-Rom 3D Spline API (`world.evalSpline`)
* Evaluates cubic Catmull-Rom position and exact analytical first-derivative tangent `(tx, ty, tz, yawDeg)` in C++20 for open or closed loops — reusable by any Lua game for traffic, pedestrians, camera rails, trains, or rollercoasters.

---

## 3. Verified RTX 5050 Telemetry Progression (`v1.3.0 -> v1.5.0`)

| Metric | `v1.3.0` | `v1.4.0` | `v1.5.0` (Draft) | **`v1.5.0` (Shipped)** |
| :--- | :---: | :---: | :---: | :---: |
| Entities / Dynamic Lights | 374 / 33 | 378 / 33 | 386 / 37 | **386 / 37** |
| Shadow Cascades | 2-CSM (135m) | 2-CSM (135m) | 3-CSM (255m) | **Tiered 3-CSM (255m)** |
| Frustum-Culled Entities | 0 | 162 (42.9%) | 167 (43.3%) | **204 (52.8%)** |
| Draw Calls / Saved via Instancing | — | — | 2,712 / 4,561 | **1,932 / 3,315 saved** |
| 12-Frame Cold-Start Avg | 49.8 ms | 28.8 ms | 45.0 ms | **29.8 ms** |
| Cold-Start Peak Frame | 226.7 ms | 190.9 ms | 241.0 ms | **179.4 ms (Record Low!)** |
| Total 12-Frame Session Time | — | 1.081 s | 1.622 s | **0.945 s (Record Fast!)** |

**Next Self-Learning Target (`v1.6.0`):** Time-slice the 255m Far Shadow Cascade (update every 2nd frame) + merge static `.glb` submeshes sharing identical material/textures at load time to cut `lastDrawCalls` below `600`, plus streaming open-world world chunks and volumetric sky clouds!
