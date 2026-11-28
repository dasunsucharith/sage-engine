# Making Your First 3D Game in SAGE (Zero C++ Needed!)

SAGE is designed so you can create **any 3D game** — racing, platformer, physics toybox, action arena, or open-world sandbox — purely by editing **plain-text JSON scenes**, dropping in **Blender `.glb` models**, and writing **hot-reloadable Lua scripts**.

---

## Step 1: Understand the 3 Game Folders

```text
sage-engine/
├── assets/models/          # 1. Your 3D .glb/.gltf models (static or animated!)
├── scenes/                 # 2. Plain-text JSON worlds (sky, camera, static props)
└── resources/              # 3. FiveM-style Lua gameplay scripts (hot-reloadable!)
```

You never need to recompile C++ while making a game. Leave `sage.exe` open on one half of your screen and your editor on the other — **press `Ctrl+S` on any `.lua` or `.json` file and SAGE reloads live in under 0.5 seconds!**

---

## Step 2: Create Your Resource Folder

1. Disable the default showcase by setting `"enabled": false` in `resources/stunt_sandbox/manifest.json`.
2. Create a new folder `resources/my_game/` with two files:

### `resources/my_game/manifest.json`
```json
{
  "name": "my_game",
  "version": "1.0.0",
  "author": "Your Name",
  "enabled": true,
  "description": "My first 3D game built with SAGE Engine.",
  "scripts": ["main.lua"]
}
```

### `resources/my_game/main.lua`
```lua
local playerX, playerZ = 0.0, 0.0
local score = 0

function onStart()
    -- 1. Build a floor and attach a static Jolt Physics collider
    world.spawnPlane("floor", 0.0, 0.0, 0.0)
    world.setScale("floor", 80.0, 1.0, 80.0)
    world.setColor("floor", 0.16, 0.19, 0.22)
    physics.addStaticBox("floor", 0.0, -1.0, 0.0, 40.0, 1.0, 40.0)

    -- 2. Spawn an animated Blender .glb drone as our player
    world.spawnModel("player", "assets/models/hover_drone.glb", 0.0, 2.0, 0.0)
    world.playAnimation("player", "", true, 1.2)
    physics.addKinematicBox("player", 0.0, 2.0, 0.0, 1.0, 0.5, 1.0)

    -- 3. Spawn a rolling Jolt Physics Stunt Ball to push around!
    world.spawnModel("ball", "assets/models/stunt_ball.glb", 0.0, 4.0, 8.0)
    physics.addDynamicSphere("ball", 0.0, 4.0, 8.0, 1.25, 25.0, 0.75, 0.35)

    world.setTimeOfDay(17.5) -- Golden sunset
    sage.log("My first SAGE game started!")
end

function onTick(dt)
    local speed = 15.0
    if input.isKeyDown("W") then playerZ = playerZ + speed * dt end
    if input.isKeyDown("S") then playerZ = playerZ - speed * dt end
    if input.isKeyDown("A") then playerX = playerX + speed * dt end
    if input.isKeyDown("D") then playerX = playerX - speed * dt end

    world.setPosition("player", playerX, 2.0, playerZ)
    physics.moveKinematic("player", playerX, 2.0, playerZ, 0.0, 0.0, 0.0, dt)

    camera.setPosition(playerX, 9.0, playerZ - 14.0)
    camera.setTarget(playerX, 1.5, playerZ + 2.0)

    if physics.getSpeed("ball") > 4.0 then
        score = score + 1
    end
end

function onRenderUI()
    if ui.beginWindow("My Game HUD", 20, 20, 240, 110, true) then
        ui.text("Score: " .. score)
        if ui.button("Launch Ball!") then
            physics.applyImpulse("ball", 0.0, 260.0, 180.0)
            audio.playChime()
        end
    end
    ui.endWindow()
end

function onShutdown()
    sage.log("My game shutting down cleanly.")
end
```

3. Run SAGE with an empty or custom scene:
   ```powershell
   .\build\sage.exe --scene scenes/sandbox.json
   ```

---

## Step 3: Build Worlds in Plain-Text JSON (`scenes/*.json`)

Place static geometry, lighting, sky time-of-day, and `.glb` props in JSON so humans and AI agents can read and edit your levels cleanly:

```json
{
  "name": "My Arena",
  "version": 1,
  "sky": {
    "timeOfDay": 17.5,
    "ambientIntensity": 0.38
  },
  "camera": {
    "position": [0.0, 10.0, -20.0],
    "target": [0.0, 1.5, 0.0],
    "fov": 60.0
  },
  "entities": [
    {
      "id": "tower_1",
      "type": "model",
      "model": "assets/models/city_tower.glb",
      "position": [15.0, 0.0, 15.0],
      "rotation": [0.0, 45.0, 0.0],
      "scale": [1.0, 1.0, 1.0]
    }
  ]
}
```

Supported entity `"type"` values: `"model"`, `"water"`, `"primitive_box"`, `"primitive_plane"`, `"primitive_sphere"`, `"primitive_triangle"`, `"primitive_ring"`.

### Add a landscape (no C++)
Add a `"terrain"` block to shape hills, an island coastline, mountains, lakes, plateaus for towns, road beds, colours, snow and grass, either procedurally or from a heightmap PNG:

