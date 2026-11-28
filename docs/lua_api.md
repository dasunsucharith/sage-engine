# SAGE Engine — Lua API Reference

> **Status:** Live (**`v1.5.0`**) — **Automatic GPU-Instanced Static Prop Batching (`glDrawElementsInstanced`)**, **3-Split Tiered Cascaded Shadow Maps (`24m / 92m / 255m` 3-CSM)**, **Euphoria-Style Active-Physics Ragdoll Blending & Procedural Head/Torso Look-At IK**, **Native C++20 Catmull-Rom 3D Spline Navigation (`world.evalSpline`)**, **Exact Per-Model Bounding-Sphere Frustum Culling & GPU Shader Warmup**, **GPU-Instanced 3D Wind Grass (~57k blades)**, **Real-Time Screen-Space Reflections (SSR)**, **Procedural 3D Heightmap Terrain + Jolt 129×129 HeightField**, **Volumetric God-Rays**, **Drifting Cloud Shadows**, **3D Spatial Audio**, **Self-Learning AI Telemetry**, **glTF 2.0 GPU Skinning & Crossfade Blending**, **FXAA 3.11**, **Cook-Torrance GGX PBR + SSAO**, **Live World Editor (`F1`)**, **33 Blender `.glb` Models**, **Jolt Physics 5.7**, and **miniaudio 0.11**.
> This file is the single source of truth for both human scripters and AI agents writing Lua for SAGE.

---

## FiveM-Style Resource Convention

Each gameplay feature or game lives in `resources/<resource_name>/`:

```json
{
  "name": "stunt_sandbox",
  "version": "0.4.0",
  "author": "Your Name",
  "enabled": true,
  "description": "What this resource does.",
  "scripts": ["main.lua"]
}
```

* Set `"enabled": false` to disable a resource without deleting it.
* Save any `.lua`, `manifest.json`, or `scenes/*.json` file (or press **`F5`** in-game) to hot-reload live — **no C++ recompile required**.
* Lua errors are logged with file path and line number without crashing the engine; fix the error and save to recover immediately.

---

## Lifecycle Callbacks

Every resource script in `resources/<name>/main.lua` can define these automatic callbacks:

```lua
function onStart()       -- Called once after the JSON scene loads (and on every hot-reload)
function onTick(dt)      -- Called at a deterministic 60 Hz (dt = seconds per tick)
function onRenderUI()    -- Optional: called every frame to draw custom game HUDs via ui.*
function onShutdown()    -- Called when the resource stops or before hot-reload
```

---

## Implemented Namespaces (`v1.5.0`)

### `sage`
| Function | Description |
| :--- | :--- |
| `sage.log(message)` | Prints a prefixed `[Lua]` message to the SAGE console. |
| `sage.getTime()` | Returns elapsed seconds since engine startup. |
| `sage.setTitle(title)` | Sets custom HUD text in the OS window title bar (FPS is appended automatically). |
| `sage.quit()` | Requests a clean engine shutdown (ideal for Quit buttons in Lua Main/Pause Menus). |
| `sage.saveData(key, content)` | Persists a string/JSON save file safely under `saves/<key>.dat` (ideal for high scores, unlocks & game saves). |
| `sage.loadData(key)` | Reads previously saved string data from `saves/<key>.dat` (returns `""` if not found). |

### `input`
| Function | Description |
| :--- | :--- |
| `input.isKeyDown(keyName)` | Returns `true` while a key is held (`"W"`, `"A"`, `"S"`, `"D"`, `"Space"`, `"LeftShift"`, `"LeftControl"`, `"Tab"`, `"Enter"`, `"Escape"`, `"F1"`–`"F12"`). Automatically pauses while typing in Dear ImGui. |
| `input.wasKeyPressed(keyName)` | Returns `true` on the frame a key was pressed. |
| `input.wasKeyReleased(keyName)` | Returns `true` on the frame a key was released. |
| `input.isMouseDown(buttonName)` | Returns `true` while `"Left"`, `"Right"`, or `"Middle"` mouse button is held (paused when hovering Dear ImGui). |
| `input.getMouseDelta()` | Returns `dx, dy` relative mouse movement for the current frame. |

