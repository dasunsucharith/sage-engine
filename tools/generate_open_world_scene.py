"""
Generates scenes/stunt_city.json — SAGE Engine v0.8 HDR Dynamic Open World!
Features:
- Carved Jolt Physics dry-land ground slabs + sunken lagoon seabed (-1.15m) so dynamic
  crates & stunt balls genuinely splash into the lagoon and float with real buoyancy!
- Drivable Wooden Beach Pier ("type": "deck") with wooden entry ramp over the waves —
  drive onto the pier or launch off into the lagoon and splash/sink realistically!
- 18 total Blender .glb models including 3 new v0.8 assets:
  * sailboat.glb (animated wave-rocking coastal sailboats!)
  * gas_station.glb (neon roadside service stations with dynamic canopy lights!)
  * lifeguard_tower.glb (Palm Coast wooden lifeguard towers!)
- Declarative JSON dynamic point lights ("lights": [...]) illuminating roads & water!
"""

import json
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "scenes", "stunt_city.json"))

entities = []
lights = []


def add_light(lid, x, y, z, color, radius=20.0, intensity=2.2):
    lights.append({
        "id": lid,
        "position": [x, y, z],
        "color": list(color),
        "radius": radius,
        "intensity": intensity,
    })


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


def add_water(eid, x, y, z, sx, sz, shallow_color, seabed_y=-1.25):
    entities.append({
        "id": eid,
        "type": "water",
        "position": [x, y, z],
        "scale": [sx, 1.0, sz],
        "color": list(shallow_color),
        "seabedY": seabed_y,
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
    """Wide 16.8m clearance highway portal arch with HDR Bloom neon strips + dynamic point light."""
    dark_steel = [0.14, 0.16, 0.20]
    span = 16.8
    half_span = span * 0.5
    height = 7.4

    if along_ns:
        add_box(f"{prefix}_pil_l", x - half_span, height * 0.5, z, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_pil_r", x + half_span, height * 0.5, z, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_beam", x, height, z, span + 1.4, 0.75, 1.0, dark_steel)
        add_box(f"{prefix}_neon", x, height - 0.36, z, span - 0.4, 0.22, 0.35, glow_color, emissive=True)
        add_box(f"{prefix}_neon_l", x - half_span + 0.50, height * 0.55, z, 0.16, height * 0.72, 0.28, glow_color, emissive=True)
        add_box(f"{prefix}_neon_r", x + half_span - 0.50, height * 0.55, z, 0.16, height * 0.72, 0.28, glow_color, emissive=True)
    else:
        add_box(f"{prefix}_pil_l", x, height * 0.5, z - half_span, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_pil_r", x, height * 0.5, z + half_span, 0.9, height, 0.9, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_beam", x, height, z, 1.0, 0.75, span + 1.4, dark_steel)
        add_box(f"{prefix}_neon", x, height - 0.36, z, 0.35, 0.22, span - 0.4, glow_color, emissive=True)
        add_box(f"{prefix}_neon_l", x, height * 0.55, z - half_span + 0.50, 0.28, height * 0.72, 0.16, glow_color, emissive=True)
        add_box(f"{prefix}_neon_r", x, height * 0.55, z + half_span - 0.50, 0.28, height * 0.72, 0.16, glow_color, emissive=True)

    add_light(f"{prefix}_light", x, height - 1.2, z, glow_color, radius=24.0, intensity=2.4)


def build_world():
    # =========================================================================
    # 1. CARVED JOLT FLOOR COLLIDERS + FOUR NON-OVERLAPPING BIOME GROUNDS
    # =========================================================================
    # Invisible Jolt Physics dry-land floor slabs cleanly framing the lagoon cutout (x: 66..110, z: -110..-66)
    # so dynamic crates & stunt balls drop into the water and float with Archimedes buoyancy!
    add_box("ground_col_west",  -72.0, -1.0,    0.0, 276.0, 2.0, 420.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [138.0, 1.0, 210.0]}, visible=False)
    add_box("ground_col_east",  160.0, -1.0,    0.0, 100.0, 2.0, 420.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [50.0, 1.0, 210.0]}, visible=False)
    add_box("ground_col_north",  88.0, -1.0,   72.0,  44.0, 2.0, 276.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [22.0, 1.0, 138.0]}, visible=False)
    add_box("ground_col_south",  88.0, -1.0, -160.0,  44.0, 2.0, 100.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [22.0, 1.0, 50.0]}, visible=False)
    # Sunken seabed Jolt floor beneath the Palm Lagoon (top face at y = -1.25m!)
    add_box("lagoon_seabed_col", 88.0, -2.25, -88.0,  48.0, 2.0,  48.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [24.0, 1.0, 24.0]}, visible=False)

    # Visual 4-biome ground tiles (y = 0.00, zero overlap!)
    add_plane("biome_downtown",  -79.0, 0.00,   79.0, 158.0, 158.0, [0.18, 0.19, 0.23])
    add_plane("biome_highlands",  79.0, 0.00,   79.0, 158.0, 158.0, [0.15, 0.27, 0.16])
    add_plane("biome_desert",    -79.0, 0.00,  -79.0, 158.0, 158.0, [0.64, 0.37, 0.21])

    sand_col = [0.78, 0.68, 0.45]
    add_plane("biome_beach_w",    33.0, 0.00,  -79.0,  66.0, 158.0, sand_col)
    add_plane("biome_beach_e",   134.0, 0.00,  -79.0,  48.0, 158.0, sand_col)
    add_plane("biome_beach_n",    88.0, 0.00,  -33.0,  44.0,  66.0, sand_col)
    add_plane("biome_beach_s",    88.0, 0.00, -134.0,  44.0,  48.0, sand_col)
    add_plane("lagoon_seabed",    88.0, -1.25, -88.0,  46.0,  46.0, [0.30, 0.34, 0.27])

    # =========================================================================
    # 1B. ANIMATED 3D OCEAN WATER (Real Seabed Depth + Buoyancy Volumes!)
    # =========================================================================
    add_water("palm_lagoon_water", 88.0, -0.26, -88.0, 44.0, 44.0, [0.06, 0.58, 0.70], seabed_y=-1.25)

    ocean_col = [0.04, 0.44, 0.62]
    add_water("ocean_east",   278.0, -0.40,    0.0, 240.0, 796.0, ocean_col, seabed_y=-3.5)
    add_water("ocean_west",  -278.0, -0.40,    0.0, 240.0, 796.0, ocean_col, seabed_y=-3.5)
    add_water("ocean_north",    0.0, -0.40,  278.0, 316.0, 240.0, ocean_col, seabed_y=-3.5)
    add_water("ocean_south",    0.0, -0.40, -278.0, 316.0, 240.0, ocean_col, seabed_y=-3.5)

    # =========================================================================
    # 2. LAYERED FLICKER-FREE HIGHWAY & CITY ROAD NETWORK
    #    NS roads at y = 0.04 | EW roads at y = 0.07 (Zero coplanar Z-fighting!)
    # =========================================================================
    asphalt_ns = [0.105, 0.115, 0.138]
    asphalt_ew = [0.108, 0.118, 0.140]

    add_plane("hwy_ns",      0.0, 0.04, 0.0, 14.0, 310.0, asphalt_ns)
    add_plane("loop_in_e",  54.0, 0.04, 0.0, 11.0, 120.0, asphalt_ns)
    add_plane("loop_in_w", -54.0, 0.04, 0.0, 11.0, 120.0, asphalt_ns)
    add_plane("ring_out_e",120.0, 0.04, 0.0, 12.0, 254.0, asphalt_ns)
    add_plane("ring_out_w",-120.0,0.04, 0.0, 12.0, 254.0, asphalt_ns)

    add_plane("hwy_ew",      0.0, 0.07,   0.0, 310.0, 14.0, asphalt_ew)
    add_plane("loop_in_n",   0.0, 0.07,  54.0, 120.0, 11.0, asphalt_ew)
    add_plane("loop_in_s",   0.0, 0.07, -54.0, 120.0, 11.0, asphalt_ew)
    add_plane("ring_out_n",  0.0, 0.07, 120.0, 254.0, 12.0, asphalt_ew)
    add_plane("ring_out_s",  0.0, 0.07,-120.0, 254.0, 12.0, asphalt_ew)

    # =========================================================================
    # 3. PERIMETER COASTAL PROMENADE SEAWALLS
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
    # 4. BIOME 1 — DOWNTOWN METRO (Central Hub + Neon Arches + Gas Station!)
    # =========================================================================
    add_neon_arch("arch_n",   0.0,  16.0, True,  [0.12, 0.88, 1.00])
    add_neon_arch("arch_s",   0.0, -16.0, True,  [1.00, 0.25, 0.68])
    add_neon_arch("arch_e",  16.0,   0.0, False, [0.18, 1.00, 0.58])
    add_neon_arch("arch_w", -16.0,   0.0, False, [1.00, 0.72, 0.14])

    towers = [
        ("tower_nw_hub", -24.0,  24.0,   0.0, (1.0, 1.25, 1.0), [0.95, 1.00, 1.08]),
        ("tower_ne_hub",  24.0,  24.0,  90.0, (1.0, 1.05, 1.0), [0.92, 1.04, 0.98]),
        ("tower_sw_hub", -24.0, -24.0, 180.0, (1.0, 0.95, 1.0), [1.06, 0.94, 0.88]),
        ("tower_se_hub",  24.0, -24.0, -90.0, (1.0, 1.15, 1.0), [1.05, 0.90, 0.98]),
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

    # NEW v0.8 Neon Roadside Gas Stations (Downtown & Desert Highway!)
    gas_stations = [
        ("gas_station_metro",  -36.0,  36.0,   0.0, [0.15, 0.90, 1.00]),
        ("gas_station_desert", -36.0, -36.0, 180.0, [1.00, 0.38, 0.65]),
    ]
    for gid, gx, gz, yaw, lcol in gas_stations:
        add_model(
            gid, "assets/models/gas_station.glb", gx, 0.0, gz,
            yaw=yaw, scale=(1.0, 1.0, 1.0),
            collider={"type": "box", "halfExtents": [6.5, 2.8, 4.2], "offsetY": 2.8},
        )
        add_light(f"{gid}_light", gx, 4.5, gz, lcol, radius=24.0, intensity=2.6)

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
    # 6. BIOME 3 — PALM COAST BEACH, LAGOON, DRIVABLE PIER, SAILBOATS & TOWERS
    # =========================================================================
    add_box("lighthouse_quay", 102.0, 0.25, -102.0, 11.0, 1.5, 11.0, [0.46, 0.44, 0.40],
            collider={"type": "box", "halfExtents": [5.5, 1.5, 5.5]})
    add_model(
        "coastal_lighthouse", "assets/models/lighthouse.glb", 102.0, 1.0, -102.0,
        yaw=-45.0, scale=(1.15, 1.15, 1.15),
        collider={"type": "cylinder", "radius": 3.5, "height": 22.0},
    )

    # Drivable Wooden Beach Pier ("type": "deck") + Smooth Wooden Entry Ramp from dry sand!
    wood_col = [0.44, 0.28, 0.16]
    post_col = [0.28, 0.18, 0.10]
    add_box("beach_pier_ramp", 63.0, 0.0, -88.0, 4.4, 0.10, 4.8, wood_col,
            yaw=90.0, visible=False,
            collider={"type": "ramp", "width": 4.8, "height": 0.54, "length": 4.4})
    add_box("beach_pier_ramp_vis", 63.0, 0.22, -88.0, 4.6, 0.22, 4.8, wood_col)
    add_box("beach_pier_deck", 76.0, 0.42, -88.0, 22.0, 0.24, 4.8, wood_col,
            collider={"type": "deck", "halfExtents": [11.0, 0.12, 2.4]})
    for idx, px in enumerate([68.0, 74.0, 80.0, 86.0], 1):
        for side_name, pz in [("n", -85.4), ("s", -90.6)]:
            add_box(f"pier_post_{idx}_{side_name}", px, 0.20, pz, 0.34, 2.2, 0.34, post_col,
                    collider={"type": "cylinder", "radius": 0.22, "height": 2.2})
            if idx in (2, 4):
                add_box(f"pier_lamp_{idx}_{side_name}", px, 1.45, pz, 0.38, 0.38, 0.38, [1.0, 0.80, 0.34], emissive=True)
    # Warm golden dynamic pier point lights reflecting on the lagoon waves!
    add_light("pier_light_mid", 74.0, 2.2, -88.0, [1.0, 0.78, 0.36], radius=22.0, intensity=2.4)
    add_light("pier_light_end", 86.0, 2.2, -88.0, [1.0, 0.80, 0.38], radius=24.0, intensity=2.8)

    # NEW v0.8 Animated Wave-Rocking Sailboats floating in the Lagoon & Offshore Ocean!
    add_model("sailboat_lagoon",  "assets/models/sailboat.glb",  94.0, -0.34, -75.0, yaw=125.0, scale=(1.05, 1.05, 1.05))
    add_model("sailboat_offshore","assets/models/sailboat.glb", 184.0, -0.42, -55.0, yaw=-35.0, scale=(1.35, 1.35, 1.35))

    # NEW v0.8 Coastal Lifeguard Towers overlooking the Palm Coast Lagoon!
    add_model(
        "lifeguard_tower_1", "assets/models/lifeguard_tower.glb", 62.0, 0.0, -72.0,
        yaw=-65.0, scale=(1.0, 1.0, 1.0),
        collider={"type": "box", "halfExtents": [1.8, 3.0, 1.8], "offsetY": 3.0},
    )
    add_model(
        "lifeguard_tower_2", "assets/models/lifeguard_tower.glb", 78.0, 0.0, -61.0,
        yaw=180.0, scale=(1.0, 1.0, 1.0),
        collider={"type": "box", "halfExtents": [1.8, 3.0, 1.8], "offsetY": 3.0},
    )
    add_light("lifeguard_light_1", 62.0, 5.2, -72.0, [1.0, 0.82, 0.42], radius=18.0, intensity=1.8)

    palm_spots = [
        (36.0, -40.0, 1.05), (42.0, -72.0, 1.15), (28.0, -96.0, 1.00),
        (68.0, -28.0, 1.10), (88.0, -34.0, 1.22), (72.0, -56.0, 1.05),
        (100.0, -56.0, 1.18), (58.0, -102.0, 1.12), (46.0, -138.0, 1.20),
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
    # 8. EIGHT NEON 3D STUNT RAMPS ACROSS ALL 4 BIOMES
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
        "name": "SAGE v0.8 — Dynamic HDR Open World (Realistic Water Buoyancy, Point Lights, 3D Particles & 18 Models)",
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
        "lights": lights,
        "entities": entities,
    }

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        json.dump(scene, f, indent=2)

    col_count = sum(1 for e in entities if "collider" in e)
    water_count = sum(1 for e in entities if e["type"] == "water")
    print(f"Generated {OUT_PATH}: {len(entities)} entities, {col_count} colliders, {water_count} water bodies, {len(lights)} dynamic lights!")


if __name__ == "__main__":
    build_world()
