"""
Generates scenes/stunt_city.json — SAGE Engine v0.9 MASSIVE 560m x 560m 4-Biome Open World!
More than 3x the playable area of v0.8, featuring:
- 4-lane 540m Cross Expressways + Inner Hub Loop (±64m) + Mid Boulevard (±148m) + Grand Coastal Ring (±236m)
- Carved 72m x 72m Turquoise Palm Lagoon Marina with sunken seabed (-1.35m), 32m Drivable Wooden Pier,
  animated rocking Sailboats, Rotating Lighthouse, 3 Lifeguard Towers & a 24m Spinning Neon Ferris Wheel!
- 24 total Blender .glb models in catalog including Alpine Cabins, Desert Saguaro Cacti, Neon Gas Stations,
  Skyscrapers, Wind Turbines, Ferris Wheel, Sailboats, Patrol Helicopter, AI Traffic & Playable Character!
- Declarative JSON auto-colliders on every world object + 18+ dynamic point lights (nearest-16 sorted on GPU!).
"""

import json
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "scenes", "stunt_city.json"))

entities = []
lights = []


def add_light(lid, x, y, z, color, radius=24.0, intensity=2.4):
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


def add_water(eid, x, y, z, sx, sz, shallow_color, seabed_y=-1.35):
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
    dark_steel = [0.14, 0.16, 0.20]
    span = 18.4
    half_span = span * 0.5
    height = 7.8
    if along_ns:
        add_box(f"{prefix}_pil_l", x - half_span, height * 0.5, z, 0.95, height, 0.95, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_pil_r", x + half_span, height * 0.5, z, 0.95, height, 0.95, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_beam", x, height, z, span + 1.4, 0.75, 1.0, dark_steel)
        add_box(f"{prefix}_neon", x, height - 0.36, z, span - 0.4, 0.24, 0.38, glow_color, emissive=True)
        add_box(f"{prefix}_neon_l", x - half_span + 0.52, height * 0.55, z, 0.18, height * 0.72, 0.30, glow_color, emissive=True)
        add_box(f"{prefix}_neon_r", x + half_span - 0.52, height * 0.55, z, 0.18, height * 0.72, 0.30, glow_color, emissive=True)
    else:
        add_box(f"{prefix}_pil_l", x, height * 0.5, z - half_span, 0.95, height, 0.95, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_pil_r", x, height * 0.5, z + half_span, 0.95, height, 0.95, dark_steel,
                collider={"type": "box", "halfExtents": [0.55, height * 0.5, 0.55]})
        add_box(f"{prefix}_beam", x, height, z, 1.0, 0.75, span + 1.4, dark_steel)
        add_box(f"{prefix}_neon", x, height - 0.36, z, 0.38, 0.24, span - 0.4, glow_color, emissive=True)
        add_box(f"{prefix}_neon_l", x, height * 0.55, z - half_span + 0.52, 0.30, height * 0.72, 0.18, glow_color, emissive=True)
        add_box(f"{prefix}_neon_r", x, height * 0.55, z + half_span - 0.52, 0.30, height * 0.72, 0.18, glow_color, emissive=True)
    add_light(f"{prefix}_light", x, height - 1.2, z, glow_color, radius=28.0, intensity=2.6)


