#!/usr/bin/env python3
"""SAGE Engine MCP server: lets AI assistants (Claude Code, Claude Desktop, ...) inspect and
control a RUNNING SAGE game through the Model Context Protocol.

    AI assistant  <--MCP (stdio, JSON-RPC)-->  this script  <--TCP 127.0.0.1-->  sage.exe --remote

* No dependencies: Python 3.8+ standard library only.
* The game must run with --remote (the `launch_game` tool can start it for you).
* Claude Code picks this server up automatically from the project's .mcp.json.
* Environment: SAGE_REMOTE_PORT (default 47800), SAGE_EXE (default <project>/build/sage.exe).

Protocol reference: https://modelcontextprotocol.io/specification
Engine commands: docs/mcp.md
"""
import base64
import json
import os
import socket
import subprocess
import sys
import time

SERVER_NAME = "sage-engine"
SERVER_VERSION = "1.7.0"
SUPPORTED_PROTOCOL_VERSIONS = ["2025-06-18", "2025-03-26", "2024-11-05"]
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PORT = int(os.environ.get("SAGE_REMOTE_PORT", "47800"))

INSTRUCTIONS = (
    "Tools for a running SAGE Engine game (C++20 + Lua). If tools report the game isn't "
    "running, call launch_game first. Coordinates are metres, +Y is up. Entities are referred "
    "to by name (id). Use screenshot to SEE the result of changes (pass camera_position/"
    "camera_target for a specific view). run_lua runs sandboxed Lua with the full game API "
    "(see docs/lua_api.md). Lua scripts that move an entity or the camera every tick will "
    "override set_transform/set_camera."
)


def log(*args):
    # stdout carries the MCP protocol only; diagnostics go to stderr.
    print("[sage-mcp]", *args, file=sys.stderr, flush=True)


# ----------------------------------------------------------------------------- engine link
class EngineLink:
    """Line-delimited JSON request/response over a localhost socket to sage.exe --remote."""

    def __init__(self, port):
        self.port = port
        self.sock = None
        self.file = None
        self.next_id = 0

    def connected(self):
        return self.sock is not None

    def connect(self, timeout=2.0):
        self.close()
        try:
            self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=timeout)
            self.sock.settimeout(30.0)
            self.file = self.sock.makefile("rwb")
            return True
        except OSError:
            self.sock = None
            return False

    def close(self):
        try:
            if self.sock:
                self.sock.close()
        except OSError:
            pass
        self.sock = None
        self.file = None

    def call(self, method, params=None):
        for attempt in range(2):  # Reconnect once if the game restarted
            if not self.connected() and not self.connect():
                raise ToolError(
                    f"SAGE isn't running with remote control on port {self.port}. "
                    "Call the launch_game tool, or start the game with: sage.exe --remote")
            self.next_id += 1
            request = {"id": self.next_id, "method": method, "params": params or {}}
            try:
                self.file.write((json.dumps(request) + "\n").encode("utf-8"))
                self.file.flush()
                line = self.file.readline()
                if not line:
                    raise OSError("connection closed")
                response = json.loads(line)
                if not response.get("ok"):
                    raise ToolError(response.get("error", "unknown engine error"))
                return response.get("result")
            except (OSError, ValueError):
                self.close()
                if attempt == 1:
                    raise ToolError("Lost connection to SAGE (did the game close?). Call launch_game to restart it.")
        raise ToolError("unreachable")


class ToolError(Exception):
    """An error reported back to the AI as a tool result (isError: true), not a protocol error."""


ENGINE = EngineLink(PORT)


