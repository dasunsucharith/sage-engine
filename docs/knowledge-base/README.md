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

## How to Add a New Entry
Create `NNN-short-topic-name.md` (e.g., `002-how-gpus-draw-triangles.md`) with:
1. **Plain-English Summary** (beginner-friendly analogy + what it does)
2. **Technical Deep Dive** (how AAA engines like RAGE implement it)
3. **How SAGE Uses This** (exact impact on our C++20 + Lua architecture)