def build_world():
    # =========================================================================
    # 1. 560m x 560m ISLAND GROUNDS + CARVED 72m x 72m LAGOON JOLT FLOOR SLABS
    #    Lagoon cutout cleanly spans x: [156..228], z: [-228..-156]
    # =========================================================================
    # Dry-land Jolt floor slabs framing [156..228, -228..-156] (invisible so zero Z-fighting!)
    add_box("ground_col_west",  -82.0, -1.0,    0.0, 476.0, 2.0, 640.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [238.0, 1.0, 320.0]}, visible=False)
    add_box("ground_col_east",  274.0, -1.0,    0.0,  92.0, 2.0, 640.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [46.0, 1.0, 320.0]}, visible=False)
    add_box("ground_col_north", 192.0, -1.0,   82.0,  72.0, 2.0, 476.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [36.0, 1.0, 238.0]}, visible=False)
    add_box("ground_col_south", 192.0, -1.0, -274.0,  72.0, 2.0,  92.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [36.0, 1.0, 46.0]}, visible=False)
    add_box("lagoon_seabed_col",192.0, -2.35,-192.0,  76.0, 2.0,  76.0, [0.1, 0.1, 0.1],
            collider={"type": "box", "halfExtents": [38.0, 1.0, 38.0]}, visible=False)

    # 4 Non-Overlapping Biome Ground Planes (y = 0.00, 560m x 560m total island!)
    add_plane("biome_downtown",  -140.0, 0.00,  140.0, 280.0, 280.0, [0.18, 0.19, 0.23])
    add_plane("biome_highlands",  140.0, 0.00,  140.0, 280.0, 280.0, [0.15, 0.28, 0.16])
    add_plane("biome_desert",    -140.0, 0.00, -140.0, 280.0, 280.0, [0.65, 0.37, 0.21])

    # SE Palm Coast Beach (4 clean non-overlapping sand slabs framing [156..228, -228..-156]!)
    sand_col = [0.79, 0.69, 0.45]
    add_plane("biome_beach_w",     78.0, 0.00, -140.0, 156.0, 280.0, sand_col)
    add_plane("biome_beach_e",    254.0, 0.00, -140.0,  52.0, 280.0, sand_col)
    add_plane("biome_beach_n",    192.0, 0.00,  -78.0,  72.0, 156.0, sand_col)
    add_plane("biome_beach_s",    192.0, 0.00, -254.0,  72.0,  52.0, sand_col)
    add_plane("lagoon_seabed",    192.0, -1.35,-192.0,  74.0,  74.0, [0.28, 0.34, 0.28])

    # =========================================================================
    # 1B. ANIMATED 3D OCEAN WATER (72m Turquoise Marina Lagoon + Endless Ocean!)
    # =========================================================================
    add_water("palm_lagoon_water", 192.0, -0.26, -192.0, 72.0, 72.0, [0.06, 0.59, 0.72], seabed_y=-1.35)

    ocean_col = [0.04, 0.44, 0.62]
    add_water("ocean_east",   440.0, -0.42,    0.0, 320.0, 1200.0, ocean_col, seabed_y=-4.0)
    add_water("ocean_west",  -440.0, -0.42,    0.0, 320.0, 1200.0, ocean_col, seabed_y=-4.0)
    add_water("ocean_north",    0.0, -0.42,  440.0, 560.0,  320.0, ocean_col, seabed_y=-4.0)
    add_water("ocean_south",    0.0, -0.42, -440.0, 560.0,  320.0, ocean_col, seabed_y=-4.0)

    # =========================================================================
    # 2. GRAND 560m MULTI-RING HIGHWAY NETWORK (ZERO Z-FIGHTING!)
    #    NS Highways at y = 0.04 | EW Highways at y = 0.07
    # =========================================================================
    asphalt_ns = [0.105, 0.115, 0.138]
    asphalt_ew = [0.108, 0.118, 0.140]

    # North-South Expressways & Loops (y = 0.04)
    add_plane("hwy_ns",        0.0, 0.04, 0.0, 16.0, 548.0, asphalt_ns)
    add_plane("loop_in_e",    64.0, 0.04, 0.0, 11.0, 140.0, asphalt_ns)
    add_plane("loop_in_w",   -64.0, 0.04, 0.0, 11.0, 140.0, asphalt_ns)
    add_plane("mid_e",       148.0, 0.04, 0.0, 12.0, 310.0, asphalt_ns)
    add_plane("mid_w",      -148.0, 0.04, 0.0, 12.0, 310.0, asphalt_ns)
    add_plane("ring_out_e",  236.0, 0.04, 0.0, 13.0, 486.0, asphalt_ns)
    add_plane("ring_out_w", -236.0, 0.04, 0.0, 13.0, 486.0, asphalt_ns)

    # East-West Expressways & Loops (y = 0.07)
    add_plane("hwy_ew",        0.0, 0.07,    0.0, 548.0, 16.0, asphalt_ew)
    add_plane("loop_in_n",     0.0, 0.07,   64.0, 140.0, 11.0, asphalt_ew)
    add_plane("loop_in_s",     0.0, 0.07,  -64.0, 140.0, 11.0, asphalt_ew)
    add_plane("mid_n",         0.0, 0.07,  148.0, 310.0, 12.0, asphalt_ew)
    add_plane("mid_s",         0.0, 0.07, -148.0, 310.0, 12.0, asphalt_ew)
    add_plane("ring_out_n",    0.0, 0.07,  236.0, 486.0, 13.0, asphalt_ew)
    add_plane("ring_out_s",    0.0, 0.07, -236.0, 486.0, 13.0, asphalt_ew)

    # =========================================================================
    # 3. PERIMETER COASTAL PROMENADE SEAWALLS (±278m!)
    # =========================================================================
    wall_col = [0.34, 0.37, 0.42]
    sand_wall = [0.58, 0.54, 0.46]
    add_box("wall_north",   0.0, 0.85,  278.0, 558.0, 1.7, 2.2, wall_col,
            collider={"type": "box", "halfExtents": [280.0, 3.0, 1.3]})
    add_box("wall_south",   0.0, 0.70, -278.0, 558.0, 1.4, 2.2, sand_wall,
            collider={"type": "box", "halfExtents": [280.0, 3.0, 1.3]})
    add_box("wall_east",  278.0, 0.70,    0.0, 2.2, 1.4, 558.0, sand_wall,
            collider={"type": "box", "halfExtents": [1.3, 3.0, 280.0]})
    add_box("wall_west", -278.0, 0.85,    0.0, 2.2, 1.7, 558.0, wall_col,
            collider={"type": "box", "halfExtents": [1.3, 3.0, 280.0]})

    # =========================================================================
    # 4. BIOME 1 — EXPANDED DOWNTOWN METRO SKYLINE (Central Hub + North-West)
    # =========================================================================
    add_neon_arch("arch_n",   0.0,  20.0, True,  [0.12, 0.88, 1.00])
    add_neon_arch("arch_s",   0.0, -20.0, True,  [1.00, 0.25, 0.68])
    add_neon_arch("arch_e",  20.0,   0.0, False, [0.18, 1.00, 0.58])
    add_neon_arch("arch_w", -20.0,   0.0, False, [1.00, 0.72, 0.14])

    towers = [
        # Central Hub Plaza
        ("tower_nw_hub",  -28.0,  28.0,   0.0, (1.05, 1.30, 1.05), [0.95, 1.00, 1.08]),
        ("tower_ne_hub",   28.0,  28.0,  90.0, (1.05, 1.15, 1.05), [0.92, 1.04, 0.98]),
        ("tower_sw_hub",  -28.0, -28.0, 180.0, (1.05, 1.05, 1.05), [1.06, 0.94, 0.88]),
        ("tower_se_hub",   28.0, -28.0, -90.0, (1.05, 1.20, 1.05), [1.05, 0.90, 0.98]),
        # Mid Downtown District
        ("tower_dt_1",   -104.0,  32.0,   0.0, (1.15, 1.65, 1.15), [0.90, 0.96, 1.10]),
        ("tower_dt_2",    -32.0, 104.0,  90.0, (1.15, 1.50, 1.15), [1.08, 0.90, 1.02]),
        ("tower_dt_3",   -106.0, 106.0, 180.0, (1.30, 1.95, 1.30), [0.88, 1.02, 1.12]),
        ("tower_dt_4",    -84.0, 122.0, -90.0, (1.00, 1.35, 1.00), [1.04, 0.96, 0.90]),
        ("tower_dt_5",   -122.0,  82.0,  45.0, (1.00, 1.40, 1.00), [0.96, 0.92, 1.08]),
        # Outer Financial Skyline (between Mid ±148 and Ring ±236!)
        ("tower_dt_6",   -186.0,  44.0,   0.0, (1.20, 1.85, 1.20), [0.90, 1.00, 1.12]),
        ("tower_dt_7",   -190.0, 104.0,  90.0, (1.15, 1.60, 1.15), [1.02, 0.92, 1.06]),
        ("tower_dt_9",   -104.0, 188.0, -90.0, (1.15, 1.65, 1.15), [1.06, 0.94, 0.92]),
        ("tower_dt_10",   -40.0, 188.0,  45.0, (1.10, 1.55, 1.10), [0.94, 1.04, 1.06]),
    ]
    for tid, tx, tz, yaw, scl, tint in towers:
        add_box(tid + "_pad", tx, 0.14, tz, 13.8 * scl[0], 0.28, 13.8 * scl[2], [0.35, 0.37, 0.42])
        add_model(
            tid, "assets/models/city_tower.glb", tx, 0.28, tz,
            yaw=yaw, scale=scl, color=tint,
            collider={"type": "box", "halfExtents": [5.9 * scl[0], 12.0 * scl[1], 5.9 * scl[2]], "offsetY": 12.0 * scl[1]},
        )

    # NEW v1.0.0 Grand Neon Stunt Stadium Colosseum in NW Outer Block (-192, 192)!
    add_model(
        "metro_stadium", "assets/models/stadium_arena.glb", -192.0, 0.0, 192.0,
        yaw=22.5, scale=(1.15, 1.15, 1.15),
        collider={"type": "cylinder", "radius": 21.5, "height": 12.0},
    )
    add_light("stadium_flood_light", -192.0, 22.0, 192.0, [0.92, 0.96, 1.0], radius=56.0, intensity=3.8)

    # NEW v1.0.0 Grand Cable-Stayed Highway Arch Monuments spanning North & South Expressways!
    for bid, bz in [("bridge_arch_n", 108.0), ("bridge_arch_s", -108.0)]:
        add_model(bid, "assets/models/suspension_bridge.glb", 0.0, 0.0, bz, yaw=0.0, scale=(1.0, 1.0, 1.0))
        add_box(bid + "_col_l", -10.5, 6.0, bz, 2.4, 12.0, 3.6, [0.2, 0.2, 0.2],
                collider={"type": "box", "halfExtents": [1.3, 6.0, 1.9]}, visible=False)
        add_box(bid + "_col_r",  10.5, 6.0, bz, 2.4, 12.0, 3.6, [0.2, 0.2, 0.2],
                collider={"type": "box", "halfExtents": [1.3, 6.0, 1.9]}, visible=False)
        add_light(bid + "_light", 0.0, 10.0, bz, [1.0, 0.82, 0.34], radius=34.0, intensity=3.0)

    # NEW v1.0.0 Glowing Neon Highway Billboards lining the Grand Expressways!
    billboards = [
        ("billboard_n",  15.5,  88.0, -35.0, [0.14, 0.85, 1.0]),
        ("billboard_s", -15.5, -88.0, 145.0, [1.0, 0.24, 0.65]),
        ("billboard_e",  88.0,  15.5, -125.0, [0.14, 0.85, 1.0]),
        ("billboard_w", -88.0, -15.5,  55.0, [1.0, 0.78, 0.20]),
    ]
    for bbid, bx, bz, yaw, lcol in billboards:
        add_model(
            bbid, "assets/models/billboard.glb", bx, 0.0, bz,
            yaw=yaw, scale=(1.0, 1.0, 1.0),
            collider={"type": "cylinder", "radius": 0.75, "height": 10.0},
        )
        add_light(bbid + "_light", bx, 9.5, bz, lcol, radius=24.0, intensity=2.4)

    # Neon Roadside Gas Stations across Metro, Desert & Outer Highway!
    gas_stations = [
        ("gas_station_metro",  -42.0,   42.0,   0.0, [0.15, 0.90, 1.00]),
        ("gas_station_north",  -82.0,  212.0, 180.0, [0.22, 1.00, 0.65]),
        ("gas_station_desert", -42.0,  -42.0, 180.0, [1.00, 0.38, 0.65]),
        ("gas_station_canyon",-192.0, -104.0,  90.0, [1.00, 0.65, 0.18]),
    ]
    for gid, gx, gz, yaw, lcol in gas_stations:
        add_model(
            gid, "assets/models/gas_station.glb", gx, 0.0, gz,
            yaw=yaw, scale=(1.0, 1.0, 1.0),
            collider={"type": "box", "halfExtents": [6.5, 2.8, 4.2], "offsetY": 2.8},
        )
        add_light(f"{gid}_light", gx, 4.5, gz, lcol, radius=26.0, intensity=2.6)

    warehouses = [
        ("warehouse_dt_w",   -110.0,  172.0,   0.0),
        ("warehouse_dt_n",    -42.0,  258.0, 180.0),
        ("warehouse_desert", -110.0, -110.0,  90.0),
        ("warehouse_alpine",  110.0,  110.0, -90.0),
    ]
    for wid, wx, wz, yaw in warehouses:
        add_model(
            wid, "assets/models/warehouse.glb", wx, 0.0, wz,
            yaw=yaw, scale=(1.0, 1.0, 1.0),
            collider={"type": "box", "halfExtents": [7.2, 3.6, 5.2], "offsetY": 3.6},
        )

    # Street Lamps lining main avenues
    lamp_spots = []
    for d in (-200.0, -112.0, -42.0, 42.0, 112.0, 200.0):
        lamp_spots.append(( 10.5, d, -90.0))
        lamp_spots.append((-10.5, d,  90.0))
        lamp_spots.append((d,  10.5, 180.0))
        lamp_spots.append((d, -10.5,   0.0))
    for idx, (lx, lz, yaw) in enumerate(lamp_spots, 1):
        add_model(
            f"lamp_{idx}", "assets/models/street_lamp.glb", lx, 0.0, lz,
            yaw=yaw,
            collider={"type": "cylinder", "radius": 0.35, "height": 6.5},
        )

    # =========================================================================
    # 5. BIOME 2 — PINE HIGHLANDS, ALPINE CABIN VILLAGE & WIND RIDGE (NE)
    # =========================================================================
    # NEW v0.9 Warm-Lit A-Frame Alpine Cabins!
    cabins = [
        ("alpine_cabin_1", 104.0,  86.0, -135.0),
        ("alpine_cabin_2", 188.0, 108.0,  -90.0),
        ("alpine_cabin_3", 184.0, 192.0, -155.0),
    ]
    for cid, cx, cz, yaw in cabins:
        add_model(
            cid, "assets/models/alpine_cabin.glb", cx, 0.0, cz,
            yaw=yaw, scale=(1.05, 1.05, 1.05),
            collider={"type": "box", "halfExtents": [4.5, 3.4, 5.6], "offsetY": 3.4},
        )
        add_light(f"{cid}_light", cx, 3.2, cz, [1.0, 0.72, 0.30], radius=24.0, intensity=2.4)

    pine_spots = [
        (38.0, 88.0, 1.10), (50.0, 112.0, 1.25), (32.0, 128.0, 1.00),
        (88.0, 36.0, 1.15), (108.0, 48.0, 1.30), (126.0, 30.0, 1.05),
        (82.0, 122.0, 1.20), (124.0, 84.0, 1.35), (84.0, 174.0, 1.15),
        (118.0, 184.0, 1.30), (124.0, 214.0, 1.18), (44.0, 186.0, 1.25),
        (48.0, 214.0, 1.12), (176.0, 44.0, 1.28), (212.0, 52.0, 1.35),
        (178.0, 76.0, 1.18), (216.0, 96.0, 1.24), (214.0, 172.0, 1.38),
        (172.0, 214.0, 1.26), (258.0, 48.0, 1.30), (258.0, 120.0, 1.35),
        (258.0, 198.0, 1.22), (82.0, 258.0, 1.30), (178.0, 258.0, 1.35),
        (42.0, 42.0, 1.05), (96.0, 208.0, 1.20), (210.0, 214.0, 1.32),
    ]
    for idx, (px, pz, s) in enumerate(pine_spots, 1):
        add_model(
            f"pine_{idx}", "assets/models/pine_tree.glb", px, 0.0, pz,
            yaw=(idx * 37.0) % 360.0, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 0.65 * s, "height": 9.0 * s},
        )

    turbines = [
        ("wind_turbine_ne_1", 118.0,  196.0, -135.0),
        ("wind_turbine_ne_2", 214.0,   72.0, -110.0),
        ("wind_turbine_ne_3", 258.0,  166.0, -145.0),
        ("wind_turbine_ne_4", 136.0,  258.0, -120.0),
        ("wind_turbine_sw_1",-196.0, -196.0,   45.0),
        ("wind_turbine_sw_2",-258.0,  -96.0,   70.0),
        ("wind_turbine_sw_3",-118.0, -258.0,   30.0),
    ]
    for tid, tx, tz, yaw in turbines:
        add_model(
            tid, "assets/models/wind_turbine.glb", tx, 0.0, tz,
            yaw=yaw, scale=(1.35, 1.35, 1.35),
            collider={"type": "cylinder", "radius": 1.05, "height": 20.0},
        )

    # =========================================================================
    # 6. BIOME 3 — PALM COAST RESORT, FERRIS WHEEL, 72m MARINA LAGOON & PIER
    # =========================================================================
    # NEW v0.9 24m Glowing Spinning Coastal Boardwalk Ferris Wheel!
    add_model(
        "coastal_ferris_wheel", "assets/models/ferris_wheel.glb", 118.0, 0.0, -192.0,
        yaw=90.0, scale=(1.15, 1.15, 1.15),
        collider={"type": "box", "halfExtents": [4.2, 12.0, 8.2], "offsetY": 12.0},
    )
    add_light("ferris_light_c", 118.0, 14.0, -192.0, [0.15, 0.90, 1.0], radius=38.0, intensity=3.2)
    add_light("ferris_light_p", 118.0,  5.5, -192.0, [1.0, 0.25, 0.70], radius=32.0, intensity=2.6)

    # Lighthouse Point inside SE Marina Corner!
    add_box("lighthouse_quay", 214.0, 0.25, -214.0, 12.0, 1.6, 12.0, [0.46, 0.44, 0.40],
            collider={"type": "box", "halfExtents": [6.0, 1.6, 6.0]})
    add_model(
        "coastal_lighthouse", "assets/models/lighthouse.glb", 214.0, 1.05, -214.0,
        yaw=-45.0, scale=(1.25, 1.25, 1.25),
        collider={"type": "cylinder", "radius": 3.8, "height": 24.0},
    )

    # 32m Drivable Wooden Beach Pier ("type": "deck") + Smooth Wooden Entry Ramp from dry sand!
    wood_col = [0.44, 0.28, 0.16]
    post_col = [0.28, 0.18, 0.10]
    add_box("beach_pier_ramp", 155.0, 0.0, -192.0, 4.4, 0.10, 5.2, wood_col,
            yaw=90.0, visible=False,
            collider={"type": "ramp", "width": 5.2, "height": 0.54, "length": 4.4})
    add_box("beach_pier_ramp_vis", 155.0, 0.22, -192.0, 4.6, 0.22, 5.2, wood_col)
    add_box("beach_pier_deck", 173.0, 0.42, -192.0, 32.0, 0.24, 5.2, wood_col,
            collider={"type": "deck", "halfExtents": [16.0, 0.12, 2.6]})
    for idx, px in enumerate([161.0, 169.0, 177.0, 185.0], 1):
        for side_name, pz in [("n", -189.2), ("s", -194.8)]:
            add_box(f"pier_post_{idx}_{side_name}", px, 0.20, pz, 0.34, 2.2, 0.34, post_col,
                    collider={"type": "cylinder", "radius": 0.22, "height": 2.2})
            if idx in (2, 4):
                add_box(f"pier_lamp_{idx}_{side_name}", px, 1.45, pz, 0.38, 0.38, 0.38, [1.0, 0.80, 0.34], emissive=True)
    add_light("pier_light_mid", 169.0, 2.4, -192.0, [1.0, 0.78, 0.36], radius=24.0, intensity=2.4)
    add_light("pier_light_end", 185.0, 2.4, -192.0, [1.0, 0.80, 0.38], radius=28.0, intensity=2.8)

    # 3 Animated Wave-Rocking Sailboats + 2 NEW v1.0.0 Animated Luxury Superyachts!
    add_model("sailboat_lagoon_1", "assets/models/sailboat.glb", 204.0, -0.34, -172.0, yaw=125.0, scale=(1.10, 1.10, 1.10))
    add_model("sailboat_lagoon_2", "assets/models/sailboat.glb", 176.0, -0.34, -214.0, yaw= 40.0, scale=(1.00, 1.00, 1.00))
    add_model("sailboat_offshore", "assets/models/sailboat.glb", 312.0, -0.44,  -95.0, yaw=-35.0, scale=(1.45, 1.45, 1.45))
    add_model("yacht_marina",      "assets/models/yacht.glb",    204.0, -0.30, -196.0, yaw=-25.0, scale=(1.00, 1.00, 1.00))
    add_model("yacht_offshore",    "assets/models/yacht.glb",    316.0, -0.44, -185.0, yaw= 15.0, scale=(1.25, 1.25, 1.25))
    add_light("yacht_marina_light", 204.0, 2.2, -196.0, [0.14, 0.86, 1.0], radius=24.0, intensity=2.4)

    # 3 Coastal Lifeguard Towers overlooking the Beach & Lagoon
    lifeguards = [
        ("lifeguard_tower_1", 132.0, -172.0, -70.0),
        ("lifeguard_tower_2", 186.0, -134.0, 180.0),
        ("lifeguard_tower_3", 258.0, -192.0,  90.0),
    ]
    for lid, lx, lz, yaw in lifeguards:
        add_model(
            lid, "assets/models/lifeguard_tower.glb", lx, 0.0, lz,
            yaw=yaw, scale=(1.0, 1.0, 1.0),
            collider={"type": "box", "halfExtents": [1.8, 3.0, 1.8], "offsetY": 3.0},
        )
    add_light("lifeguard_light_1", 132.0, 5.2, -172.0, [1.0, 0.82, 0.42], radius=20.0, intensity=2.0)

    palm_spots = [
        (42.0, -42.0, 1.08), (44.0, -88.0, 1.18), (34.0, -124.0, 1.02),
        (88.0, -34.0, 1.15), (118.0, -44.0, 1.25), (96.0, -96.0, 1.12),
        (124.0, -118.0, 1.20), (88.0, -176.0, 1.15), (56.0, -188.0, 1.22),
        (76.0, -216.0, 1.14), (176.0, -42.0, 1.25), (212.0, -56.0, 1.18),
        (182.0, -98.0, 1.20), (216.0, -118.0, 1.14), (258.0, -44.0, 1.28),
        (258.0, -114.0, 1.20), (258.0, -220.0, 1.25), (86.0, -258.0, 1.18),
        (178.0, -258.0, 1.24), (36.0, -210.0, 1.12), (118.0, -218.0, 1.16),
    ]
    for idx, (px, pz, s) in enumerate(palm_spots, 1):
        add_model(
            f"palm_{idx}", "assets/models/palm_tree.glb", px, 0.0, pz,
            yaw=(idx * 47.0) % 360.0, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 0.55 * s, "height": 8.0 * s},
        )

    # =========================================================================
    # 7. BIOME 4 — RED ROCK DESERT CANYON & SAGUARO CACTUS VALLEY (SW)
    # =========================================================================
    rock_spots = [
        (-42.0,  -92.0, 1.20,  15.0), (-92.0,  -40.0, 1.35,  70.0),
        (-118.0, -46.0, 1.55, 140.0), (-84.0, -118.0, 1.30, 210.0),
        (-42.0, -124.0, 1.15, 300.0), (-188.0, -42.0, 1.70,  40.0),
        (-204.0,-168.0, 1.85, 110.0), (-174.0,-206.0, 1.95, 200.0),
        (-104.0,-196.0, 1.55, 250.0), (-42.0, -196.0, 1.40,  85.0),
        (-258.0, -44.0, 1.75, 130.0), (-258.0,-176.0, 1.90,  25.0),
        (-192.0,-258.0, 1.65, 275.0), (-58.0, -258.0, 1.50, 160.0),
    ]
    for idx, (rx, rz, s, yaw) in enumerate(rock_spots, 1):
        add_model(
            f"desert_rock_{idx}", "assets/models/desert_rock.glb", rx, 0.0, rz,
            yaw=yaw, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 4.2 * s, "height": 7.5 * s},
        )

    # NEW v0.9 Desert Saguaro Cacti (ALL with solid cylinder colliders!)
    cactus_spots = [
        (-30.0, -82.0, 1.05), (-82.0, -78.0, 1.20), (-124.0, -84.0, 0.95),
        (-174.0,-78.0, 1.15), (-212.0,-82.0, 1.25), (-178.0,-124.0, 1.00),
        (-120.0,-176.0, 1.18), (-82.0,-182.0, 1.10), (-36.0, -172.0, 0.98),
        (-182.0,-186.0, 1.22), (-216.0,-210.0, 1.15), (-86.0, -214.0, 1.08),
        (-256.0,-126.0, 1.20), (-142.0,-258.0, 1.12), (-32.0, -218.0, 1.05),
        (-104.0, -26.0, 1.10), (-172.0, -24.0, 1.16), (-214.0, -26.0, 1.02),
    ]
    for idx, (cx, cz, s) in enumerate(cactus_spots, 1):
        add_model(
            f"cactus_{idx}", "assets/models/cactus.glb", cx, 0.0, cz,
            yaw=(idx * 53.0) % 360.0, scale=(s, s, s),
            collider={"type": "cylinder", "radius": 0.45 * s, "height": 4.8 * s},
        )

    # =========================================================================
    # 8. TWELVE NEON 3D STUNT RAMPS ACROSS THE 560m 4-BIOME ISLAND!
    # =========================================================================
    ramps = [
        ("ramp_hub_n",     0.0,   36.0,    0.0, 1.05),
        ("ramp_hub_s",     0.0,  -36.0,  180.0, 1.05),
        ("ramp_hub_e",    36.0,    0.0,   90.0, 1.05),
        ("ramp_hub_w",   -36.0,    0.0,  -90.0, 1.05),
        ("ramp_metro",   -64.0,  104.0,    0.0, 1.25),
        ("ramp_pine",    104.0,   64.0,   90.0, 1.25),
        ("ramp_beach",    64.0, -104.0,  180.0, 1.25),
        ("ramp_desert", -104.0,  -64.0,  -90.0, 1.25),
        ("ramp_ring_n",    0.0,  196.0,    0.0, 1.35),
        ("ramp_ring_s",    0.0, -196.0,  180.0, 1.35),
        ("ramp_ring_e",  196.0,    0.0,   90.0, 1.35),
        ("ramp_ring_w", -196.0,    0.0,  -90.0, 1.35),
    ]
    for rid, rx, rz, yaw, s in ramps:
        add_model(
            rid, "assets/models/stunt_ramp.glb", rx, 0.05, rz,
            yaw=yaw, scale=(s, s, s),
            collider={"type": "ramp", "width": 5.0 * s, "height": 2.2 * s, "length": 7.5 * s},
        )

    scene = {
        "name": "SAGE v1.0.0 — Production 560m Grand Open World (28 Models, Weather/Wet Roads, 2-Cascade CSM & Live Editor)",
        "version": 2,
        "sky": {
            "timeOfDay": 17.4,
            "ambientIntensity": 0.40,
            "fogStart": 185.0,
            "fogEnd": 640.0,
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
