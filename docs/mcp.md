# AI Assistants in a Running Game (MCP Bridge)

SAGE lets AI assistants such as **Claude Code** or **Claude Desktop** work with a **running game**, not just its files: list and inspect objects, spawn and move things, steer the camera, take screenshots they can actually see, read the engine log, check performance and run Lua. It uses the [Model Context Protocol (MCP)](https://modelcontextprotocol.io), the standard way AI assistants connect to tools.

```text
AI assistant  <--MCP (stdio)-->  tools/sage_mcp/server.py  <--TCP 127.0.0.1:47800-->  sage.exe --remote
```

* The MCP server is a single Python file with **no dependencies** (Python 3.8+ standard library).
* The game only accepts connections when started with `--remote`, and **only from your own machine** (it listens on `127.0.0.1`, never on the network).

---

## Quick start (Claude Code)

1. Build the engine (see [Getting Started](getting_started.md)).
2. Open Claude Code in the project folder. The project's [`.mcp.json`](../.mcp.json) registers the `sage` server automatically; approve it when asked.
3. Ask things like:
   * *"Launch the survival game and show me a screenshot of the lake."*
   * *"Put five pine trees on the hill north of the cabin, then show me from above."*
   * *"Why is the frame rate low? Check performance and the log for errors."*
   * *"Spawn a red box at the plaza, make it 3 m tall, and check it isn't floating."*

The assistant calls `launch_game` itself if the game isn't running. To start it yourself instead:

```powershell
.\build\sage.exe --remote
```

`--remote` combines with every other flag, e.g. `--survival --remote`, or `--remote 47900` for a different port (then set `SAGE_REMOTE_PORT=47900` for the MCP server).

## Claude Desktop (or other MCP clients)

Add this to the client's MCP configuration, using the full path to your project:

```json
{
  "mcpServers": {
    "sage": {
      "command": "python",
      "args": ["D:/Projects/sage-engine/tools/sage_mcp/server.py"]
    }
  }
}
```

---

## Tools

| Tool | What it does |
| :--- | :--- |
| `launch_game` | Starts SAGE with `--remote` if it isn't running (`survival`, `scene`, `resource`, `mute` options). |
| `get_info` | Engine version, scene, entity/light/physics counts, FPS, simulation time. |
| `list_entities` | Names, shapes/models, positions and visibility (`prefix` filter, `limit`). |
| `get_entity` | One entity in full: transform, colour, animation, look-at/ragdoll components, ground height below it. |
| `spawn` | Create an entity from a `.glb` model or a primitive shape. `snap_to_ground` puts it on the terrain. |
| `set_transform` | Move, rotate and/or scale an entity (instant, no interpolation slide). |
| `set_visible` / `destroy` | Show, hide or remove an entity. |
| `get_camera` / `set_camera` | Read or move the game camera. |
| `screenshot` | Returns the 3D view as an **image the assistant can see** (JPEG, default ≤1024 px wide). `camera_position` + `camera_target` render from any viewpoint **without** moving the game camera. |
| `get_logs` | Recent engine log lines (`after` = only newer lines, `errors_only`). |
| `get_performance` | Live FPS, frame time, CPU sections, GPU passes, draw calls (single-frame snapshot). |
| `run_lua` | Run sandboxed Lua with the full [Lua API](lua_api.md). Expressions return their value; `print` output is returned; 1 s time limit. |
| `reload` | Hot-reload all Lua resources and the scene (like `F5`). |

**Good to know**
* **Scripts win:** a game script that moves an entity or the camera every tick overrides `set_transform` and `set_camera`. For a one-off view, pass a camera to `screenshot`.
* **Changes are live only:** entities you add or move this way exist only in the running game. Ask the assistant to also edit the scene JSON if a change should be permanent.
* **Not for benchmarking:** `get_performance` is a single frame. For real comparisons use `sage.exe --benchmark` (see [`AGENTS.md`](../AGENTS.md)).

---

## Engine protocol (for your own scripts and tools)

Anything can drive the game through the same port: test scripts, editors, other bots. Connect to `127.0.0.1:47800` and send **one JSON object per line**; each request gets one JSON line back:

```text
-> {"id": 1, "method": "spawn", "params": {"id": "crate_1", "model": "assets/models/crate.glb", "position": [3, 0, 5], "snapToGround": true}}
<- {"id": 1, "ok": true, "result": {"id": "crate_1", "shape": "model", "model": "assets/models/crate.glb", "position": [3.0, 0.0, 5.0], "visible": true}}
-> {"id": 2, "method": "get_entity", "params": {"id": "nope"}}
<- {"id": 2, "ok": false, "error": "no entity named 'nope' (use list_entities to see names)"}
```

| Method | Params |
| :--- | :--- |
| `ping`, `info`, `get_camera`, `get_performance`, `reload` | none |
| `list_entities` | `prefix?`, `limit?` (default 200) |
| `get_entity`, `destroy` | `id` |
| `spawn` | `id`, `position [x,y,z]`, `model?` or `shape?` (box, sphere, plane, ring, triangle, water), `scale?`, `rotation? [pitch,yaw,roll]°`, `color? [r,g,b]`, `snapToGround?` |
| `set_transform` | `id`, `position?`, `rotation?`, `scale?`, `teleport?` (default true) |
| `set_visible` | `id`, `visible` |
| `set_camera` | `position?`, `target?`, `fov?` |
| `screenshot` | `maxWidth?` (default 1024, 0 = full size), `format?` (`jpg`/`png`), `camera? {position, target, fov?}`. Returns `{path}` to the saved image. |
| `get_logs` | `after?`, `limit?`, `errorsOnly?` |
| `run_lua` | `code`. Returns `{ok, output}`. |

Commands run on the engine's main thread **between frames**, so they never race with rendering, physics or Lua.

---

## Security

* **Off by default:** remote control only runs when you pass `--remote`.
* **Localhost only:** the port binds to `127.0.0.1`, so other machines can't connect. Programs running on your own computer can, so don't use `--remote` on a shared machine.
* **Sandboxed Lua:** `run_lua` uses the same sandbox as game scripts (no file access, no `io`/`os`, 1 s time limit).

## Troubleshooting

| Problem | Fix |
| :--- | :--- |
| "SAGE isn't running with remote control" | Call `launch_game`, or start `sage.exe --remote`. |
| "Could not listen on 127.0.0.1:47800" | Another SAGE instance is using the port. Close it, or use `--remote 47900` plus `SAGE_REMOTE_PORT=47900`. |
| The MCP server doesn't start | Check `python --version` works in a terminal. On some Windows setups the command is `py`: change `"command"` in `.mcp.json`. |
| "Engine executable not found" | Build first (`cmake --build build --target sage`), or set `SAGE_EXE` to your `sage.exe`. |
