# 015 — SAGE Engine v1.2.0: Skeletal Crossfade Blending, Biomechanical Gait, FXAA 3.11 & Surface Footsteps

## 1. Plain-English Summary

Switching instantly from `Idle` to `Walk` or `Run` in a single frame makes even the best 3D rig feel jerky, and simple sine-wave leg swings make characters look robotic instead of weighty. **SAGE Engine v1.2.0** upgrades the entire character, graphics, and audio pipeline together:

1. **Bone-Level Skeletal Crossfade Blending:** Instead of snapping between clips, SAGE evaluates both the outgoing animation (`prevAnimation`) and the incoming animation (`activeAnimation`) simultaneously and smoothly interpolates every bone's position (`mix`) and rotation (`slerp`) along an S-curve (`blendSeconds`).
2. **8-Phase Biomechanical Gait (`Walk`, `Run`, `Jump`, `Idle`, `Swim`):** Rebuilt `tools/build_realistic_human.py` using periodic Catmull-Rom biomechanical curves — real heel-strike dorsiflexion, loading knee cushion, mid-stance vault & lateral hip weight-shift, pelvis counter-twist against chest/shoulders, toe-off plantarflexion, sprinter heel-to-glute recovery fold, and a dedicated airborne `Jump` tuck.
3. **FXAA 3.11 Anti-Aliasing, Atmospheric Solar Haze & Rim Lighting:** Eliminates jagged diagonal edges in the HDR composite shader, adds warm Mie solar forward-scattering to distant fog, and adds subtle Fresnel rim highlights so 3D characters and vehicles pop cleanly.
4. **Surface-Aware Procedural Footstep Synth (`audio.playFootstep`):** Real-time miniaudio synthesis of asphalt sneaker taps, grass/sand crunch, hollow wooden pier thumps, and shallow water splashes synced to the player's stride speed and landing impacts.

---

## 2. Technical Deep Dive

- **Crossfade Blending (`world.h`, `world.cpp`, `gl_renderer.cpp`):**
  - When `World::playAnimation(id, clip, loop, speed, blendSeconds)` switches to a new clip, `Entity` snapshots `prevAnimation`, `prevAnimTime`, `prevAnimLoop`, `prevAnimSpeed`, and starts `blendElapsed = 0` over `blendDuration`.
  - Both clocks advance in `World::updateAnimations(dt)` so the outgoing walk/run stride keeps moving naturally while fading out.
  - `evaluateModelNodeMatrices()` samples both clips into `(T, R, S)` arrays and applies smoothstep $w = 3t^2 - 2t^3$ via `glm::mix` and `glm::slerp` before building final bone palette matrices (`nodeWorld[joint] * inverseBindMatrix`).
- **HDR FXAA 3.11 (`kCompositeFrag` in `gl_renderer.cpp`):**
  - Evaluates tone-mapped luminance on a 5-tap cross (`M, NW, NE, SW, SE`), skips flat regions (`lumaRange < max(0.045, lumaMax * 0.125)`), and blends along the detected edge gradient vector to smooth silhouettes without blurring interior detail.
- **Footstep Audio (`audio_engine.cpp`):**
  - Lock-free atomic mailbox (`triggerFootstep`, `triggerFootSurface`) wakes a dedicated SVF bandpass + pitch-swept sine thump voice with per-step random pitch jitter (`±10%`) so consecutive steps never sound repetitive.

---

## 3. How SAGE Uses This

- **Lua API additions:**
  - `world.playAnimation(id, clipName, loop, speed, blendSeconds)` — smooth crossfade transitions (default `0.18s`).
  - `world.setFXAA(enabled)` & `world.setSSAO(enabled)` — runtime post-processing toggles.
  - `audio.playFootstep(surface, intensity01)` — 0 = Asphalt, 1 = Grass/Dirt, 2 = Wood Pier, 3 = Water Splash.
- **Dear ImGui Live Editor (`F1`):**
  - Toggle `FXAA 3.11 Edge Anti-Aliasing` live and inspect active animation clip names, speed multipliers, and live `[Blending]` state on any selected `.glb` entity.
