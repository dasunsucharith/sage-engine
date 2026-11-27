"""
SAGE Engine v0.8 — Blender 5.3 Procedural 3D Asset Builder
Generates 3 new game-ready glTF 2.0 (.glb) assets:
  1. assets/models/sailboat.glb        (Animated coastal sailboat gently rocking on waves!)
  2. assets/models/gas_station.glb     (Neon roadside service canopy & fuel pumps)
  3. assets/models/lifeguard_tower.glb (Palm Coast wooden beach lifeguard tower)

Run with:
  "D:\Program Files\Blender Foundation\Blender 5.3\blender.exe" --background --python tools/build_v08_assets.py
"""

import bpy
import math
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
os.makedirs(MODELS_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_mat(name, color, roughness=0.5, metallic=0.0, emission=None, emission_strength=1.0):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission is not None:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def assign_mat(obj, mat):
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)


def add_box(name, loc, scale, mat, rot=(0.0, 0.0, 0.0), parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    assign_mat(obj, mat)
    if parent:
        obj.parent = parent
    return obj


def add_cyl(name, loc, radius, depth, mat, verts=16, rot=(0.0, 0.0, 0.0), parent=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    assign_mat(obj, mat)
    if parent:
        obj.parent = parent
    return obj


def export_glb(filename, has_anim=False):
    out_path = os.path.join(MODELS_DIR, filename)
    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format="GLB",
        export_yup=True,
        export_animations=has_anim,
        export_apply=not has_anim,
    )
    print(f"[SAGE][Blender] Exported {out_path}")


# =============================================================================
# 1. ANIMATED COASTAL SAILBOAT (sailboat.glb)
# =============================================================================
def build_sailboat():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 96
    bpy.context.scene.render.fps = 24

    mat_hull = make_mat("HullWhite", (0.94, 0.95, 0.97), roughness=0.28, metallic=0.05)
    mat_stripe = make_mat("HullNavy", (0.08, 0.22, 0.48), roughness=0.30, metallic=0.1)
    mat_deck = make_mat("TeakDeck", (0.58, 0.36, 0.18), roughness=0.65)
    mat_mast = make_mat("MastAlum", (0.78, 0.80, 0.84), roughness=0.25, metallic=0.85)
    mat_sail = make_mat("SailCream", (0.97, 0.95, 0.90), roughness=0.75)
    mat_accent = make_mat("SailCoral", (0.95, 0.32, 0.22), roughness=0.65)
    mat_lamp = make_mat("MastLight", (1.0, 0.85, 0.45), roughness=0.2, emission=(1.0, 0.85, 0.40), emission_strength=3.0)

    root = bpy.data.objects.new("BoatRoot", None)
    bpy.context.collection.objects.link(root)

    # Tapered hull sections along Y (Blender -Y = SAGE +Z)
    add_box("HullMid",   (0.0,  0.0, 0.35), (2.2, 4.4, 0.95), mat_hull, parent=root)
    add_box("HullBow",   (0.0, -2.7, 0.45), (1.35, 1.9, 0.85), mat_hull, rot=(0.0, 0.0, 0.0), parent=root)
    add_box("HullTip",   (0.0, -3.7, 0.58), (0.55, 0.9, 0.65), mat_hull, parent=root)
    add_box("HullStripe",(0.0, -0.3, 0.52), (2.26, 4.9, 0.22), mat_stripe, parent=root)
    add_box("TeakDeck",  (0.0, -0.2, 0.84), (2.05, 4.6, 0.10), mat_deck, parent=root)
    add_box("Cabin",     (0.0,  0.35, 1.25), (1.45, 2.2, 0.72), mat_hull, parent=root)
    add_box("CabinWin",  (0.0,  0.35, 1.32), (1.50, 1.5, 0.28), mat_stripe, parent=root)

    # Tall Mast & Boom
    add_cyl("Mast", (0.0, -0.45, 4.2), 0.09, 6.8, mat_mast, verts=12, parent=root)
    add_cyl("Boom", (0.0,  0.95, 1.85), 0.06, 2.9, mat_mast, verts=10, rot=(math.radians(90), 0.0, 0.0), parent=root)

    # Main Sail (sleek triangular prism)
    mesh = bpy.data.meshes.new("MainSailMesh")
    verts = [
        (-0.04, -0.35, 1.90), ( 0.04, -0.35, 1.90),
        (-0.04,  2.35, 1.90), ( 0.04,  2.35, 1.90),
        (-0.03, -0.35, 7.35), ( 0.03, -0.35, 7.35),
    ]
    faces = [(0, 2, 4), (1, 5, 3), (0, 1, 3, 2), (2, 3, 5, 4), (0, 4, 5, 1)]
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    sail = bpy.data.objects.new("MainSail", mesh)
    bpy.context.collection.objects.link(sail)
    assign_mat(sail, mat_sail)
    sail.parent = root

    # Front Jib Sail (coral accent!)
    jmesh = bpy.data.meshes.new("JibSailMesh")
    jverts = [
        (-0.03, -0.58, 1.70), ( 0.03, -0.58, 1.70),
        (-0.03, -3.55, 1.15), ( 0.03, -3.55, 1.15),
        (-0.02, -0.58, 6.60), ( 0.02, -0.58, 6.60),
    ]
    jmesh.from_pydata(jverts, [], faces)
    jmesh.update()
    jib = bpy.data.objects.new("JibSail", jmesh)
    bpy.context.collection.objects.link(jib)
    assign_mat(jib, mat_accent)
    jib.parent = root

    # Glowing masthead beacon lantern
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.14, location=(0.0, -0.45, 7.65))
    beacon = bpy.context.active_object
    beacon.name = "MastBeacon"
    assign_mat(beacon, mat_lamp)
    beacon.parent = root

    # Smooth looping wave-rocking keyframe animation on BoatRoot!
    root.rotation_mode = "XYZ"
    keyframes = [
        (1,  (0.0, 0.0,  0.00), ( 0.00,  0.00, 0.0)),
        (25, (0.0, 0.0,  0.20), ( 0.04,  0.09, 0.0)),
        (49, (0.0, 0.0,  0.02), ( 0.00,  0.00, 0.0)),
        (73, (0.0, 0.0, -0.16), (-0.04, -0.09, 0.0)),
        (96, (0.0, 0.0,  0.00), ( 0.00,  0.00, 0.0)),
    ]
    for frame, loc, rot in keyframes:
        bpy.context.scene.frame_set(frame)
        root.location = loc
        root.rotation_euler = rot
        root.keyframe_insert(data_path="location", frame=frame)
        root.keyframe_insert(data_path="rotation_euler", frame=frame)

    export_glb("sailboat.glb", has_anim=True)


