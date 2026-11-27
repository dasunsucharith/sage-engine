# SAGE Engine — Lua API Reference

> **Status:** Live (`v0.4.0`) — **Lua 5.4.9** + **`sol2` 3.5.0**, **3D glTF 2.0 (`.glb`)**, **Jolt Physics 5.7**, **miniaudio 0.11**, **Dear ImGui 1.92**, and **Live Hot-Reload**.
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
function onShutdown()    -- Called when the resource stops or before hot-reload
```

---

## Implemented Namespaces (`v0.4`)

### `sage`
| Function | Description |
| :--- | :--- |
| `sage.log(message)` | Prints a prefixed `[Lua]` message to the SAGE console. |
| `sage.getTime()` | Returns elapsed seconds since engine startup. |
| `sage.setTitle(title)` | Sets custom HUD text in the OS window title bar (FPS is appended automatically). |

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
| `world.spawnPlane(id, x, y, z)` | Spawns a 3D horizontal plane entity and returns `id`. |
| `world.spawnTriangle(id, x, y, z)` | Spawns a 3D extruded pointer chevron (points along `+Z` at `yaw = 0`). |
| `world.spawnRing(id, x, y, z)` | Spawns a glowing 3D pulsing checkpoint beacon ring. |
| `world.setPosition(id, x, y, z)` | Moves an entity in 3D space. |
| `world.getPosition(id)` | Returns `x, y, z` for an entity. |
| `world.setRotation(id, pitch, yaw, roll)` | Sets Euler rotation in degrees (`yaw` around `+Y`, `pitch` around `+X`, `roll` around `+Z`). |
| `world.setScale(id, sx, sy, sz)` | Sets 3D entity scale in meters. |
| `world.setColor(id, r, g, b)` | Sets RGB albedo for primitives, or tint multiplier for `.glb` models (`1, 1, 1` preserves original PBR colors). |
| `world.setEmissive(id, enabled)` | Enables or disables glowing emissive shading on an entity. |
| `world.setVisible(id, visible)` | Shows or hides an entity without destroying it. |
| `world.setTimeOfDay(hours)` | Dynamically updates sun angle, sky gradient, ambient fill, and horizon fog (`0.0`–`24.0`). |
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
| `physics.addDynamicBox(id, px, py, pz, hx, hy, hz [, mass, restitution, friction])` | Attaches a full 3D tumbling rigid body to `id`; automatically syncs 3D position & quaternion back to `world` every 60 Hz step. |
| `physics.addKinematicBox(id, px, py, pz, hx, hy, hz)` | Registers a script-driven moving collider (e.g., player vehicle hull) that physically transfers momentum to dynamic bodies on collision. |
| `physics.moveKinematic(id, px, py, pz, pitch, yaw, roll, dt)` | Moves a kinematic body cleanly over `dt` seconds so Jolt computes exact impact velocity. |
| `physics.applyImpulse(id, ix, iy, iz)` | Applies an instant linear impulse (N·s) to a dynamic rigid body. |
| `physics.applyExplosion(cx, cy, cz, radius, strength)` | Blasts all dynamic bodies within `radius` outward and upward with angular spin; returns count of affected bodies. |
| `physics.getSpeed(id)` | Returns the current linear speed (`m/s`) of a physics body. |
| `physics.setGravity(gx, gy, gz)` | Sets world gravity vector (default `0, -22, 0`; try `0, -5, 0` for Moon gravity!). |
| `physics.raycast(ox, oy, oz, dx, dy, dz, maxDist)` | Casts a 3D physics ray and returns `hit, hx, hy, hz, entityId`. |

### `audio` *(miniaudio 0.11)*
| Function | Description |
| :--- | :--- |
| `audio.setVehicleState(rpm01, throttle01, drift01, boost01, airborne)` | Drives the real-time V12 harmonic engine synth, tire drift screech, and nitro afterburner roar. |
| `audio.playImpact([intensity01])` | Plays a dynamic collision thud/smash SFX (`0.1`–`1.0`). |
| `audio.playChime()` | Plays an ascending harmonic reward chord (deliveries & stunt jumps). |
| `audio.playExplosion()` | Plays a sub-bass shockwave boom SFX. |
| `audio.setVolume(vol01)` | Sets master audio volume (`0.0`–`1.0`). |