### `world`
Right-handed 3D coordinates in meters: **`+Y` is up**, **`+Z` is forward** (`yaw = 0`), **`+X` is left**.

| Function | Description |
| :--- | :--- |
| `world.spawnModel(id, path, x, y, z)` | Loads & caches a `.glb` / `.gltf` 3D model from `assets/models/` and returns `id`. |
| `world.spawnBox(id, x, y, z)` | Spawns a 3D unit cube entity and returns `id`. |
| `world.spawnPlane(id, x, y, z)` | Spawns a 3D horizontal plane entity (with procedural surface micro-grain) and returns `id`. |
| `world.spawnWater(id, x, y, z)` | Spawns an animated 3D ocean water surface (`56×56` subdivided grid with multi-wave displacement, Fresnel sky reflection, sun glints, and shoreline surf foam) and returns `id`. |
| `world.spawnSphere(id, x, y, z)` | Spawns a 3D UV-sphere entity and returns `id`. |
| `world.spawnTriangle(id, x, y, z)` | Spawns a 3D extruded pointer chevron (points along `+Z` at `yaw = 0`). |
| `world.spawnRing(id, x, y, z)` | Spawns a glowing 3D pulsing checkpoint beacon ring. |
| `world.setPosition(id, x, y, z)` | Moves an entity in 3D space. |
| `world.getPosition(id)` | Returns `x, y, z` for an entity. |
| `world.setRotation(id, pitch, yaw, roll)` | Sets Euler rotation in degrees (`yaw` around `+Y`, `pitch` around `+X`, `roll` around `+Z`). |
| `world.setScale(id, sx, sy, sz)` | Sets 3D entity scale in meters. |
| `world.setColor(id, r, g, b)` | Sets RGB albedo for primitives, or tint multiplier for `.glb` models (`1, 1, 1` preserves original PBR colors). |
| `world.setEmissive(id, enabled)` | Enables or disables glowing emissive shading on an entity. |
| `world.setVisible(id, visible)` | Shows or hides an entity without destroying it. |
| `world.setTimeOfDay(hours)` | Dynamically updates sun angle, sky gradient, cloud lighting, ambient fill, and horizon fog (`0.0`–`24.0`). |
| `world.setBloom(enabled [, intensity])` | Toggles real-time HDR `RGBA16F` Bloom post-processing and optionally sets bloom strength (`0.0`–`2.0`, default `0.55`). |
| `world.setFXAA(enabled)` | Enables or disables GPU FXAA 3.11 edge-directed anti-aliasing in the HDR composite pass (`true` by default). |
| `world.setSSAO(enabled)` | Enables or disables Screen-Space Depth Contact Ambient Occlusion (`true` by default). |
| `world.setSSR(enabled [, intensity])` | Toggles Real-Time Screen-Space Reflections (SSR) for glossy wet roads & metallic vehicles and optionally sets reflection strength (`0.0`–`1.2`, default `0.62`). |
| `world.setGrassEnabled(enabled)` | Enables or disables the single-draw-call GPU-instanced wind-swaying 3D grass & wildflower field (`true` by default). |
| `world.setInstancingEnabled(enabled)` | Enables or disables automatic GPU-instanced batching (`glDrawElementsInstanced`) for repeated static `.glb` props across both Shadow & PBR passes (`true` by default). |
| `world.setGrassInteractPos(x, y, z)` | Updates the real-time 3D world position (player or active vehicle) that bends and parts nearby 3D grass blades smoothly as you move through meadows. |
| `world.setLookAt(id, tx, ty, tz [, weight])` | Enables real-time procedural Head, Neck & Chest Look-At IK on a rigged character toward `(tx, ty, tz)` (`weight` `0.0`–`1.0`, default `0.85`). |
| `world.clearLookAt(id)` | Disables procedural Look-At IK on an entity so its skeletal animation controls the head/torso naturally. |
| `world.triggerRagdoll(id, ix, iy, iz [, duration])` | Launches a rigged character into an Euphoria-style active-physics ragdoll tumble with velocity impulse `(ix, iy, iz)` for `duration` seconds (default `2.4`), bouncing on 3D terrain and smoothly blending back into standing animation on recovery. |
| `world.getRagdollState(id)` | Returns `active, blend01` indicating whether an entity is currently ragdolling or blending out of recovery (`blend01` in `0..1`). |
| `world.evalSpline(pointsTable, t [, closed])` | Evaluates a smooth C1 3D Catmull-Rom spline (`pointsTable` = list of `{x, y, z}` waypoints, `t` in `0..1`, `closed` default `true`) and returns `x, y, z, tanX, tanY, tanZ, yawDeg`. |
| `world.setGodRays(enabled [, intensity])` | Toggles Screen-Space Volumetric Sun Shafts (Crepuscular God-Rays) and optionally sets intensity (`0.0`–`1.5`, default `0.42`). |
| `world.setCloudShadows(enabled)` | Toggles drifting FBM cloud shadows cast across terrain, mountains, roads, and city blocks (`true` by default). |
| `world.setTerrainEnabled(enabled)` | Enables or disables the synchronized 160×160 3D multi-biome heightmap terrain (`true` by default). |
| `world.sampleTerrain(x, z)` | Samples the synchronized 3D heightmap terrain at `(x, z)` and returns `height, normalX, normalY, normalZ, biomeId` (exact match to GPU mesh & Jolt HeightField). |
| `world.setWeather(rainIntensity [, wetness])` | Controls real-time 3D falling rain (`0.0`–`1.0`) and reflective wet-puddle road/terrain shading (`0.0`–`1.0`). |
| `world.setPointLight(id, x, y, z, r, g, b [, radius, intensity])` | Creates or updates a real-time dynamic point light (up to 16 active) that illuminates 3D meshes, roads, and reflects off animated ocean waves. |
| `world.removePointLight(id)` | Removes a dynamic point light by `id`. |
| `world.spawnParticles(x, y, z, vx, vy, vz, r, g, b, count [, size, life, emissive, gravity])` | Emits a burst of 3D particles (water splashes, tire drift smoke, nitro sparks, debris) with optional HDR emission and gravity. |
| `world.sampleWater(x, z)` | Queries animated water volumes at `(x, z)` and returns `inWater, waveSurfaceY, seabedY, waterDepth, shoreDist` (exact match to the GPU wave shader). |
| `world.playAnimation(id [, clipName, loop, speed, blendSeconds])` | Plays a glTF 2.0 keyframe or skeletal-skinned animation clip on a `.glb` model with smooth bone-level crossfade blending (default `blendSeconds = 0.18`). |
| `world.stopAnimation(id)` | Pauses glTF animation playback on an entity. |
| `world.setAnimationSpeed(id, speed)` | Adjusts glTF animation playback speed multiplier. |
| `world.destroy(id)` | Removes an entity from the world. |
| `world.clear()` | Removes all entities from the world. |

