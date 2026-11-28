# SAGE Living Knowledge Base

This folder is the **engineering brain** of SAGE Engine. Every time we study a game engine video, paper, graphics technique, or finish a milestone, we document what we learned here in plain English — so both beginners and AI agents share the same growing knowledge.

## Entries

| # | Topic | Source / Milestone |
| :--- | :--- | :--- |
| [001](001-rage-and-gtavi.md) | Inside RAGE & GTA VI + AI-Native Engine Design | Speclizer (*Inside the Engine Behind GTA VI*) & Adrian Courrèges (*GTA V Graphics Study*) |
| [002](002-window-loop-and-first-triangle.md) | OS Window, Fixed-Timestep Game Loop & First GPU Triangle | Milestone `v0.1 — Alive` |
| [003](003-lua-scripting-and-hot-reload.md) | FiveM-Style Lua Scripting & Live Hot-Reload | Milestone `v0.2 — Lua Brain` |
| [004](004-3d-world-gltf-and-lighting.md) | 3D World, glTF 2.0 (`.glb`) Models, Lighting & Blender Workflow | Milestone `v0.3 — 3D World` |
| [005](005-jolt-physics-imgui-and-audio.md) | Jolt Physics, Dear ImGui Live Editor & miniaudio Synth | Milestone `v0.4 — Editor, Physics & Audio` |
| [006](006-cascaded-shadow-maps-and-soft-shadows.md) | Real-Time Cascaded Shadow Maps (CSM) & PCF Soft Shadows | Daily Research Proposal (`v0.5`) |
| [007](007-gltf-animations-shadows-and-lua-hud.md) | Real-Time Soft Shadows, glTF 2.0 Keyframe Animations & Lua Game HUDs | Milestone `v0.5` Implementation |
| [008](008-auto-colliders-menus-and-multi-biome-worlds.md) | Declarative JSON Auto-Colliders, Game Menus & Multi-Biome Worlds | Milestone `v0.6` Implementation |
| [009](009-hdr-bloom-water-shader-and-zfighting-fix.md) | Eliminating Road Z-Fighting, HDR Bloom Post-Processing & Animated 3D Ocean Water | Milestone `v0.7` Implementation |
| [010](010-realistic-water-physics-point-lights-and-particles.md) | Realistic Water Physics, Archimedes Buoyancy, Dynamic Point Lights & 3D Particles | Milestone `v0.8` Implementation |
| [011](011-cascaded-shadows-560m-world-and-walk-drive-ai.md) | 2-Cascade Soft Shadow Maps (CSM), 560m Open World, Walk/Drive Mode & AI Traffic | Milestone `v0.9` Implementation |
| [012](012-v1-production-weather-minimap-and-world-editor.md) | SAGE Engine v1.0.0: Dynamic Weather & Wet Roads, 2D Minimap Canvas, Save Data & Live World Editor | Milestone `v1.0.0` Production Release |
| [013](013-ggx-pbr-ssao-multi-clip-hero-and-speedboat.md) | Cook-Torrance GGX PBR, Normal/MR Maps, Screen-Space Depth AO, 4-Clip Hero & Drivable Speedboat | Milestone `v1.1.0` Implementation |
| [014](014-gltf-gpu-skeletal-skinning-and-human-rig.md) | glTF 2.0 GPU Skeletal Skinning (`JOINTS_0`/`WEIGHTS_0`) & Seamless Rigged Human Pipeline | Milestone `v1.1.2` Implementation |
| [015](015-v12-skeletal-blending-fxaa-and-footsteps.md) | Skeletal Crossfade Blending, 8-Phase Biomechanical Gait, FXAA 3.11 & Surface Footstep Synth | Milestone `v1.2.0` Implementation |
| [016](016-v13-3d-terrain-godrays-spatial-audio-and-ai-learning.md) | 3D Heightmap Terrain, Jolt HeightField, 4-Wheel Suspension, Volumetric God-Rays, Spatial 3D Audio & AI Self-Learning Telemetry | Milestone `v1.3.0` Implementation |
| [017](017-v14-instanced-grass-ssr-frustum-culling-and-crowd-ai.md) | GPU-Instanced Interactive 3D Grass, Alpha-Packed Screen-Space Reflections (SSR), Frustum Culling, Preloading & Reactive Crowd AI | Milestone `v1.4.0` Implementation |
| [018](018-v15-gpu-prop-instancing-3csm-euphoria-ragdoll-and-spline-ai.md) | GPU-Instanced Static Prop Batching, Tiered 3-CSM, Exact Bounding Spheres, Euphoria Active-Physics Ragdoll, Look-At IK & Spline AI | Milestone `v1.5.0` Implementation |
| [019](019-v16-material-merging-soft-water-entity-ik-and-survival.md) | Material-Merged Instancing, Pose Cache, Time-Sliced 3-CSM, Depth-Buffered Soft Water, Double-Sided PBR, Entity-Space Look-At IK & Echo Island Survival | Milestone `v1.6.0` Implementation |
| [020](020-honest-benchmarks-simulation-clock-and-lua-sandbox.md) | Steady-State `--benchmark` Percentiles, Deterministic Simulation Clock, Lua Sandbox Hardening & Runaway-Script Watchdog | Foundation Fix (post-`v1.6.0`) |
| [021](021-render-interpolation.md) | Render Interpolation: Smooth 120/144 Hz+ Motion from a Fixed 60 Hz Simulation, Teleports & Camera Cuts | Foundation Fix (post-`v1.6.0`) |
| [022](022-data-driven-terrain.md) | Data-Driven Terrain: Procedural & Heightmap Landscapes, Features, Layers, Grass & Collision from Scene JSON | Foundation Fix (post-`v1.6.0`) |
| [023](023-per-resource-saves.md) | Per-Resource Save Folders, Crash-Safe Writes & Automatic Save Migration | Foundation Fix (post-`v1.6.0`) |
| [024](024-gpu-pass-timing.md) | Non-Blocking GPU Timer Queries per Render Pass | Foundation Fix (post-`v1.6.0`) |
| [025](025-cpu-frame-timing.md) | CPU Frame Sections, Per-Pass Submission Cost & Why SAGE Is Actually GPU/Driver-Bound | Foundation Fix (post-`v1.6.0`) |
| [027](027-opaque-pass-draw-order.md) | Opaque Pass: Pixel-Bound Diagnosis, Terrain-Last & Front-to-Back Draw Order (Same Image, -8 to -15% GPU) | Foundation Fix (post-`v1.6.0`) |
| [028](028-point-light-cost-analysis.md) | Point-Light Cost Analysis: Per-Object, Tiled & Clustered Culling Measured (Scenes Are Densely Lit, Little to Skip) | Foundation (post-`v1.6.0`) |
| [026](026-shadow-pass-analysis.md) | Shadow Pass Analysis: Exact Light-Space Caster Culling (Missing-Shadow Fix), Merged Depth-Only Meshes & Per-Cascade Timing | Foundation Fix (post-`v1.6.0`) |

## How to Add a New Entry
Create `NNN-short-topic-name.md` (e.g., `002-how-gpus-draw-triangles.md`) with:
1. **Plain-English Summary** (beginner-friendly analogy + what it does)
2. **Technical Deep Dive** (how AAA engines like RAGE implement it)
3. **How SAGE Uses This** (exact impact on our C++20 + Lua architecture)
