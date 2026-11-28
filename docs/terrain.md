# Data-Driven Terrain (`scenes/*.json` → `"terrain"`)

Shape a whole landscape with **no C++**: hills, an island coastline, mountains, lakes, building plateaus, road beds, surface colours, snow, grass and collision are all set in the scene's `"terrain"` block. Save the file while the engine runs and the terrain, grass and physics collider rebuild live.

**Try it:**

```powershell
.\build\sage.exe --scene scenes/terrain_demo.json --resource terrain_demo
```

```powershell
.\build\sage.exe --scene scenes/heightmap_demo.json --resource terrain_demo
```

`terrain_demo.json` is a fully procedural island; `heightmap_demo.json` is a river canyon from a PNG. Press `Tab` for the free-fly camera and `F1` for the editor.

---

## How the height is built

For every point `(x, z)`, in this order:

1. `baseHeight`
2. \+ the **heightmap** image (if any), stretched over the whole terrain
3. \+ **noise** (natural hills)
4. → the **island** mask (land inside the coastline, sloping down to `seabedHeight` outside)
5. → each **feature** in the order listed (peaks, lakes, plateaus, roads)

The CPU (`world.sampleTerrain`, auto-grounding of models, grass placement), the GPU mesh and the Jolt collider all use this same function, so they always agree.

**Coordinates:** the terrain covers `-halfSize .. +halfSize` on X and Z, centred on the origin. +Y is up, heights are in metres. Colours are **linear RGB 0..1**, the same as entity `"color"` values.

---

## Full reference

```jsonc
"terrain": {
  "type": "procedural",          // "procedural" | "heightmap" | "preset" (see Presets)
  "enabled": true,
  "halfSize": 320.0,             // Terrain spans -320..+320 m (10..4000)
  "maxHeight": 60.0,             // Informational; used by the editor/log
  "gridResolution": 220,         // Render mesh cells per side (32..240)
  "baseHeight": 1.0,

  "heightmap": {                 // Required when type is "heightmap", optional otherwise
    "path": "assets/terrain/my_map.png",  // 8- or 16-bit grayscale PNG, north (+Z) at the top
    "minHeight": -8.0,           // Height of black pixels
    "maxHeight": 70.0            // Height of white pixels
  },

  "noise": {                     // Fractal hills
    "amplitude": 14.0,           // Metres of height variation
    "frequency": 0.008,          // 0.01 = hills ~100 m apart; bigger = busier
    "octaves": 5,                // Detail layers (1..8)
    "ridged": 0.35,              // 0 = rolling, 1 = sharp ridgelines
    "warp": 60.0,                // Metres of domain warp (organic, non-grid shapes)
    "seed": 3.0                  // Change for a different landscape
  },

  "island": {                    // Omit for land that reaches the edges
    "center": [0.0, 0.0],
    "radius": 225.0,             // Coastline distance from centre
    "falloff": 70.0,             // Beach/shelf width
    "coastNoise": 34.0,          // Metres of bays & peninsulas
    "seabedHeight": -6.5
  },

  "features": [
    { "type": "peak",    "center": [-40, 95], "radius": 150, "height": 46, "sharpness": 2.2 },
    { "type": "basin",   "center": [-70, -55], "innerRadius": 20, "radius": 58, "depth": 4, "strength": 0.9 },
    { "type": "flatten", "center": [60, 60], "innerRadius": 24, "radius": 62, "height": 9 },
    { "type": "road", "width": 8, "blend": 10, "height": 0,
      "points": [[178, -112, 1.4], [130, -60, 3.5], [60, 60, 9.0]] }
  ],

  "layers": [                    // Lowest first; up to 6 are rendered
    { "name": "sand",   "color": [0.50, 0.42, 0.27], "roughness": 0.80 },
    { "name": "meadow", "color": [0.09, 0.20, 0.06], "startHeight": 2.4, "blend": 2.0 },
    { "name": "forest", "color": [0.06, 0.14, 0.05], "startHeight": 10.0, "blend": 5.0 }
  ],
  "cliff": { "color": [0.21, 0.20, 0.19], "slopeStart": 0.15, "slopeEnd": 0.34 },
  "snow":  { "startHeight": 40.0, "endHeight": 48.0, "color": [0.84, 0.88, 0.94] },

  "grass": {
    "enabled": true,
    "count": 40000,              // Target tufts (0..60000)
    "area": 0.8,                 // Fraction of halfSize to cover
    "minHeight": 2.0, "maxHeight": 26.0,
    "minNormalY": 0.78,          // 1.0 = flat ground only; lower allows steeper slopes
    "avoidFeatures": true        // No grass on plateaus and roads
  },

  "collisionMinHeight": -4.5,    // No physics collision below this (props sink into lakes/sea)
  "hideBelowHeight": -1e9        // Skip drawing terrain entirely below this (see the note below)
}
```

