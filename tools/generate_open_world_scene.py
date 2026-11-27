"""
Generates scenes/stunt_city.json — SAGE Engine v0.7 Multi-Biome 320m x 320m Open World!
Features:
- Zero Z-fighting layered road network (invisible ground collider, biome terrain at y=0.00,
  NS roads at y=0.04, EW roads at y=0.07, road dashes in Lua at y=0.06/0.09).
- Animated 3D Ocean Water ("type": "water") in the SE Palm Coast Lagoon (with carved seabed
  and wooden pier + glowing lanterns) AND endless surrounding 3D ocean outside the seawalls!
- Glowing Neon Downtown Portal Arches that showcase the v0.7 HDR Bloom post-processing!
- Declarative "collider" blocks on every building, tree, turbine, lamp, rock, arch pillar,
  lighthouse, ramp, and perimeter wall.
"""

import json
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "scenes", "stunt_city.json"))

entities = []


def add_plane(eid, x, y, z, sx, sz, color, emissive=False):
    e = {
        "id": eid,
        "type": "primitive_plane",
        "position": [x, y, z],
        "scale": [sx, 1.0, sz],
        "color": list(color),
    }
    if emissive:
        e["emissive"] = True
    entities.append(e)


def add_water(eid, x, y, z, sx, sz, shallow_color):
    entities.append({
        "id": eid,
        "type": "water",
        "position": [x, y, z],
        "scale": [sx, 1.0, sz],
        "color": list(shallow_color),
    })


def add_box(eid, x, y, z, sx, sy, sz, color, collider=None, emissive=False, yaw=0.0, visible=True):
    e = {
        "id": eid,
        "type": "primitive_box",
        "position": [x, y, z],
        "rotation": [0.0, yaw, 0.0],
        "scale": [sx, sy, sz],
        "color": list(color),
    }
    if not visible:
        e["visible"] = False
    if emissive:
        e["emissive"] = True
    if collider is not None:
        e["collider"] = collider
    entities.append(e)


def add_model(eid, model, x, y, z, yaw=0.0, scale=(1.0, 1.0, 1.0), color=None, collider=None):
    e = {
        "id": eid,
        "type": "model",
        "model": model,
        "position": [x, y, z],
        "rotation": [0.0, yaw, 0.0],
        "scale": list(scale),
    }
    if color is not None:
        e["color"] = list(color)
    if collider is not None:
        e["collider"] = collider
    entities.append(e)


def add_neon_arch(prefix, x, z, along_ns, glow_color):
    """Wide 16.5m clearance highway portal arch with HDR Bloom neon strips + solid pillar colliders."""
    dark_steel = [0.14, 0.16, 0.20]
    span = 16.8
    half_span = span * 0.5
    height = 7.4

    if along_ns:
        # Spans across X over an NS highway
        add_box(f"{prefix}_pil_l", x - half_span, height * 0.5, z, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_pil_r", x + half_span, height * 0.5, z, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_beam", x, height, z, span + 1.4, 0.75, 1.0, dark_steel)
        add_box(f"{prefix}_neon", x, height - 0.36, z, span - 0.4, 0.22, 0.35, glow_color, emissive=True)
        add_box(f"{prefix}_neon_l", x - half_span + 0.50, height * 0.55, z, 0.16, height * 0.72, 0.28, glow_color, emissive=True)
        add_box(f"{prefix}_neon_r", x + half_span - 0.50, height * 0.55, z, 0.16, height * 0.72, 0.28, glow_color, emissive=True)
    else:
        # Spans across Z over an EW highway
        add_box(f"{prefix}_pil_l", x, height * 0.5, z - half_span, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_pil_r", x, height * 0.5, z + half_span, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_beam", x, height, z, 1.0, 0.75, span + 1.4, dark_steel)
        add_box(f"{prefix}_neon", x, height - 0.36, z, 0.35, 0.22, span - 0.4, glow_color, emissive=True)
        add_box(f"{prefix}_neon_l", x, height * 0.55, z - half_span + 0.50, 0.28, height * 0.72, 0.16, glow_color, emissive=True)
        add_box(f"{prefix}_neon_r", x, height * 0.55, z + half_span - 0.50, 0.28, height * 0.72, 0.16, glow_color, emissive=True)


