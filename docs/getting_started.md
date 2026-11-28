# Getting Started with SAGE Engine

This guide gets you from a fresh `git clone` to a running **3D SAGE Engine** window in just a few minutes — with **zero manual `.dll` or `.lib` hunting** (CMake automatically downloads and builds every dependency from source).

---

## 1. Prerequisites

| Platform | Required Tools |
| :--- | :--- |
| **Windows 10/11** | **Visual Studio 2022 or 2026 Build Tools** (*"Desktop development with C++"* workload), **CMake 3.24+**, **Ninja**, **Git** |
| **Linux (Ubuntu/Debian)** | `g++` (GCC 11+) or `clang++`, `cmake`, `ninja-build`, `git`, `libgl1-mesa-dev`, `libx11-dev` |
| **macOS** | Xcode Command Line Tools (`xcode-select --install`), `cmake`, `ninja` |

### Quick Windows Install (`winget`)
Open **Windows Terminal / PowerShell** and run:
```powershell
winget install --id Kitware.CMake --exact
winget install --id Ninja-build.Ninja --exact
winget install --id Microsoft.VisualStudio.BuildTools --exact --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```
*(Restart your terminal after installation so `cmake` and `ninja` are on your `PATH`.)*

---

## 2. Clone, Build & Run

### Windows (Developer Command Prompt for VS or PowerShell)
```powershell
git clone https://github.com/dasunsucharith/sage-engine.git
cd sage-engine

# Configure (fetches SDL3, Lua 5.4, sol2, GLM, cgltf, Jolt Physics, Dear ImGui, miniaudio)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build the standalone sage.exe binary
cmake --build build --target sage

# Launch the built-in 3D Aventador SVJ Stunt & Delivery Showcase!
.\build\sage.exe
```

> [!TIP]
> On Windows, if you run from a standard PowerShell window without Visual Studio's environment loaded, either open **Developer Command Prompt for VS** or use the Visual Studio generator directly:
> ```powershell
> cmake -S . -B build
> cmake --build build --target sage --config Release
> .\build\Release\sage.exe
> ```

### Linux / macOS
```bash
git clone https://github.com/dasunsucharith/sage-engine.git
cd sage-engine
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target sage
./build/sage
```

