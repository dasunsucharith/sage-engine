# 032 — MCP Bridge: AI Assistants in a Running Game

## Plain-English Summary

Before this, an AI assistant could read and edit SAGE's files, but it was blind to the running game. It couldn't see whether a tree it placed was floating or whether a script it wrote threw an error. Now an assistant can connect to the live game and use it the way a developer would: look around (screenshots it can see), list and inspect objects, spawn and move things, read the log, check performance and run Lua. That's what "AI-native" means in practice: the assistant gets feedback, not just text files.

## Technical Deep Dive

### Architecture
```text
AI assistant --MCP stdio--> tools/sage_mcp/server.py --TCP 127.0.0.1:47800, JSON lines--> sage.exe --remote
```
Two deliberately separate pieces:

1. **Engine port** (`RemoteServer` in `engine/src/remote/`, handlers in `engine_remote.cpp`): protocol-agnostic, so scripts, tests and other tools can use it too.
   * **Opt-in and local:** only runs with `--remote [port]`, and binds `127.0.0.1` only.
   * **No threads:** sockets are non-blocking and `poll()`ed once per frame on the main thread, right after the frame finishes. Commands run between frames, so they never race with rendering, physics or Lua, and there are no locks.
   * **Simple protocol:** one JSON object per line, answered with `{"id", "ok", "result" | "error"}`. Every bad input gets a readable error: unknown entity, malformed vector, unknown method (with the list of valid methods), invalid JSON.
2. **MCP server** (`tools/sage_mcp/server.py`): a single standard-library Python file (no `pip install`). It implements the MCP stdio JSON-RPC protocol: `initialize` (negotiating versions 2025-06-18 / 2025-03-26 / 2024-11-05), `tools/list` and `tools/call`, with notifications left unanswered. Tool failures come back as `isError` results the assistant can read and act on, not protocol errors. `launch_game` starts the engine if needed. `.mcp.json` registers it for Claude Code.

### Engine features added for it
* **Log capture:** `LogBuffer` tees `std::cout`/`std::cerr` into a 2,000-line ring buffer with sequence numbers (`get_logs after=N`). The console output is unchanged. A lock guards the tee because other threads log too.
* **Screenshots:** `Renderer::captureScreenshotScaled` box-downscales and writes JPEG (16–38 KB versus MB-sized PNGs). A `screenshot` with its own camera re-renders the 3D view from there, with render interpolation suppressed for the camera during that render, then restores the game camera.
* **`LuaEngine::runSnippet`:** a fresh sandboxed environment with the full game API, `print` captured, and REPL-style evaluation (tries `return <code>` first). The same 1 s watchdog applies: a `while true do end` returns an error after exactly 1.0 s and the game keeps responding.
* **`spawn` with `snapToGround`:** `y` becomes height above the terrain. Verified on Mount Echo: spawned at 73.898 m with the ground at 73.898 m.

## Verification
* **Engine protocol probe** (raw TCP, 26 calls): all 15 methods plus the error paths (unknown entity, bad vector, unknown method, malformed JSON, runaway Lua). The game stayed responsive throughout, and a screenshot from a custom camera showed the spawned objects without moving the game camera.
* **Official MCP client SDK** (`mcp` 2.3.0, throwaway venv):
  * the handshake negotiated protocol `2025-06-18`;
  * all 15 tools listed;
  * `get_info` before launch gave a helpful error, then `launch_game` started Echo Island;
  * spawn, inspect, Lua queries, valid JPEG image content, logs, performance and destroy all worked;
  * an unknown tool gave a protocol error, and `sage.quit()` closed the game.
* **No regressions:** the existing 14 automated tests and both games' smoke tests pass.

## How SAGE Uses This
* [`docs/mcp.md`](../mcp.md) is the user guide and protocol reference. `AGENTS.md` now tells AI agents to verify their changes in the running game (reload → screenshot → logs).
* **Next steps:**
  * a `pause`/`step N ticks` command for deterministic gameplay testing;
  * a "save the live changes into the scene JSON" tool;
  * a headless test target for the remote dispatcher.