def build_world():
    # =========================================================================
    # 1. FOUR DISTINCT BIOME GROUND ZONES (y = 0.00, ZERO OVERLAP!)
    # =========================================================================
    # Invisible solid Jolt Physics ground slab (top face at y = 0.00; never rendered!)
    add_box("world_ground_collider", 0.0, -1.0, 0.0, 420.0, 2.0, 420.0, [0.12, 0.14, 0.16],
            collider={"type": "box", "halfExtents": [210.0, 1.0, 210.0]}, visible=False)

    # NW (-X, +Z): Downtown Metro Urban Concrete (x: -158..0, z: 0..158)
    add_plane("biome_downtown",  -79.0, 0.00,   79.0, 158.0, 158.0, [0.18, 0.19, 0.23])
    # NE (+X, +Z): Pine Highlands Lush Forest Floor (x: 0..158, z: 0..158)
    add_plane("biome_highlands",  79.0, 0.00,   79.0, 158.0, 158.0, [0.15, 0.27, 0.16])
    # SW (-X, -Z): Red Rock Desert Terracotta Floor (x: -158..0, z: -158..0)
    add_plane("biome_desert",    -79.0, 0.00,  -79.0, 158.0, 158.0, [0.64, 0.37, 0.21])

    # SE (+X, -Z): Palm Coast Beach — 4 clean non-overlapping sand slabs framing the carved Lagoon Cove (x: 66..110, z: -110..-66)!
    sand_col = [0.78, 0.68, 0.45]
    add_plane("biome_beach_w",    33.0, 0.00,  -79.0,  66.0, 158.0, sand_col)
    add_plane("biome_beach_e",   134.0, 0.00,  -79.0,  48.0, 158.0, sand_col)
    add_plane("biome_beach_n",    88.0, 0.00,  -33.0,  44.0,  66.0, sand_col)
    add_plane("biome_beach_s",    88.0, 0.00, -134.0,  44.0,  48.0, sand_col)

    # Sunken sandy seabed beneath the SE Lagoon Cove (y = -1.15 so animated 3D waves NEVER clip sand!)
    add_plane("lagoon_seabed",    88.0, -1.15, -88.0,  46.0,  46.0, [0.32, 0.35, 0.28])

    # =========================================================================
    # 1B. ANIMATED 3D OCEAN WATER (Turquoise Lagoon + Endless Surrounding Ocean!)
    # =========================================================================
    # Sparkling Turquoise Palm Coast Lagoon right inside the SE Beach Cove!
    add_water("palm_lagoon_water", 88.0, -0.32, -88.0, 44.0, 44.0, [0.06, 0.58, 0.70])

    # Endless animated 3D Open Ocean surrounding all 4 sides of the 320m island (zero overlap with land!)
    ocean_col = [0.04, 0.44, 0.62]
    add_water("ocean_east",   278.0, -0.40,    0.0, 240.0, 796.0, ocean_col)
    add_water("ocean_west",  -278.0, -0.40,    0.0, 240.0, 796.0, ocean_col)
    add_water("ocean_north",    0.0, -0.40,  278.0, 316.0, 240.0, ocean_col)
    add_water("ocean_south",    0.0, -0.40, -278.0, 316.0, 240.0, ocean_col)

    # =========================================================================
    # 2. LAYERED FLICKER-FREE HIGHWAY & CITY ROAD NETWORK
    #    NS roads at y = 0.04 | EW roads at y = 0.07 (Zero coplanar Z-fighting!)
    # =========================================================================
    asphalt_ns = [0.105, 0.115, 0.138]
    asphalt_ew = [0.108, 0.118, 0.140]

    # North-South Expressways & Loops (y = 0.04)
    add_plane("hwy_ns",      0.0, 0.04, 0.0, 14.0, 310.0, asphalt_ns)
    add_plane("loop_in_e",  54.0, 0.04, 0.0, 11.0, 120.0, asphalt_ns)
    add_plane("loop_in_w", -54.0, 0.04, 0.0, 11.0, 120.0, asphalt_ns)
    add_plane("ring_out_e",120.0, 0.04, 0.0, 12.0, 254.0, asphalt_ns)
    add_plane("ring_out_w",-120.0,0.04, 0.0, 12.0, 254.0, asphalt_ns)

    # East-West Expressways & Loops (y = 0.07 — cleanly 3 cm above NS roads!)
    add_plane("hwy_ew",      0.0, 0.07,   0.0, 310.0, 14.0, asphalt_ew)
    add_plane("loop_in_n",   0.0, 0.07,  54.0, 120.0, 11.0, asphalt_ew)
    add_plane("loop_in_s",   0.0, 0.07, -54.0, 120.0, 11.0, asphalt_ew)
    add_plane("ring_out_n",  0.0, 0.07, 120.0, 254.0, 12.0, asphalt_ew)
    add_plane("ring_out_s",  0.0, 0.07,-120.0, 254.0, 12.0, asphalt_ew)

    # =========================================================================
    # 3. PERIMETER COASTAL PROMENADE SEAWALLS (Low Scenic View + Solid Colliders!)
    # =========================================================================
    wall_col = [0.34, 0.37, 0.42]
    add_box("wall_north",   0.0, 0.85,  157.0, 316.0, 1.7, 2.0, wall_col,
            collider={"type": "box", "halfExtents": [158.0, 3.0, 1.2]})
    add_box("wall_south",   0.0, 0.70, -157.0, 316.0, 1.4, 2.0, [0.58, 0.54, 0.46],
            collider={"type": "box", "halfExtents": [158.0, 3.0, 1.2]})
    add_box("wall_east",  157.0, 0.70,    0.0, 2.0, 1.4, 316.0, [0.58, 0.54, 0.46],
            collider={"type": "box", "halfExtents": [1.2, 3.0, 158.0]})
    add_box("wall_west", -157.0, 0.85,    0.0, 2.0, 1.7, 316.0, wall_col,
            collider={"type": "box", "halfExtents": [1.2, 3.0, 158.0]})

    # =========================================================================
    # 4. BIOME 1 — DOWNTOWN METRO (Central Hub + North-West + Neon Arches!)
    # =========================================================================
    # 4 Glowing Neon Highway Portal Arches framing the Downtown Central Hub (HDR Bloom!)
    add_neon_arch("arch_n",   0.0,  16.0, True,  [0.12, 0.88, 1.00])
    add_neon_arch("arch_s",   0.0, -16.0, True,  [1.00, 0.25, 0.68])
    add_neon_arch("arch_e",  16.0,   0.0, False, [0.18, 1.00, 0.58])
    add_neon_arch("arch_w", -16.0,   0.0, False, [1.00, 0.72, 0.14])

    towers = [
        # Central 4 plaza skyscrapers framing the starting intersection
        ("tower_nw_hub", -24.0,  24.0,   0.0, (1.0, 1.25, 1.0), [0.95, 1.00, 1.08]),
        ("tower_ne_hub",  24.0,  24.0,  90.0, (1.0, 1.05, 1.0), [0.92, 1.04, 0.98]),
        ("tower_sw_hub", -24.0, -24.0, 180.0, (1.0, 0.95, 1.0), [1.06, 0.94, 0.88]),
        ("tower_se_hub",  24.0, -24.0, -90.0, (1.0, 1.15, 1.0), [1.05, 0.90, 0.98]),
        # Deep Downtown NW skyline district
        ("tower_dt_1",   -84.0,  28.0,   0.0, (1.1, 1.50, 1.1), [0.90, 0.96, 1.10]),
        ("tower_dt_2",   -28.0,  84.0,  90.0, (1.1, 1.40, 1.1), [1.08, 0.90, 1.02]),
        ("tower_dt_3",   -86.0,  86.0, 180.0, (1.2, 1.75, 1.2), [0.88, 1.02, 1.12]),
        ("tower_dt_4",   -64.0,  98.0, -90.0, (0.95, 1.18, 0.95), [1.04, 0.96, 0.90]),
        ("tower_dt_5",   -98.0,  62.0,  45.0, (0.95, 1.22, 0.95), [0.96, 0.92, 1.08]),
    ]
    for tid, tx, tz, yaw, scl, tint in towers:
        add_box(tid + "_pad", tx, 0.14, tz, 13.8 * scl[0], 0.28, 13.8 * scl[2], [0.35, 0.37, 0.42])
        add_model(
            tid, "assets/models/city_tower.glb", tx, 0.28, tz,
            yaw=yaw, scale=scl, color=tint,
            collider={"type": "box", "halfExtents": [5.9 * scl[0], 12.0 * scl[1], 5.9 * scl[2]], "offsetY": 12.0 * scl[1]},
        )

    # Industrial Warehouses / Delivery Depots (Downtown, Desert & Highlands)
    warehouses = [
        ("warehouse_dt_w",   -86.0,  60.0,   0.0),
        ("warehouse_dt_n",   -32.0, 138.0, 180.0),
        ("warehouse_desert", -86.0, -86.0,  90.0),
        ("warehouse_alpine",  86.0,  86.0, -90.0),
    ]
    for wid, wx, wz, yaw in warehouses:
        add_model(
            wid, "assets/models/warehouse.glb", wx, 0.0, wz,
            yaw=yaw, scale=(1.0, 1.0, 1.0),
            collider={"type": "box", "halfExtents": [7.2, 3.6, 5.2], "offsetY": 3.6},
        )

    # Street Lamps lining main avenues (EVERY pole has a solid cylinder collider!)
    lamp_spots = [
        ( 9.5,  36.0, -90.0), (-9.5,  36.0,  90.0),
        ( 9.5, -36.0, -90.0), (-9.5, -36.0,  90.0),
        ( 9.5,  72.0, -90.0), (-9.5,  72.0,  90.0),
        ( 9.5, -72.0, -90.0), (-9.5, -72.0,  90.0),
        ( 36.0,  9.5, 180.0), ( 36.0, -9.5,   0.0),
        (-36.0,  9.5, 180.0), (-36.0, -9.5,   0.0),
        (-74.0,  9.5, 180.0), (-74.0, -9.5,   0.0),
        ( 74.0,  9.5, 180.0), ( 74.0, -9.5,   0.0),
    ]
    for idx, (lx, lz, yaw) in enumerate(lamp_spots, 1):
        add_model(
            f"lamp_{idx}", "assets/models/street_lamp.glb", lx, 0.0, lz,
            yaw=yaw,
            collider={"type": "cylinder", "radius": 0.35, "height": 6.5},
        )

    # =========================================================================
    # 5. BIOME 2 — PINE HIGHLANDS & WIND RIDGE (North-East: +X, +Z)
    # =========================================================================
    pine_spots = [
        (36.0, 68.0, 1.05), (46.0, 82.0, 1.20), (30.0, 96.0, 0.95),
        (68.0, 32.0, 1.10), (82.0, 42.0, 1.25), (96.0, 26.0, 1.00),
        (72.0, 68.0, 1.15), (98.0, 66.0, 1.30), (66.0, 102.0, 1.05),
        (104.0, 98.0, 1.22), (138.0, 34.0, 1.18), (140.0, 74.0, 1.30),
        (136.0, 104.0, 1.12), (42.0, 138.0, 1.25), (84.0, 140.0, 1.15),
        (110.0, 138.0, 1.05), (140.0, 140.0, 1.35), (34.0, 40.0, 0.95),
    ]
    for idx, (px, pz, s) in enumerate(pine_spots, 1):
        add_model(
            f"pine_{idx}", "assets/models/pine_tree.glb", px, 0.0, pz,
            yaw=(idx * 37.0) % 360.0, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 0.65 * s, "height": 9.0 * s},
        )

    # Animated Spinning Wind Turbines on Pine Wind Ridge & Desert Wind Farm
    turbines = [
        ("wind_turbine_ne_1",  94.0,  104.0, -135.0),
        ("wind_turbine_ne_2", 138.0,   52.0, -110.0),
        ("wind_turbine_ne_3",  66.0,  140.0, -150.0),
        ("wind_turbine_sw_1", -96.0, -104.0,   45.0),
        ("wind_turbine_sw_2",-140.0,  -62.0,   70.0),
        ("wind_turbine_sw_3", -66.0, -140.0,   30.0),
    ]
    for tid, tx, tz, yaw in turbines:
        add_model(
            tid, "assets/models/wind_turbine.glb", tx, 0.0, tz,
            yaw=yaw, scale=(1.2, 1.2, 1.2),
            collider={"type": "cylinder", "radius": 0.95, "height": 18.0},
        )

    # =========================================================================
    # 6. BIOME 3 — PALM COAST BEACH, TURQUOISE LAGOON, PIER & LIGHTHOUSE (SE)
    # =========================================================================
    # Raised Stone Lighthouse Point overlooking the animated 3D Lagoon!
    add_box("lighthouse_quay", 102.0, 0.25, -102.0, 11.0, 1.5, 11.0, [0.46, 0.44, 0.40],
            collider={"type": "box", "halfExtents": [5.5, 1.5, 5.5]})
    add_model(
        "coastal_lighthouse", "assets/models/lighthouse.glb", 102.0, 1.0, -102.0,
        yaw=-45.0, scale=(1.15, 1.15, 1.15),
        collider={"type": "cylinder", "radius": 3.5, "height": 22.0},
    )

    # Wooden Beach Pier extending over the animated 3D Lagoon waves + glowing HDR lanterns!
    wood_col = [0.42, 0.27, 0.15]
    post_col = [0.28, 0.18, 0.10]
    add_box("beach_pier_deck", 76.0, 0.42, -88.0, 22.0, 0.24, 4.6, wood_col)
    for idx, px in enumerate([68.0, 74.0, 80.0, 86.0], 1):
        for side_name, pz in [("n", -85.8), ("s", -90.2)]:
            add_box(f"pier_post_{idx}_{side_name}", px, 0.20, pz, 0.34, 2.2, 0.34, post_col,
                    collider={"type": "cylinder", "radius": 0.22, "height": 2.2})
            if idx in (2, 4):
                add_box(f"pier_lamp_{idx}_{side_name}", px, 1.45, pz, 0.38, 0.38, 0.38, [1.0, 0.80, 0.34], emissive=True)

    # Palm trees placed cleanly on dry beach sand (never standing inside the lagoon!)
    palm_spots = [
        (36.0, -40.0, 1.05), (42.0, -72.0, 1.15), (28.0, -96.0, 1.00),
        (68.0, -28.0, 1.10), (88.0, -34.0, 1.22), (72.0, -58.0, 1.05),
        (100.0, -58.0, 1.18), (60.0, -98.0, 1.12), (46.0, -138.0, 1.20),
        (82.0, -138.0, 1.10), (138.0, -30.0, 1.25), (138.0, -72.0, 1.15),
        (22.0, -70.0, 0.98), (74.0, -42.0, 1.08), (38.0, -104.0, 1.14),
    ]
    for idx, (px, pz, s) in enumerate(palm_spots, 1):
        add_model(
            f"palm_{idx}", "assets/models/palm_tree.glb", px, 0.0, pz,
            yaw=(idx * 47.0) % 360.0, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 0.55 * s, "height": 8.0 * s},
        )

    # =========================================================================
    # 7. BIOME 4 — RED ROCK DESERT CANYON (South-West: -X, -Z)
    # =========================================================================
    rock_spots = [
        (-36.0,  -76.0, 1.15,  15.0),
        (-74.0,  -34.0, 1.30,  70.0),
        (-96.0,  -38.0, 1.50, 140.0),
        (-68.0,  -96.0, 1.25, 210.0),
        (-36.0, -102.0, 1.10, 300.0),
        (-138.0, -30.0, 1.60,  40.0),
        (-138.0, -94.0, 1.75, 110.0),
        (-136.0,-138.0, 1.85, 200.0),
        (-92.0, -140.0, 1.45, 250.0),
        (-34.0, -140.0, 1.35,  85.0),
    ]
    for idx, (rx, rz, s, yaw) in enumerate(rock_spots, 1):
        add_model(
            f"desert_rock_{idx}", "assets/models/desert_rock.glb", rx, 0.0, rz,
            yaw=yaw, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 4.2 * s, "height": 7.5 * s},
        )

    # =========================================================================
    # 8. EIGHT NEON 3D STUNT RAMPS ACROSS ALL 4 BIOMES (Auto Ramp Colliders!)
    # =========================================================================
    ramps = [
        ("ramp_hub_n",     0.0,   28.0,    0.0, 1.0),
        ("ramp_hub_s",     0.0,  -28.0,  180.0, 1.0),
        ("ramp_hub_e",    28.0,    0.0,   90.0, 1.0),
        ("ramp_hub_w",   -28.0,    0.0,  -90.0, 1.0),
        ("ramp_metro",   -54.0,   86.0,    0.0, 1.2),
        ("ramp_pine",     86.0,   54.0,   90.0, 1.2),
        ("ramp_beach",    54.0,  -86.0,  180.0, 1.2),
        ("ramp_desert",  -86.0,  -54.0,  -90.0, 1.25),
    ]
    for rid, rx, rz, yaw, s in ramps:
        add_model(
            rid, "assets/models/stunt_ramp.glb", rx, 0.05, rz,
            yaw=yaw, scale=(s, s, s),
            collider={"type": "ramp", "width": 5.0 * s, "height": 2.2 * s, "length": 7.5 * s},
        )

    scene = {
        "name": "SAGE v0.7 — HDR Open World (4 Biomes, 3D Animated Ocean Water, Sky Clouds & Bloom)",
        "version": 2,
        "sky": {
            "timeOfDay": 17.4,
            "ambientIntensity": 0.40,
            "fogStart": 140.0,
            "fogEnd": 460.0,
        },
        "camera": {
            "position": [10.0, 5.5, -14.0],
            "target": [0.0, 1.2, 0.0],
            "fov": 60.0,
        },
        "entities": entities,
    }

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        json.dump(scene, f, indent=2)

    col_count = sum(1 for e in entities if "collider" in e)
    water_count = sum(1 for e in entities if e["type"] == "water")
    print(f"Generated {OUT_PATH}: {len(entities)} entities, {col_count} colliders, {water_count} 3D animated water bodies!")


if __name__ == "__main__":
    build_world()
