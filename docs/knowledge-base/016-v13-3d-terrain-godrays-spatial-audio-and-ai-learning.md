# 016 — SAGE Engine v1.3.0: 3D Heightmap Terrain, 4-Wheel Suspension, Volumetric God-Rays, Spatial 3D Audio & Self-Learning AI Telemetry

## 1. Plain-English Summary

To rival and surpass AAA open-world engines like Rockstar's RAGE (GTA VI), an engine needs **living 3D topography**, **physical vehicle weight**, **cinematic atmosphere**, **directional 3D sound**, and a **continuous self-improvement feedback loop**. **SAGE Engine v1.3.0** ships all five together:

1. **Synchronized 3D Heightmap Terrain + Jolt `HeightFieldShape`:** Rolling Alpine hills, terraced Red Rock canyon mesas, and 46m snow-capped mountain ranges rise smoothly around the 560m island while automatically flattening highways, ring roads, Downtown Plaza, and the Marina Lagoon — and auto-grounding off-road trees, cabins, and desert rocks cleanly onto the slopes.
2. **4-Wheel Spring-Damper Vehicle Suspension:** Every vehicle samples terrain, ramp, and pier elevation at all 4 wheel corners (`FL`, `FR`, `RL`, `RR`) to compute real hill pitch, side-slope roll, crest launches, and spring-damper compression bounces on landing.
3. **Volumetric Sun Shafts (God-Rays) & Drifting Cloud Shadows:** Projects the dynamic 3D sun into screen space for radial depth-occluded crepuscular sunbeams around buildings and mountain ridges, paired with drifting FBM cloud shadows across the entire world.
4. **3D Spatial Stereo-Panned Audio (`audio.setListener`, `audio.setSpatialEmitter`):** Up to 8 lock-free 3D world emitters (Ocean Surf, Plaza Fountain, Overhead Patrol Helicopter Rotor, Alpine Wind Ridge) with inverse-square distance falloff and equal-power stereo panning.
5. **Self-Learning AI Telemetry & Engineering Memory (`logs/last_session_telemetry.json` + `docs/ai_learning_log.json`):** Every engine run automatically records hardware/FPS/entity metrics, and `AGENTS.md` now enforces a structured AI learning loop so every future iteration builds on verified engineering lessons and real performance data.

---

## 2. Technical Deep Dive

- **Zero-Z-Fighting Terrain Architecture (`world.cpp`, `gl_renderer.cpp`, `physics_world.cpp`):**
  - `World::sampleTerrain(x, z)` evaluates 4-octave value-noise FBM + ridged peaks masked cleanly away from highways, ring loops, Downtown Plaza, and the Marina Lagoon, with central-difference analytical normals.
  - The GPU 160×160 terrain mesh skips completely flat quads and sinks edge skirts `0.22m` below `y = 0` so flat roads never Z-fight, while casting mountain shadows into both 2048px shadow cascades and splatting 5 biome materials (Alpine Grass, Red Rock Strata, Golden Sand, Slope Granite, Snow Caps) in `kLitFragmentShader`.
  - `PhysicsWorld::buildTerrainCollider()` mirrors the heightmap into a Jolt `HeightFieldShape` (`129×129`), marking water volumes and flat road zones as `cNoCollisionValue` so Jolt dynamic crates/balls roll down 3D hills AND sink/float naturally in the Marina Lagoon.
- **Volumetric God-Rays (`kCompositeFrag`):**
  - Projects `camPos + normalize(sunDir) * 450` through `viewProj` into screen UV `(uSunScreenPos)` and runs a 10-tap decaying radial occlusion march against `uDepthTex` (`step(0.9985, depth)`).
- **Equal-Power 3D Spatial Audio (`audio_engine.cpp`):**
  - Per audio block, computes right-vector `R = (-fwdZ, 0, fwdX)`, pan `p = clamp(dot(dir, R), -1, 1)`, angle `theta = (p * 0.5 + 0.5) * pi/2`, and equal-power gains `(cos(theta), sin(theta))` scaled by `(1 - d/r)^2`.

---

## 3. How SAGE Uses This

- Any Lua game can immediately build open-world games on hilly 3D terrain using `world.sampleTerrain(x, z)`, `world.setGodRays(true, 0.45)`, `world.setCloudShadows(true)`, and `audio.setSpatialEmitter(...)`.
- Every run records `logs/last_session_telemetry.json`, feeding directly into `docs/ai_learning_log.json` for continuous AI-guided engine evolution toward `v1.4` and `v2.0`.
