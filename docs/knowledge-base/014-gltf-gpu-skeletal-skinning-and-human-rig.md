# 014 — glTF 2.0 GPU Skeletal Skinning & Rigged Human Pipeline

## 1. Plain-English Summary

Early 3D characters built out of separate rigid cylinders or spheres always show visible seams at shoulders, elbows, knees, and necks whenever limbs bend. Modern game engines (including Rockstar's RAGE, Unreal Engine, and now **SAGE Engine**) solve this with **Skeletal Skinning (Linear Blend Skinning)**:

1. An artist (or Blender Python script) builds a **seamless quad-topology mesh** and places an internal **Armature skeleton** of connected bones (`Hips` → `Spine` → `Chest` → `Neck` → `Head`, plus `UpperArm`/`Forearm`/`Hand` and `UpperLeg`/`LowerLeg`/`Foot`).
2. Each vertex stores up to **4 influencing bone indices (`JOINTS_0`)** and **4 normalized blend weights (`WEIGHTS_0`)** computed via Blender's Bone-Heat weighting (`ARMATURE_AUTO`).
3. Every frame, the CPU evaluates the active animation clip (`Idle`, `Walk`, `Run`, `Swim`) into a palette of bone matrices, uploads them to the GPU, and the **Vertex Shader** smoothly blends each vertex across its weighted bones — in **both** the main GGX PBR lighting pass and the 2-Cascade Shadow Map pass!

---

## 2. Technical Deep Dive

Following the **glTF 2.0 Skinning Specification (Section 3.7.3.2)**:

- Each glTF `skin` stores an array of joint node indices (`joints[]`) and matching `inverseBindMatrices[]` (column-major 4×4 matrices that transform vertices from mesh bind-pose space into each joint's local rest space).
- Every frame, after evaluating TRS animation channels and propagating parent-to-child world transforms (`nodeWorld[]`), the joint palette matrix for joint `j` is:

$$M_{\text{joint}}[j] = M_{\text{world}}[\text{joints}[j]] \cdot M_{\text{IBM}}[j]$$

- On the GPU (`GLSL #version 330 core`, `MAX_BONES = 64`), each vertex blends its 4 bone matrices:

$$M_{\text{skin}} = \sum_{k=0}^{3} w_k \cdot M_{\text{joint}}[j_k]$$

- The skinned vertex position `M_skin * vec4(aPos, 1.0)` and normal `mat3(M_skin) * aNormal` are then transformed directly by the entity world matrix `uModel` (ignoring the skinned mesh node's own local transform, per the glTF 2.0 spec).
- Because both `kLitVertexShader` and `kShadowVertexShader` run the identical skinning math, rigged characters cast **live animated shadows** into both the 32m Near and 135m Far PCF shadow cascades.

---

## 3. How SAGE Uses This

- **Renderer (`engine/src/renderer/gl_renderer.cpp`):**
  - Extended `Vertex3D` with `aJoints` (`location = 3`) and `aWeights` (`location = 4`).
  - Added `GpuSkin` (`jointNodeIndices`, `inverseBindMatrices`), `cgltf` parsing of `JOINTS_0`/`WEIGHTS_0`/`data->skins`, per-skin bone palette upload (`uploadSkinPalette`), and `uHasSkin`/`uBoneMatrices[64]` uniform support in both PBR and Shadow passes.
- **Blender Character Pipeline (`tools/build_realistic_human.py`):**
  - Generates a 1.78m seamless quad-topology Subdivision-Surface human (`HeadNeck`, open-front leather `Jacket` + continuous sleeves, `InnerTee`, continuous pelvis-to-ankle `Jeans`, sculpted fingers & sneakers, polar-aligned eyeballs with iris/limbal ring/pupil, eyelids, sculpted nose/lips/jawline, and short-fade hair) rigged to a 22-bone Blender Armature (`HumanArmature`) with 4 named NLA animation clips (`Idle`, `Walk`, `Run`, `Swim`).
  - Exports editable `assets/models/human_rigged.blend` and game-ready `assets/models/character_rigged.glb` with 0 glTF export warnings.
- **Lua Gameplay (`resources/stunt_sandbox/main.lua`):**
  - Zero new Lua boilerplate needed — existing `world.spawnModel` + `world.playAnimation("player_char", "Idle"|"Walk"|"Run"|"Swim", ...)` seamlessly drives the skinned character!