# =============================================================================
# 2. NEON ROADSIDE GAS STATION CANOPY (gas_station.glb)
# =============================================================================
def build_gas_station():
    clear_scene()
    mat_concrete = make_mat("StationConcrete", (0.45, 0.47, 0.50), roughness=0.85)
    mat_canopy = make_mat("CanopyDark", (0.14, 0.16, 0.21), roughness=0.45, metallic=0.3)
    mat_pillar = make_mat("PillarWhite", (0.86, 0.88, 0.90), roughness=0.40)
    mat_pump = make_mat("FuelPumpRed", (0.88, 0.16, 0.18), roughness=0.35, metallic=0.2)
    mat_neon_cyan = make_mat("NeonCyan", (0.15, 0.92, 1.0), roughness=0.15, emission=(0.12, 0.90, 1.0), emission_strength=3.5)
    mat_neon_pink = make_mat("NeonPink", (1.0, 0.22, 0.65), roughness=0.15, emission=(1.0, 0.22, 0.65), emission_strength=3.0)
    mat_screen = make_mat("PumpScreen", (0.2, 0.95, 0.6), roughness=0.2, emission=(0.2, 0.95, 0.6), emission_strength=2.2)

    # Raised island pad
    add_box("IslandPad", (0.0, 0.0, 0.12), (13.0, 8.5, 0.24), mat_concrete)

    # High overhead canopy roof + glowing double neon trim!
    add_box("CanopyRoof", (0.0, 0.0, 5.20), (14.2, 9.2, 0.95), mat_canopy)
    add_box("CanopyNeonTop", (0.0, 0.0, 5.52), (14.42, 9.42, 0.16), mat_neon_cyan)
    add_box("CanopyNeonBot", (0.0, 0.0, 4.86), (14.42, 9.42, 0.14), mat_neon_pink)

    # 4 Structural Support Pillars
    for px in (-4.8, 4.8):
        for py in (-2.6, 2.6):
            add_box(f"Pillar_{px}_{py}", (px, py, 2.5), (0.65, 0.65, 4.8), mat_pillar)

    # 2 Fuel Pump Islands with glowing screens
    for px in (-2.2, 2.2):
        add_box(f"PumpBase_{px}", (px, 0.0, 0.30), (1.4, 3.0, 0.36), mat_concrete)
        add_box(f"PumpBody_{px}", (px, 0.0, 1.15), (0.75, 1.15, 1.55), mat_pump)
        add_box(f"PumpScreen_{px}", (px, 0.0, 1.40), (0.79, 0.72, 0.42), mat_screen)

    export_glb("gas_station.glb", has_anim=False)