### `camera`
| Function | Description |
| :--- | :--- |
| `camera.setPosition(x, y, z)` | Sets the 3D camera world position. |
| `camera.setTarget(x, y, z)` | Sets the 3D point the camera looks at (2-arg `setTarget(x, z)` also supported). |
| `camera.setFov(fovDegrees)` | Sets vertical perspective Field of View (`25.0`–`110.0` degrees). |

### `physics` *(Jolt Physics 5.7)*
| Function | Description |
| :--- | :--- |
| `physics.addStaticBox(id, px, py, pz, hx, hy, hz)` | Registers an immovable 3D box collider (ground, walls, buildings) with half-extents `(hx, hy, hz)`. |
| `physics.addStaticDeck(id, px, py, pz, hx, hy, hz [, yawDeg])` | Registers a drivable elevated deck/pier/bridge surface (solid for Jolt objects and sampled by `sampleRampSurface`, without blocking vehicle entry horizontally). |
| `physics.addStaticCylinder(id, px, py, pz, radius, height)` | Registers a solid vertical cylinder collider (tree trunks, street lamps, wind turbine masts, lighthouses). |
| `physics.addStaticRamp(id, px, py, pz, w, h, l, yawDeg)` | Registers a 3D convex-hull ramp wedge collider so physics objects can roll up and launch off ramps. |
| `physics.addDynamicBox(id, px, py, pz, hx, hy, hz [, mass, restitution, friction])` | Attaches a full 3D tumbling box rigid body to `id`. |
| `physics.addDynamicSphere(id, px, py, pz, radius [, mass, restitution, friction])` | Attaches a rolling, bouncing 3D sphere rigid body to `id`. |
| `physics.addDynamicCapsule(id, px, py, pz, radius, halfHeight [, mass, restitution, friction])` | Attaches a 3D capsule rigid body (cones, barrels, characters) to `id`. |
| `physics.addKinematicBox(id, px, py, pz, hx, hy, hz)` | Registers a script-driven moving collider (e.g., player vehicle hull) that physically transfers momentum to dynamic bodies on collision. |
| `physics.moveKinematic(id, px, py, pz, pitch, yaw, roll, dt)` | Moves a kinematic body cleanly over `dt` seconds so Jolt computes exact impact velocity. |
| `physics.applyImpulse(id, ix, iy, iz)` | Applies an instant linear impulse (N·s) to a dynamic rigid body. |
| `physics.applyExplosion(cx, cy, cz, radius, strength)` | Blasts all dynamic bodies within `radius` outward and upward with angular spin; returns count of affected bodies. |
| `physics.getSpeed(id)` | Returns the current linear speed (`m/s`) of a physics body. |
| `physics.setGravity(gx, gy, gz)` | Sets world gravity vector (default `0, -22, 0`; try `0, -5, 0` for Moon gravity!). |
| `physics.raycast(ox, oy, oz, dx, dy, dz, maxDist)` | Casts a 3D physics ray and returns `hit, hx, hy, hz, entityId`. |
| `physics.resolveCircleXZ(x, y, z, radius, vx, vz [, bounce])` | Resolves a horizontal circle (car or character) against ALL static boxes & cylinders (including JSON auto-colliders!) and returns `hit, newX, newZ, newVx, newVz, hitEntityId`. |
| `physics.sampleRampSurface(x, z)` | Samples elevation across ALL registered 3D ramps and returns `onRamp, groundY, rampHeight, rampYawDeg`. |

