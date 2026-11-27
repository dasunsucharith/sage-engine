# SAGE Engine — Lua API Reference

> **Status:** Planning / Blueprint (`v0.0.1`). Live Lua execution via `sol2` arrives in **Milestone v0.2**.
> This file is the single source of truth for both human scripters and AI agents writing Lua for SAGE.

---

## Lifecycle Callbacks (FiveM-Style)

Every resource script in `resources/<name>/main.lua` can define these automatic callbacks:

```lua
function onStart()       -- Called once when the resource loads
function onTick(dt)      -- Called every frame (dt = seconds since last frame)
function onShutdown()    -- Called when the resource stops or reloads
```

---

## Planned Core Namespaces

### `sage` *(v0.2)*
| Function | Description |
| :--- | :--- |
| `sage.log(message)` | Prints a message to the SAGE console and log file. |
| `sage.getTime()` | Returns total elapsed seconds since engine startup. |

### `input` *(v0.2)*
| Function | Description |
| :--- | :--- |
| `input.isKeyDown(keyName)` | Returns `true` while a key (e.g. `"W"`, `"Space"`) is held. |
| `input.getMouseDelta()` | Returns `dx, dy` mouse movement for camera control. |

### `world` *(v0.3)*
| Function | Description |
| :--- | :--- |
| `world.spawnModel(path, pos)` | Loads a `.glb`/`.gltf` model from `assets/models/` and returns an entity ID. |
| `world.setPosition(id, x, y, z)` | Moves an entity in 3D space. |
| `world.setRotation(id, pitch, yaw, roll)` | Rotates an entity in degrees. |
| `world.destroy(id)` | Removes an entity from the scene. |