# ----------------------------------------------------------------------------- tools
def vec3_schema(desc):
    return {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": desc}


def text(value):
    if not isinstance(value, str):
        value = json.dumps(value, indent=2)
    return [{"type": "text", "text": value}]


def tool_launch_game(args):
    if ENGINE.connect(timeout=0.5):
        return text(f"SAGE is already running with remote control on port {PORT}.")
    exe = os.environ.get("SAGE_EXE", os.path.join(PROJECT_ROOT, "build", "sage.exe" if os.name == "nt" else "sage"))
    if not os.path.exists(exe):
        raise ToolError(f"Engine executable not found: {exe}. Build it first: cmake --build build --target sage")
    cmd = [exe, "--remote", str(PORT)]
    if args.get("survival"):
        cmd.append("--survival")
    if args.get("scene"):
        cmd += ["--scene", args["scene"]]
    if args.get("resource"):
        cmd += ["--resource", args["resource"]]
    if args.get("mute", True):
        cmd.append("--mute")
    flags = 0
    if os.name == "nt":
        flags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS
    subprocess.Popen(cmd, cwd=PROJECT_ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                     stderr=subprocess.DEVNULL, creationflags=flags)
    deadline = time.time() + 45
    while time.time() < deadline:
        if ENGINE.connect(timeout=0.5):
            info = ENGINE.call("info")
            return text({"launched": " ".join(cmd), "info": info})
        time.sleep(0.5)
    raise ToolError("The game was started but remote control didn't answer within 45 s. Check the game window/console.")


def tool_screenshot(args):
    params = {"maxWidth": int(args.get("max_width", 1024)), "format": "jpg"}
    if "camera_position" in args or "camera_target" in args:
        cam = {}
        if "camera_position" in args:
            cam["position"] = args["camera_position"]
        if "camera_target" in args:
            cam["target"] = args["camera_target"]
        if "fov" in args:
            cam["fov"] = args["fov"]
        params["camera"] = cam
    result = ENGINE.call("screenshot", params)
    with open(result["path"], "rb") as f:
        data = base64.b64encode(f.read()).decode("ascii")
    return [{"type": "image", "data": data, "mimeType": "image/jpeg"},
            {"type": "text", "text": f"Screenshot saved to {result['path']}"}]


def passthrough(method, mapping=None):
    """A tool that forwards its arguments to an engine method (renaming keys if needed)."""
    def run(args):
        params = {}
        for key, value in args.items():
            params[(mapping or {}).get(key, key)] = value
        return text(ENGINE.call(method, params))
    return run


def tool_run_lua(args):
    result = ENGINE.call("run_lua", {"code": args["code"]})
    output = result.get("output", "")
    if not result.get("ok"):
        raise ToolError(output or "Lua error")
    return text(output if output else "(ok, no output)")


TOOLS = [
    {
        "name": "launch_game",
        "description": "Start SAGE with remote control (if it isn't already running) and return engine info. "
                       "Defaults to Stunt City; set survival=true for Echo Island, or give a scene and resource.",
        "inputSchema": {"type": "object", "properties": {
            "survival": {"type": "boolean", "description": "Launch the Wild Frontier: Echo Island survival game"},
            "scene": {"type": "string", "description": "Scene JSON path, e.g. scenes/terrain_demo.json"},
            "resource": {"type": "string", "description": "Load only this Lua resource, e.g. terrain_demo"},
            "mute": {"type": "boolean", "description": "Start with audio muted (default true)"},
        }},
        "run": tool_launch_game,
    },
    {
        "name": "get_info",
        "description": "Engine version, loaded scene, entity/light/physics counts, FPS and simulation time.",
        "inputSchema": {"type": "object", "properties": {}},
        "run": passthrough("info"),
    },
    {
        "name": "list_entities",
        "description": "List entities in the world (name, shape/model, position, visibility). Filter by name prefix.",
        "inputSchema": {"type": "object", "properties": {
            "prefix": {"type": "string", "description": "Only names starting with this"},
            "limit": {"type": "integer", "description": "Max entities to return (default 200)"},
        }},
        "run": passthrough("list_entities"),
    },
    {
        "name": "get_entity",
        "description": "Full details of one entity: transform, colour, animation, look-at/ragdoll components, ground height below it.",
        "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}}, "required": ["id"]},
        "run": passthrough("get_entity"),
    },
    {
        "name": "spawn",
        "description": "Create (or replace) an entity: a .glb model (e.g. assets/models/pine_tree.glb) or a primitive "
                       "shape (box, sphere, plane, ring, triangle, water).",
        "inputSchema": {"type": "object", "properties": {
            "id": {"type": "string", "description": "Unique name"},
            "position": vec3_schema("[x, y, z] in metres"),
            "model": {"type": "string", "description": "Model path, e.g. assets/models/crate.glb"},
            "shape": {"type": "string", "enum": ["box", "sphere", "plane", "ring", "triangle", "water"]},
            "scale": vec3_schema("[x, y, z] scale (default [1, 1, 1])"),
            "rotation": vec3_schema("[pitch, yaw, roll] in degrees"),
            "color": vec3_schema("[r, g, b] 0..1 (primitives only)"),
            "snap_to_ground": {"type": "boolean",
                               "description": "Place it on the terrain: position y becomes height above the ground"},
        }, "required": ["id", "position"]},
        "run": passthrough("spawn", {"snap_to_ground": "snapToGround"}),
    },
    {
        "name": "set_transform",
        "description": "Move, rotate and/or scale an entity (moves are instant, not interpolated).",
        "inputSchema": {"type": "object", "properties": {
            "id": {"type": "string"},
            "position": vec3_schema("[x, y, z]"),
            "rotation": vec3_schema("[pitch, yaw, roll] degrees"),
            "scale": vec3_schema("[x, y, z]"),
        }, "required": ["id"]},
        "run": passthrough("set_transform"),
    },
    {
        "name": "set_visible",
        "description": "Show or hide an entity.",
        "inputSchema": {"type": "object", "properties": {
            "id": {"type": "string"}, "visible": {"type": "boolean"}}, "required": ["id", "visible"]},
        "run": passthrough("set_visible"),
    },
    {
        "name": "destroy",
        "description": "Remove an entity from the world.",
        "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}}, "required": ["id"]},
        "run": passthrough("destroy"),
    },
    {
        "name": "get_camera",
        "description": "Current game camera position, look-at target and field of view.",
        "inputSchema": {"type": "object", "properties": {}},
        "run": passthrough("get_camera"),
    },
    {
        "name": "set_camera",
        "description": "Move the game camera. Games that drive the camera every tick will override it; "
                       "for a one-off view use screenshot's camera_position/camera_target instead.",
        "inputSchema": {"type": "object", "properties": {
            "position": vec3_schema("[x, y, z]"), "target": vec3_schema("[x, y, z] look-at point"),
            "fov": {"type": "number", "description": "Vertical field of view in degrees (25-110)"},
        }},
        "run": passthrough("set_camera"),
    },
    {
        "name": "screenshot",
        "description": "Capture the 3D view (no UI overlay) and return it as an image you can see. Optionally "
                       "render from a specific camera without moving the game camera.",
        "inputSchema": {"type": "object", "properties": {
            "camera_position": vec3_schema("[x, y, z] to render from"),
            "camera_target": vec3_schema("[x, y, z] to look at"),
            "fov": {"type": "number", "description": "Field of view in degrees"},
            "max_width": {"type": "integer", "description": "Max image width in pixels (default 1024)"},
        }},
        "run": tool_screenshot,
    },
    {
        "name": "get_logs",
        "description": "Recent engine log lines (script errors, asset/shader problems, hot-reload messages). "
                       "Pass 'after' (a previous lastSeq) to get only new lines.",
        "inputSchema": {"type": "object", "properties": {
            "after": {"type": "integer"}, "limit": {"type": "integer", "description": "Default 200"},
            "errors_only": {"type": "boolean"},
        }},
        "run": passthrough("get_logs", {"errors_only": "errorsOnly"}),
    },
    {
        "name": "get_performance",
        "description": "Live frame time, FPS, CPU sections, GPU passes and draw calls (single-frame snapshot).",
        "inputSchema": {"type": "object", "properties": {}},
        "run": passthrough("get_performance"),
    },
    {
        "name": "run_lua",
        "description": "Run sandboxed Lua in the game with the full SAGE API (world, physics, camera, audio, ui, sage). "
                       "Expressions return their value, e.g. world.getPosition('player_car'). print() output is "
                       "returned. 1 s time limit.",
        "inputSchema": {"type": "object", "properties": {"code": {"type": "string"}}, "required": ["code"]},
        "run": tool_run_lua,
    },
    {
        "name": "reload",
        "description": "Hot-reload all Lua resources and the scene (like pressing F5).",
        "inputSchema": {"type": "object", "properties": {}},
        "run": passthrough("reload"),
    },
]
TOOLS_BY_NAME = {t["name"]: t for t in TOOLS}


# ----------------------------------------------------------------------------- MCP protocol
def handle(message):
    """Returns a response dict, or None for notifications."""
    method = message.get("method")
    msg_id = message.get("id")
    is_request = "id" in message and method is not None
    params = message.get("params") or {}

    def ok(result):
        return {"jsonrpc": "2.0", "id": msg_id, "result": result}

    def err(code, text_):
        return {"jsonrpc": "2.0", "id": msg_id, "error": {"code": code, "message": text_}}

    if method is None:
        return None  # A response from the client (we never send requests): ignore
    if method == "initialize":
        requested = params.get("protocolVersion")
        version = requested if requested in SUPPORTED_PROTOCOL_VERSIONS else SUPPORTED_PROTOCOL_VERSIONS[0]
        return ok({
            "protocolVersion": version,
            "capabilities": {"tools": {"listChanged": False}},
            "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
            "instructions": INSTRUCTIONS,
        })
    if method == "ping":
        return ok({}) if is_request else None
    if method == "tools/list":
        return ok({"tools": [{k: v for k, v in t.items() if k != "run"} for t in TOOLS]})
    if method == "tools/call":
        tool = TOOLS_BY_NAME.get(params.get("name"))
        if tool is None:
            return err(-32602, f"Unknown tool: {params.get('name')}")
        try:
            content = tool["run"](params.get("arguments") or {})
            return ok({"content": content, "isError": False})
        except ToolError as e:
            return ok({"content": text(str(e)), "isError": True})
        except Exception as e:  # Never crash the server on a bad tool call
            log("tool failed:", repr(e))
            return ok({"content": text(f"Tool failed: {e}"), "isError": True})
    if not is_request:
        return None  # notifications/initialized, notifications/cancelled, ...
    return err(-32601, f"Method not found: {method}")


def main():
    stdin = sys.stdin.buffer
    stdout = sys.stdout.buffer
    log(f"started (project {PROJECT_ROOT}, engine port {PORT})")
    for raw in stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            message = json.loads(raw)
        except ValueError:
            response = {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Parse error"}}
            stdout.write((json.dumps(response) + "\n").encode("utf-8"))
            stdout.flush()
            continue
        batch = message if isinstance(message, list) else [message]
        responses = [r for r in (handle(m) for m in batch if isinstance(m, dict)) if r is not None]
        if not responses:
            continue
        out = responses if isinstance(message, list) else responses[0]
        stdout.write((json.dumps(out) + "\n").encode("utf-8"))
        stdout.flush()


if __name__ == "__main__":
    main()