### `audio` *(miniaudio 0.11)*
| Function | Description |
| :--- | :--- |
| `audio.setVehicleProfile(profile)` | Selects the active combustion engine profile (`0` = Supercar V12, `1` = Superbike I4, `2` = Marine Speedboat V8 + water wake, `3` = Sports Coupe V8). |
| `audio.setVehicleState(rpm01, throttle01, drift01, boost01, airborne)` | Drives the real-time combustion engine + exhaust formant synth, turbo whine, aerodynamic wind/wake rush, and filtered tire drift squeal (fades cleanly to silence when idle/off). |
| `audio.playShiftPop([intensity01])` | Triggers a crisp exhaust gear-upshift crackle / backfire pop (`0.2`–`1.0`). |
| `audio.playFootstep([surface, intensity01])` | Plays a procedural surface-aware footstep/landing sound (`0` = Asphalt, `1` = Grass/Dirt, `2` = Wood Pier, `3` = Water Splash; `intensity01` `0.15`–`1.0`). |
| `audio.playImpact([intensity01])` | Plays a dynamic collision thud/smash SFX (`0.1`–`1.0`). |
| `audio.playChime()` | Plays an ascending harmonic reward chord (deliveries & stunt jumps). |
| `audio.playExplosion()` | Plays a sub-bass shockwave boom SFX. |
| `audio.setVolume(vol01)` | Sets master audio volume (`0.0`–`1.0`). |
| `audio.setListener(x, y, z, fwdX, fwdZ)` | Updates the 3D spatial audio listener position & horizontal forward direction (typically your active camera) for equal-power stereo panning. |
| `audio.setSpatialEmitter(slot, type, x, y, z [, radius, gain])` | Configures one of 8 live 3D spatial stereo-panned world emitters (`slot` `0`–`7`; `type`: `0` = Ocean Surf, `1` = Plaza Fountain, `2` = Helicopter Rotor, `3` = Alpine Wind/Breeze). |

