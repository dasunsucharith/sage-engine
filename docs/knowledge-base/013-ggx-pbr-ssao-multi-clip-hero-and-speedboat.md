# 013 — Cook-Torrance GGX PBR, Normal/MR Maps, Screen-Space Depth AO, 4-Clip Hero & Drivable Speedboat

> **Milestone:** `v1.1.0 — Next-Gen GGX PBR Graphics, Multi-Clip Character Animations & Land/Sea Vehicles`

---

## 1. Plain-English Summary

In **`v1.1.0`**, SAGE takes a huge leap forward in **graphics realism**, **player character fidelity & animation**, and **vehicle variety**:
1. **True Cook-Torrance GGX PBR & glTF Normal/Metallic-Roughness Maps:** Upgraded the core shader from legacy Blinn-Phong to physically based **GGX Microfacet PBR** (the same lighting math used in Unreal Engine & Rockstar's RAGE!) plus automatic loading of glTF 2.0 `normal_texture` and `metallic_roughness_texture` maps. Metallic paint on the Lamborghini Aventador SVJ, Cyberpunk Superbike, Racing Speedboat, hero visor, and rain-slicked roads now reflect sunlight and dynamic point lights with realistic highlights!
2. **Real-Time Screen-Space Contact AO (SSAO) & HDR Exposure Grading:** Replaced the non-sampleable HDR depth renderbuffer with a real sampleable `GL_DEPTH_COMPONENT24` depth texture so the final composite pass darkens geometric crevices, wheel arches, character feet, and building contact seams in real time (toggleable live in `F1`).
3. **Sculpted 4-Clip Hero Character (`Idle`, `Walk`, `Run`, `Swim`):** Rebuilt `character.glb` in Blender 5.3 with smooth contoured armor, racing helmet visor, aero backpack, articulated limbs, and **4 named NLA animation clips** that seamlessly switch as you stand, walk, sprint, or swim!
4. **Drivable Racing Speedboat + Cyberpunk Superbike + Animated Living World (32 `.glb` Models!):** Added a drivable **Luxury Racing Speedboat** (moored at the 32m Marina Pier — press `F` to pilot it over the 3D waves with spinning twin propellers & wake spray!), selectable **Cyberpunk Superbike** (leans realistically into turns!), **Animated Marble Plaza Fountains**, **Flapping Coastal Seagulls**, and a **Supersonic Sky Jet**!

---

## 2. Technical Deep Dive

### A. Cook-Torrance GGX PBR + Screen-Space Derivative Normal Mapping (`gl_renderer.cpp`)
For both the directional sun and all 16 nearest dynamic point lights, `evaluatePbrLight` computes:
- `D_GGX(N, H, roughness)` for physically plausible microfacet highlight distribution
- `G_Smith(N, V, L, roughness)` for microfacet self-shadowing/masking
- `F_Schlick(H·V, F0)` with `F0 = mix(0.04, albedo, metallic)` so metals tint specular reflections naturally while dielectrics exhibit crisp grazing Fresnel
- Screen-space derivative TBN (`dFdx`/`dFdy`) normal perturbation whenever a `.glb` material provides a normal map (`GL_TEXTURE4`), plus green/blue channel roughness/metallic modulation (`GL_TEXTURE5`).

### B. Depth-Texture Screen-Space Ambient Occlusion (`kCompositeFrag`)
Attaching `m_hdrDepthTex` (`GL_DEPTH_COMPONENT24`) to `m_hdrFbo` allows `Pass 5` to linearize pixel depth (`near = 0.50m`, `far = 1250m`) and sample an 8-direction depth ring around every non-sky pixel, applying range-checked contact occlusion (`uSsaoEnabled`) alongside live `uExposure` and vibrance-enhanced ACES Filmic tone mapping.

### C. Named Multi-Clip glTF 2.0 Animation Playback
`evaluateModelNodeMatrices` now resolves named animation clips (`"Idle"`, `"Walk"`, `"Run"`, `"Swim"`) by exact match or prefix match (supporting Blender NLA track export conventions), enabling clean Lua animation state machines with a single `world.playAnimation(id, clipName, loop, speed)` call.

---

## 3. How SAGE Uses This

- **32 Built-in `.glb` 3D Models:** Generated via [tools/build_v11_assets.py](../../tools/build_v11_assets.py) (`character.glb` v2 with 4 clips, `speedboat.glb`, `superbike.glb`, `fountain.glb`, `seagull.glb`, `jet_plane.glb`).
- **3 Seamless Gameplay Modes (`resources/stunt_sandbox/main.lua`):** Press **`F`** to seamlessly transition between **Land Vehicles** (Aventador SVJ / Cyberpunk Superbike / Sports Coupe), **On-Foot Hero** (`Idle`/`Walk`/`Run`/`Swim`), and the **Marina Racing Speedboat**!
