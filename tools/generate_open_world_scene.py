"""
Generates scenes/stunt_city.json — SAGE Engine v0.6 Multi-Biome 320m x 320m Open World!
Every building, tree, wind turbine, street lamp, rock mesa, warehouse, lighthouse,
ramp, and perimeter wall carries a declarative "collider" block so Jolt Physics AND
vehicles automatically collide solidly with every world object.
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


def add_box(eid, x, y, z, sx, sy, sz, color, collider=None, emissive=False, yaw=0.0):
    e = {
        "id": eid,
        "type": "primitive_box",
        "position": [x, y, z],
        "rotation": [0.0, yaw, 0.0],
        "scale": [sx, sy, sz],
        "color": list(color),
    }
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


def build_world():
    # =========================================================================
    # 1. FOUR DISTINCT BIOME GROUND ZONES (320m x 320m Open World!)
    # =========================================================================
    # Distant outer horizon ground + buried Jolt ground floor slab
    add_box("world_ground_collider", 0.0, -1.0, 0.0, 420.0, 2.0, 420.0, [0.12, 0.14, 0.16],
            collider={"type": "box", "halfExtents": [210.0, 1.0, 210.0]})

    # NW (-X, +Z): Downtown Metro Urban Concrete
    add_plane("biome_downtown", -80.0, -0.02,  80.0, 160.0, 160.0, [0.17, 0.18, 0.21])
    # NE (+X, +Z): Pine Highlands Lush Forest Floor
    add_plane("biome_highlands", 80.0, -0.02,  80.0, 160.0, 160.0, [0.14, 0.25, 0.15])
    # SE (+X, -Z): Palm Coast Warm Beach Sand + Turquoise Ocean Bay!
    add_plane("biome_beach",     80.0, -0.02, -80.0, 160.0, 160.0, [0.76, 0.66, 0.44])
    add_plane("ocean_bay",      125.0,  0.01, -125.0,  86.0,  86.0, [0.07, 0.42, 0.56])
    add_plane("ocean_foam",     108.0,  0.015,-108.0,  56.0,  56.0, [0.14, 0.56, 0.68])
    # SW (-X, -Z): Red Rock Desert Terracotta Floor
    add_plane("biome_desert",   -80.0, -0.02, -80.0, 160.0, 160.0, [0.62, 0.36, 0.21])

    # =========================================================================
    # 2. SWEEPING HIGHWAY & CITY ROAD NETWORK
    # =========================================================================
    asphalt = [0.11, 0.12, 0.14]
    # Grand 4-lane cross expressways (310m span!)
    add_plane("hwy_ns", 0.0, 0.00, 0.0, 14.0, 310.0, asphalt)
    add_plane("hwy_ew", 0.0, 0.00, 0.0, 310.0, 14.0, asphalt)

    # Inner downtown & hub loop (±54m)
    add_plane("loop_in_n", 0.0, 0.00,  54.0, 120.0, 11.0, asphalt)
    add_plane("loop_in_s", 0.0, 0.00, -54.0, 120.0, 11.0, asphalt)
    add_plane("loop_in_e", 54.0, 0.00, 0.0, 11.0, 120.0, asphalt)
    add_plane("loop_in_w",-54.0, 0.00, 0.0, 11.0, 120.0, asphalt)

    # Outer 4-Biome Scenic Ring Highway (±120m — connects every biome!)
    add_plane("ring_out_n",  0.0, 0.00,  120.0, 254.0, 12.0, asphalt)
    add_plane("ring_out_s",  0.0, 0.00, -120.0, 254.0, 12.0, asphalt)
    add_plane("ring_out_e", 120.0, 0.00,   0.0, 12.0, 254.0, asphalt)
    add_plane("ring_out_w",-120.0, 0.00,   0.0, 12.0, 254.0, asphalt)

    # =========================================================================
    # 3. PERIMETER HIGHWAY BARRIER WALLS (Solid Colliders at ±156m!)
    # =========================================================================
    wall_col = [0.28, 0.31, 0.36]
    add_box("wall_north",   0.0, 1.4,  156.0, 316.0, 2.8, 2.0, wall_col,
            collider={"type": "box", "halfExtents": [158.0, 3.0, 1.2]})
    add_box("wall_south",   0.0, 1.4, -156.0, 316.0, 2.8, 2.0, wall_col,
            collider={"type": "box", "halfExtents": [158.0, 3.0, 1.2]})
    add_box("wall_east",  156.0, 1.4,    0.0, 2.0, 2.8, 316.0, wall_col,
            collider={"type": "box", "halfExtents": [1.2, 3.0, 158.0]})
    add_box("wall_west", -156.0, 1.4,    0.0, 2.0, 2.8, 316.0, wall_col,
            collider={"type": "box", "halfExtents": [1.2, 3.0, 158.0]})

    # =========================================================================
    # 4. BIOME 1 — DOWNTOWN METRO (Central Hub + North-West)
    # =========================================================================
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
        add_box(tid + "_pad", tx, 0.12, tz, 13.8 * scl[0], 0.24, 13.8 * scl[2], [0.35, 0.37, 0.42])
        add_model(
            tid, "assets/models/city_tower.glb", tx, 0.24, tz,
            yaw=yaw, scale=scl, color=tint,
            collider={"type": "box", "halfExtents": [5.9 * scl[0], 12.0 * scl[1], 5.9 * scl[2]], "offsetY": 12.0 * scl[1]},
        )

    # Industrial Warehouses / Delivery Depots (Downtown & Desert)
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
        ( 9.5,  16.0, -90.0), (-9.5,  16.0,  90.0),
        ( 9.5, -16.0, -90.0), (-9.5, -16.0,  90.0),
        ( 9.5,  72.0, -90.0), (-9.5,  72.0,  90.0),
        ( 9.5, -72.0, -90.0), (-9.5, -72.0,  90.0),
        (16.0,   9.5, 180.0), (16.0,  -9.5,   0.0),
        (-16.0,  9.5, 180.0), (-16.0, -9.5,   0.0),
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

    # Animated Spinning Wind Turbines on Pine Wind Ridge & Desert Wind Farm (ALL with solid mast colliders!)
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
    # 6. BIOME 3 — PALM COAST BEACH & LIGHTHOUSE (South-East: +X, -Z)
    # =========================================================================
    # Animated Rotating Coastal Lighthouse!
    add_model(
        "coastal_lighthouse", "assets/models/lighthouse.glb", 94.0, 0.0, -94.0,
        yaw=-45.0, scale=(1.15, 1.15, 1.15),
        collider={"type": "cylinder", "radius": 3.5, "height": 22.0},
    )

    palm_spots = [
        (36.0, -40.0, 1.05), (42.0, -72.0, 1.15), (28.0, -96.0, 1.00),
        (68.0, -28.0, 1.10), (88.0, -34.0, 1.22), (72.0, -68.0, 1.05),
        (100.0, -68.0, 1.18), (66.0, -98.0, 1.12), (46.0, -138.0, 1.20),
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
            rid, "assets/models/stunt_ramp.glb", rx, 0.0, rz,
            yaw=yaw, scale=(s, s, s),
            collider={"type": "ramp", "width": 5.0 * s, "height": 2.2 * s, "length": 7.5 * s},
        )

    scene = {
        "name": "SAGE v0.6 — 4-Biome Open World (Metro, Highlands, Palm Coast, Red Rock Desert)",
        "version": 2,
        "sky": {
            "timeOfDay": 17.4,
            "ambientIntensity": 0.38,
            "fogStart": 125.0,
            "fogEnd": 420.0,
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
    print(f"Generated {OUT_PATH}: {len(entities)} entities, {col_count} automatic solid colliders!")


if __name__ == "__main__":
    build_world()
