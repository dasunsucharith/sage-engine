# SAGE Engine — Lua API Reference

> **Status:** Live (`v0.3.0`) — **Lua 5.4.9** + **`sol2` 3.5.0**, **3D Perspective + glTF 2.0 (`.glb`) Models**, **JSON Scenes**, and **Live Hot-Reload**.
> This file is the single source of truth for both human scripters and AI agents writing Lua for SAGE.

---

## FiveM-Style Resource Convention

Each gameplay feature or game lives in `resources/<resource_name>/`:

```json
{
  "name": "stunt_sandbox",
  "version": "0.3.0",
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

## Implemented Namespaces (`v0.3`)

### `sage`
| Function | Description |
| :--- | :--- |
| `sage.log(message)` | Prints a prefixed `[Lua]` message to the SAGE console. |
| `sage.getTime()` | Returns elapsed seconds since engine startup. |
| `sage.setTitle(title)` | Sets custom HUD text in the OS window title bar (FPS is appended automatically). |

### `input`
| Function | Description |
| :--- | :--- |
| `input.isKeyDown(keyName)` | Returns `true` while a key is held (case-insensitive: `"W"`, `"A"`, `"S"`, `"D"`, `"Up"`, `"Down"`, `"Left"`, `"Right"`, `"Space"`, `"LeftShift"`, `"LeftControl"`, `"Tab"`, `"Enter"`, `"Escape"`, `"F1"`–`"F12"`). |
| `input.wasKeyPressed(keyName)` | Returns `true` on the frame a key was pressed. |
| `input.wasKeyReleased(keyName)` | Returns `true` on the frame a key was released. |
| `input.getMouseDelta()` | Returns `dx, dy` relative mouse movement for the current frame. |

### `world`
Right-handed 3D coordinates in meters: **`+Y` is up**, **`+Z` is forward** (`yaw = 0`), **`+X` is left** when looking along `+Z`.

| Function | Description |
| :--- | :--- |
| `world.spawnModel(id, path, x, y, z)` | Loads & caches a `.glb` / `.gltf` 3D model from `assets/models/` (e.g. `"assets/models/sports_car.glb"`) and returns `id`. |
| `world.spawnBox(id, x, y, z)` | Spawns a 3D unit cube entity and returns `id`. |
| `world.spawnPlane(id, x, y, z)` | Spawns a 3D horizontal ground/road plane entity and returns `id`. |
| `world.spawnTriangle(id, x, y, z)` | Spawns a 3D extruded pointer arrow/chevron (points along `+Z` at `yaw = 0`). |
| `world.spawnRing(id, x, y, z)` | Spawns a glowing 3D pulsing checkpoint beacon ring. |
| `world.setPosition(id, x, y, z)` | Moves an entity in 3D space. |
| `world.getPosition(id)` | Returns `x, y, z` for an entity. |
| `world.setRotation(id, pitch, yaw, roll)` | Sets Euler rotation in degrees (`yaw` around `+Y`, `pitch` around `+X`, `roll` around `+Z`). |
| `world.setScale(id, sx, sy, sz)` | Sets 3D entity scale in meters. |
| `world.setColor(id, r, g, b)` | Sets RGB albedo for primitives, or color tint multiplier for `.glb` models (`1, 1, 1` keeps original materials). |
| `world.setEmissive(id, enabled)` | Enables or disables glowing emissive shading on an entity. |
| `world.setVisible(id, visible)` | Shows or hides an entity without destroying it. |
| `world.setTimeOfDay(hours)` | Dynamically updates sun angle, sky gradient, ambient fill, and horizon fog (`0.0`–`24.0`, e.g., `17.5` = Golden Sunset, `12.5` = Noon, `22.5` = Night). |
| `world.destroy(id)` | Removes an entity from the world. |
| `world.clear()` | Removes all entities from the world. |

### `camera`
| Function | Description |
| :--- | :--- |
| `camera.setPosition(x, y, z)` | Sets the 3D camera world position. |
| `camera.setTarget(x, y, z)` | Sets the 3D point the camera looks at (2-arg `setTarget(x, z)` also supported). |
| `camera.setFov(fovDegrees)` | Sets vertical perspective Field of View (`25.0`–`110.0` degrees). |

---

## Planned Namespaces (`v0.4+`)

| Function | Milestone | Description |
| :--- | :--- | :--- |
| `physics.addRigidBody(id, opts)` | `v0.4` | Attaches a Jolt Physics rigid body or raycast vehicle collider. |
| `audio.play3D(path, x, y, z)` | `v0.4` | Plays positional 3D sound via `miniaudio`. |