```json
"terrain": {
  "type": "procedural",
  "halfSize": 300,
  "noise": { "amplitude": 12, "frequency": 0.01, "warp": 40 },
  "island": { "radius": 210, "falloff": 60, "coastNoise": 30 },
  "features": [
    { "type": "peak", "center": [0, 80], "radius": 120, "height": 40 },
    { "type": "flatten", "center": [60, -40], "innerRadius": 20, "radius": 45, "height": 6 }
  ]
}
```

Models with `position.y = 0` are placed on the ground automatically. See [**Data-Driven Terrain**](terrain.md) for every option, and `scenes/terrain_demo.json` / `scenes/heightmap_demo.json` for complete examples.

---

## Step 4: Drop In Your Own 3D Models from Blender

SAGE natively loads **glTF 2.0 Binary (`.glb`)** files — including multi-material PBR colors, metallic/roughness, emissive glow, embedded PNG/JPEG textures, and **keyframe animations**.

### Blender Export Checklist
1. Place the bottom of your model at **`Z = 0`** and orient its front toward **`-Y`** in Blender (glTF `+Y Up` maps Blender `-Y` → SAGE `+Z` forward, and Blender `+Z` → SAGE `+Y` up).
2. Use **Principled BSDF** materials (avoid unbaked procedural nodes like ColorRamp/Noise on `Base Color` — use direct colors or baked image textures).
3. Go to **File → Export → glTF 2.0 (`.glb`/`.gltf`)**:
   * Format: **glTF Binary (`.glb`)**
   * Transform: **+Y Up** (checked)
   * Data → Mesh: **Apply Modifiers**, **Normals**, **UVs** (checked)
   * Animation *(if animated)*: **Animation** (checked)
4. Save into `assets/models/my_asset.glb` and spawn it in JSON or with `world.spawnModel("id", "assets/models/my_asset.glb", x, y, z)`.

---

## Step 5: Built-In Starter 3D Asset Library (`assets/models/`)

Every SAGE clone already ships with **15 ready-to-use Blender `.glb` assets**:

| Model Path | Description | Animated? | Suggested JSON `"collider"` |
| :--- | :--- | :---: | :--- |
| `assets/models/aventador_svj.glb` | Custom sculpted Lamborghini Aventador SVJ (128 submeshes, Verde Mantis PBR) | Static | `addKinematicBox(..., 1.05, 0.62, 2.25)` |
| `assets/models/sports_car.glb` | Stylized red sports coupe with 5-spoke rims & GT spoiler | Static | `addKinematicBox(..., 1.05, 0.62, 2.25)` |
| `assets/models/stunt_ramp.glb` | Neon-railed concrete 3D stunt jump ramp (`5m × 2.2m × 7.5m`) | Static | `{"type": "ramp", "width": 5, "height": 2.2, "length": 7.5}` |
| `assets/models/crate.glb` | Framed wooden shipping crate (`1.28m` cube) | Static | `addDynamicBox(..., 0.64, 0.64, 0.64)` |
| `assets/models/stunt_ball.glb` | Giant metallic neon-ringed stunt soccer ball (`r = 1.25m`) | Static | `addDynamicSphere(..., 1.25)` |
| `assets/models/traffic_cone.glb` | High-visibility reflective stunt cone | Static | `addDynamicCapsule(..., 0.24, 0.26)` |
| `assets/models/wind_turbine.glb` | 15m city wind turbine with 3 spinning blades | **Yes** | `{"type": "cylinder", "radius": 0.95, "height": 18}` |
| `assets/models/hover_drone.glb` | Sci-fi quad-rotor drone (bobbing chassis + 4 spinning rotors) | **Yes** | Kinematic / patrol |
| `assets/models/lighthouse.glb` | Coastal lighthouse with 360° rotating glowing searchlight beam | **Yes** | `{"type": "cylinder", "radius": 3.5, "height": 22}` |
| `assets/models/city_tower.glb` | Multi-tier glass & concrete skyscraper with neon crown | Static | `{"type": "box", "halfExtents": [5.9, 12, 5.9], "offsetY": 12}` |
| `assets/models/warehouse.glb` | Industrial steel delivery depot / garage hangar | Static | `{"type": "box", "halfExtents": [7.2, 3.6, 5.2], "offsetY": 3.6}` |
| `assets/models/palm_tree.glb` | Curved tropical palm tree (Palm Coast biome) | Static | `{"type": "cylinder", "radius": 0.55, "height": 8}` |
| `assets/models/pine_tree.glb` | Layered alpine evergreen conifer (Pine Highlands biome) | Static | `{"type": "cylinder", "radius": 0.65, "height": 9}` |
| `assets/models/desert_rock.glb` | Stratified red-rock canyon mesa (Red Rock Desert biome) | Static | `{"type": "cylinder", "radius": 4.2, "height": 7.5}` |
| `assets/models/street_lamp.glb` | 6.4m modern boulevard LED street lamp | Static | `{"type": "cylinder", "radius": 0.35, "height": 6.5}` |

---

## Step 6: Making Games with an AI Coding Assistant

Because SAGE uses plain-text JSON, modular Lua, and self-documenting Markdown (`AGENTS.md` + `docs/lua_api.md`), you can simply prompt your AI assistant:

> *"Create a new Lua resource in `resources/rocket_soccer/` that uses `aventador_svj.glb`, two goals made of glowing `world.spawnBox` posts, and `stunt_ball.glb` with `physics.addDynamicSphere`, plus a scoreboard HUD using `onRenderUI()`."*

Read the complete [**Lua API Reference**](lua_api.md) for every available function!