### Run the automated tests
The test suite (`tests/`, built with [doctest](https://github.com/doctest/doctest)) checks entity handles, render interpolation, data-driven terrain, the Lua sandbox, per-resource saves and deterministic simulation. It needs no window or GPU and takes about 2 seconds:

```powershell
cmake --build build --target sage_tests
ctest --test-dir build --output-on-failure
```

The same tests run automatically on GitHub for every push and pull request (`.github/workflows/ci.yml`).

---

## 3. Command-Line Options

| Flag | Example | Description |
| :--- | :--- | :--- |
| `--scene <path>` | `.\build\sage.exe --scene scenes/sandbox.json` | Loads a custom JSON scene file instead of the default `scenes/stunt_city.json`. |
| `--mute` | `.\build\sage.exe --mute` | Launches with audio muted (no OS audio device opened). |
| `--smoke-test` | `.\build\sage.exe --smoke-test` | Initializes GPU, Jolt Physics, Dear ImGui, JSON scene, `.glb` models, and Lua scripts, renders 12 real frames, prints diagnostics, and exits `0` (ideal for CI and AI agents!). This is a **boot check, not a performance measurement**: 12 frames are dominated by the cold-start hitch. |
| `--benchmark [frames]` | `.\build\sage.exe --benchmark 600` | Steady-state performance run: turns VSync off, mutes audio, skips 120 warm-up frames, then measures `frames` frames (default 600). Prints mean/p50/p95/p99/max frame time, **the GPU time of each render pass** (shadows, opaque, grass, water, post, ...) and **the CPU time of each engine section** (lua, physics, renderSubmit, ui, present, ...), and writes them to `steadyState`, `steadyState.gpu` and `steadyState.cpu` in `logs/last_session_telemetry.json`. Always benchmark a **Release** build (`-DCMAKE_BUILD_TYPE=Release`) when comparing versions. |
| `--survival` | `.\build\sage.exe --survival` | Loads `scenes/survival_island.json` with only the `survival_game` resource. |
| *(terrain demo)* | `.\build\sage.exe --scene scenes/terrain_demo.json --resource terrain_demo` | Orbiting preview of the data-driven terrain demo (also try `scenes/heightmap_demo.json`). |
| `--resource <name>` | `.\build\sage.exe --resource stunt_sandbox` | Loads only this Lua resource (even if its manifest is disabled). |
| `--no-vsync` | `.\build\sage.exe --no-vsync` | Disables VSync for interactive runs. |
| `--resolution WxH` | `.\build\sage.exe --benchmark --resolution 1920x1080` | Sets the window size. Benchmark at two resolutions to tell pixel-bound passes (cost scales with pixel count) from geometry-bound ones. |
| `--no-light-tiles` | `.\build\sage.exe --benchmark --no-light-tiles` | Turns off clustered point-light culling (every pixel loops over every light). The image is identical either way; this is for A/B performance tests. |
| `--no-interp` | `.\build\sage.exe --no-interp` | Turns render interpolation off, drawing raw 60 Hz tick positions (for A/B comparison on high-refresh displays). |
| `--screenshot <file.png>` | `.\build\sage.exe --smoke-test --screenshot shot.png` | With `--smoke-test`/`--benchmark`, saves a PNG of the final 3D frame (no UI overlay). |
| `--cam px py pz tx ty tz` | `.\build\sage.exe --smoke-test --cam 0 40 -80 0 0 0` | Overrides the camera position and look-at target (useful for repeatable screenshots). |

---

## 4. Built-In Controls (Showcase & Editor)

### Engine & Editor Hotkeys (Available in Every Game)
| Key | Action |
| :--- | :--- |
| **`F1`** | Toggle the **Dear ImGui Live Inspector** (FPS graph, Time of Day, Shadows, Jolt Gravity, Audio, Entity Editor) |
| **`F5`** | Manually hot-reload active JSON scene + all enabled Lua resources (saving files also hot-reloads automatically!) |
| **`Tab`** | Toggle **Free-Fly 3D Inspector Camera** (Right-Mouse drag to look, `WASD` + `Q`/`E` to fly, Mouse Wheel for speed) |
| **`Esc`** | Quit cleanly |

### Built-In Showcase Game (`resources/stunt_sandbox/`)
| Key | Action |
| :--- | :--- |
| **`W` / `S`** or **Up / Down** | Accelerate / Brake & Reverse (6-gear automatic transmission) |
| **`A` / `D`** or **Left / Right** | Speed-sensitive steering |
| **`Space`** | Handbrake Drift |
| **`Left Shift`** | Nitro Boost (V12 roar + wider 3D FOV) |
| **`Right-Mouse Drag`** | Orbit 3D chase camera around the car |
| **`C`** | Cycle 3 camera angles (*Far Chase → Close Action → Hood Cam*) |
| **`X`** | Physics Shockwave (blasts nearby Jolt crates, balls & cones sky-high!) |
| **`B`** | Drop a fresh 6-crate Jolt Physics pyramid ahead of your car |
| **`T`** | Cycle Time of Day (*Golden Sunset → Bright Noon → Neon Night → Early Sunrise*) |
| **`R`** | Reset vehicle to city center |

---

## 5. Next Steps

* Ready to build **your own 3D game without touching C++**? Read [**Making Your First Game**](making_a_game.md).
* Looking for exact Lua function signatures? Read the [**Lua API Reference**](lua_api.md).
* Want to contribute to the C++20 core? Read [**Engine Architecture**](architecture.md) and [`CONTRIBUTING.md`](../CONTRIBUTING.md).