### `ui` *(Lua-Driven Game Menus & HUDs — call inside `onRenderUI()`)*
| Function | Description |
| :--- | :--- |
| `ui.getViewportSize()` | Returns current window `width, height` in pixels (for responsive corner HUDs). |
| `ui.beginCenteredWindow(title, w, h)` | Opens a centered modal menu window (ideal for Main Menu / Pause Menu). |
| `ui.beginWindow(title, x, y, w, h [, overlay])` | Opens a positioned HUD/menu window and returns `true` if visible. |
| `ui.endWindow()` | Closes the current HUD/menu window (always pair with `beginWindow` / `beginCenteredWindow`). |
| `ui.text(message)` | Draws a line of text. |
| `ui.textCentered(message)` | Draws horizontally centered text inside the current window. |
| `ui.setFontScale(scale)` | Scales window font size (`0.5`–`3.0`) for menu headings. |
| `ui.progressBar(fraction01 [, label])` | Draws a progress bar (RPM, Nitro, Health, Loading). |
| `ui.button(label [, width, height])` | Draws a clickable button and returns `true` on click. |
| `ui.sameLine()` | Places the next widget on the same horizontal row. |
| `ui.spacing()` | Adds vertical spacing between widgets. |
| `ui.separator()` | Draws a horizontal divider line. |
| `ui.drawRect(x, y, w, h, r, g, b, a [, filled, rounding])` | Draws a 2D filled or outlined rectangle inside the current `ui` window (great for minimaps, radar zones & HUD panels). |
| `ui.drawCircle(cx, cy, radius, r, g, b, a [, filled])` | Draws a 2D filled or outlined circle inside the current `ui` window (great for radar blips & crosshairs). |
| `ui.drawLine(x1, y1, x2, y2, r, g, b, a [, thickness])` | Draws a 2D line segment inside the current `ui` window (great for minimap roads & compasses). |

---

## Copy-Paste Cookbook (Common Game Patterns)

### 1. Spawn a Rolling Physics Ball & Kick It
```lua
world.spawnModel("soccer_ball", "assets/models/stunt_ball.glb", 0, 4, 10)
physics.addDynamicSphere("soccer_ball", 0, 4, 10, 1.25, 28.0, 0.75, 0.35)
physics.applyImpulse("soccer_ball", 0.0, 120.0, 260.0)
```

### 2. Play a Blender `.glb` Keyframe Animation
```lua
world.spawnModel("drone", "assets/models/hover_drone.glb", 0, 6, 0)
world.playAnimation("drone", "", true, 1.5) -- "" plays all animation tracks, looping at 1.5x speed
```

### 3. Cast a 3D Physics Ray (Ground Check / Shooting / Interaction)
```lua
local hit, hx, hy, hz, entityId = physics.raycast(0, 10, 0, 0, -1, 0, 50.0)
if hit then
    sage.log("Ray hit " .. entityId .. " at y=" .. string.format("%.2f", hy))
end
```

### 4. Draw a Custom In-Game HUD Window
```lua
function onRenderUI()
    if ui.beginWindow("Player HUD", 20, 20, 220, 95, true) then
        ui.text("Health")
        ui.progressBar(0.85, "85 / 100")
        if ui.button("Heal") then
            audio.playChime()
        end
    end
    ui.endWindow()
end
```