### Features

| `type` | What it does | Keys |
| :--- | :--- | :--- |
| `peak` | Adds a smooth mountain or hill | `center`, `radius`, `height`, `sharpness` (1 = cone, 2+ = rounder) |
| `basin` (alias `lake`, `crater`) | Flattens toward the centre, then carves down: lakes, craters, quarries | `center`, `innerRadius`, `radius`, `depth`, `strength` (0..1 flattening) |
| `flatten` (alias `plateau`) | Levels a round area to an exact height: towns, bases, arenas | `center`, `innerRadius` (fully flat), `radius` (blend edge), `height` |
| `road` | Levels a strip along a path: roads, runways, rail beds | `points` (`[x, z]` or `[x, z, height]`), `width`, `blend`, `height` (default for 2-value points) |

Features apply **in order**, so a later `flatten` can level the top of an earlier `peak`. Add a `"name"` to any feature as a note to yourself; the engine ignores it.

### Water
Terrain doesn't create water. Add a `"type": "water"` entity (usually at `y = 0`, scaled to cover the map) and anything below its surface becomes sea, lake or river. Water depth, buoyancy and the shoreline foam use the real terrain height underneath.

### Models on terrain
Any `"type": "model"` entity with `position.y` of `0` is automatically placed on the terrain surface, sampling its footprint so it sits firmly on slopes.

### Lua
* `world.sampleTerrain(x, z)` returns `height, nx, ny, nz, layerIndex`. `layerIndex` is the index of the highest `layers` entry this height has reached (0 = lowest).
* `world.setTerrainEnabled(bool)` turns terrain on or off.

### Presets (legacy showcase maps)
`"type": "preset"` with `"preset": "stunt_city"` or `"echo_island"` loads the two hand-coded showcase landscapes (the old `"mode": "city"` / `"island"` still works). They're kept so the shipped games look exactly as before. **New games should use `procedural` or `heightmap`.**

---

## Errors are explicit

Mistakes are reported with the scene file and the exact JSON path, and an invalid terrain block falls back to flat ground instead of crashing:

```text
[SAGE][Scene][Terrain] ERROR scenes/x.json -> terrain.features[0].type: unknown feature "volcano" (use peak, basin, flatten or road)
[SAGE][Scene][Terrain] WARNING scenes/x.json -> terrain.noise.octavs: unknown key (ignored). Check the spelling against docs/terrain.md
[SAGE][Scene][Terrain] WARNING scenes/x.json -> terrain.layers[0].color: color components should be 0..1 (did you use 0..255?). Values were clamped
```

## Tips & gotchas
* **Heightmaps:** use 16-bit PNGs for smooth slopes (8-bit shows terracing). `tools/generate_demo_heightmap.py` shows how to make one with Python. The physics collider samples the terrain on a 129×129 grid, so very fine heightmap detail is visual only.
* **`hideBelowHeight`** saves triangles under deep water, but the water shader shades by depth, so hiding seabed under *shallow* water shows a stair-step edge. Keep it at least 1–2 m below your shallowest visible seabed, or leave it out.
* **Roads through hills** cut steep walls, which get the cliff colour. Route roads around peaks, raise their point heights, or widen `blend`.
* **Hot reload** rebuilds the mesh, grass and collider in about 0.5 s, which is fine while editing.