# =============================================================================
# 3. PALM COAST LIFEGUARD TOWER (lifeguard_tower.glb)
# =============================================================================
def build_lifeguard_tower():
    clear_scene()
    mat_wood = make_mat("BeachWood", (0.62, 0.42, 0.24), roughness=0.78)
    mat_paint = make_mat("TowerWhite", (0.94, 0.94, 0.92), roughness=0.55)
    mat_roof = make_mat("TowerBlue", (0.14, 0.48, 0.78), roughness=0.45)
    mat_buoy = make_mat("RescueRed", (0.95, 0.16, 0.14), roughness=0.35)
    mat_lamp = make_mat("TowerLantern", (1.0, 0.84, 0.42), roughness=0.2, emission=(1.0, 0.84, 0.42), emission_strength=3.0)

    # 4 Tall Wooden Stilts
    for sx in (-1.35, 1.35):
        for sy in (-1.35, 1.35):
            add_box(f"Stilt_{sx}_{sy}", (sx, sy, 1.75), (0.24, 0.24, 3.5), mat_wood)

    # Cross Braces & Viewing Deck Platform
    add_box("DeckFloor", (0.0, 0.0, 3.55), (3.6, 3.6, 0.22), mat_wood)
    add_box("RailFront", (0.0, -1.72, 4.10), (3.5, 0.12, 0.14), mat_paint)
    add_box("RailBack",  (0.0,  1.72, 4.10), (3.5, 0.12, 0.14), mat_paint)
    add_box("RailLeft",  (-1.72, 0.0, 4.10), (0.12, 3.5, 0.14), mat_paint)
    add_box("RailRight", ( 1.72, 0.0, 4.10), (0.12, 3.5, 0.14), mat_paint)

    # Lookout Booth Cabin + Slanted Coastal Roof
    add_box("BoothWalls", (0.0, 0.15, 4.55), (2.25, 2.15, 1.85), mat_paint)
    add_box("BoothRoof",  (0.0, 0.10, 5.75), (3.20, 3.10, 0.28), mat_roof, rot=(math.radians(8.0), 0.0, 0.0))

    # Access Stairs on the side
    for step in range(7):
        add_box(f"Step_{step}", (2.15, -1.6 + step * 0.48, 0.45 + step * 0.48), (1.0, 0.36, 0.12), mat_wood)

    # Red Lifebuoy + Warm Beach Lantern
    bpy.ops.mesh.primitive_torus_add(major_radius=0.38, minor_radius=0.10, location=(0.0, -1.80, 4.30), rotation=(math.radians(90), 0.0, 0.0))
    buoy = bpy.context.active_object
    buoy.name = "RescueBuoy"
    assign_mat(buoy, mat_buoy)

    add_box("TowerLight", (0.0, -1.45, 5.45), (0.30, 0.30, 0.30), mat_lamp)

    export_glb("lifeguard_tower.glb", has_anim=False)


if __name__ == "__main__":
    build_sailboat()
    build_gas_station()
    build_lifeguard_tower()
    print("[SAGE][Blender] All 3 v0.8.0 assets built successfully!")
