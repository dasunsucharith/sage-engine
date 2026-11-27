"""
SAGE Engine v1.0.0 — Blender 5.3 Procedural 3D Asset Builder
Generates 4 landmark glTF 2.0 (.glb) assets (28 total built-in .glb models in SAGE!):
  1. assets/models/suspension_bridge.glb (Grand Cable-Stayed Highway Arch Monument!)
  2. assets/models/stadium_arena.glb     (Illuminated Neon Stunt Stadium with Floodlights!)
  3. assets/models/yacht.glb             (Animated Wave-Rocking Luxury Superyacht!)
  4. assets/models/billboard.glb         (Glowing Neon Highway Billboard Tower!)

Run with:
  "D:/Program Files/Blender Foundation/Blender 5.3/blender.exe" --background --python tools/build_v10_assets.py
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
# 1. GRAND CABLE-STAYED HIGHWAY ARCH MONUMENT (suspension_bridge.glb)
#    Wide 20m clear highway span across X so 4-lane expressways pass right under!
# =============================================================================
def build_suspension_bridge():
    clear_scene()
    mat_red = make_mat("BridgeRed", (0.86, 0.22, 0.16), roughness=0.35, metallic=0.55)
    mat_concrete = make_mat("BridgePillar", (0.48, 0.50, 0.54), roughness=0.80)
    mat_cable = make_mat("BridgeCable", (0.85, 0.87, 0.90), roughness=0.25, metallic=0.85)
    mat_glow = make_mat("BridgeBeacon", (1.0, 0.84, 0.36), roughness=0.15, emission=(1.0, 0.82, 0.32), emission_strength=3.6)

    # Twin Tall Steel Towers framing a wide 20m highway opening (-10.5m and +10.5m on X)
    for sx in (-10.5, 10.5):
        add_box(f"PierBase_{sx}", (sx, 0.0, 1.5), (2.4, 3.6, 3.0), mat_concrete)
        add_box(f"TowerColA_{sx}", (sx, -1.0, 13.5), (1.3, 1.2, 24.0), mat_red)
        add_box(f"TowerColB_{sx}", (sx,  1.0, 13.5), (1.3, 1.2, 24.0), mat_red)
        add_box(f"TowerCap_{sx}",  (sx,  0.0, 25.8), (1.8, 3.8, 0.8), mat_red)
        add_box(f"TowerLight_{sx}",(sx,  0.0, 26.6), (0.65, 0.65, 0.65), mat_glow)

    # Overhead Crossbeams (high 10m+ clearance for vehicles & trucks!)
    add_box("CrossBeamLow",  (0.0, 0.0, 11.2), (22.0, 2.8, 1.1), mat_red)
    add_box("CrossBeamMid",  (0.0, 0.0, 18.2), (22.0, 2.6, 1.0), mat_red)
    add_box("CrossBeamTop",  (0.0, 0.0, 24.4), (22.0, 2.8, 1.2), mat_red)
    add_box("CrossNeon",     (0.0, 0.0, 10.5), (19.6, 0.36, 0.24), mat_glow)

    # Diagonal Suspension Cables fanning out along ±Y on both towers
    for sx in (-10.5, 10.5):
        for i, y_off in enumerate((5.0, 10.0, 15.0)):
            for sy in (-1.0, 1.0):
                z_top = 24.0 - i * 1.4
                y_end = sy * y_off
                mid_y = y_end * 0.5
                mid_z = (z_top + 1.0) * 0.5
                length = math.sqrt(y_end * y_end + (z_top - 1.0) ** 2)
                angle_x = math.atan2(y_end, z_top - 1.0)
                add_cyl(f"Cable_{sx}_{sy}_{i}", (sx, mid_y, mid_z), 0.11, length, mat_cable, verts=8, rot=(-angle_x, 0.0, 0.0))

    export_glb("suspension_bridge.glb", has_anim=False)


# =============================================================================
# 2. NEON STUNT STADIUM / COLOSSEUM ARENA (stadium_arena.glb)
# =============================================================================
def build_stadium_arena():
    clear_scene()
    mat_shell = make_mat("StadiumConcrete", (0.76, 0.78, 0.82), roughness=0.60)
    mat_trim = make_mat("StadiumSteel", (0.16, 0.20, 0.26), roughness=0.35, metallic=0.65)
    mat_neon = make_mat("StadiumCyan", (0.14, 0.88, 1.0), roughness=0.15, emission=(0.14, 0.88, 1.0), emission_strength=3.2)
    mat_flood = make_mat("StadiumFlood", (1.0, 0.96, 0.86), roughness=0.1, emission=(1.0, 0.96, 0.86), emission_strength=4.2)

    # Segmented 16-sided stadium ring wall + glowing roof rim
    segs = 16
    radius = 18.0
    for i in range(segs):
        ang = (2.0 * math.pi * i) / segs
        px = math.cos(ang) * radius
        py = math.sin(ang) * radius
        seg_w = (2.0 * math.pi * radius / segs) + 0.5
        add_box(f"Wall_{i}", (px, py, 4.5), (3.4, seg_w, 9.0), mat_shell, rot=(0.0, 0.0, ang))
        add_box(f"RoofRim_{i}", (px * 0.95, py * 0.95, 9.4), (4.6, seg_w + 0.2, 0.9), mat_trim, rot=(0.0, 0.0, ang))
        add_box(f"NeonBand_{i}", (px * 1.06, py * 1.06, 8.2), (0.35, seg_w, 0.45), mat_neon, rot=(0.0, 0.0, ang))

    # 4 Towering Corner Floodlight Pylons
    for i in range(4):
        ang = math.radians(45.0 + i * 90.0)
        fx = math.cos(ang) * (radius + 1.2)
        fy = math.sin(ang) * (radius + 1.2)
        add_box(f"Pylon_{i}", (fx, fy, 13.0), (1.1, 1.1, 16.0), mat_trim, rot=(0.0, 0.0, ang))
        add_box(f"FloodPanel_{i}", (fx * 0.96, fy * 0.96, 20.8), (0.7, 4.2, 1.8), mat_flood, rot=(0.0, 0.0, ang))

    export_glb("stadium_arena.glb", has_anim=False)


# =============================================================================
# 3. ANIMATED LUXURY SUPERYACHT (yacht.glb — gentle wave rocking!)
# =============================================================================
def build_yacht():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 120
    bpy.context.scene.render.fps = 24

    mat_white = make_mat("YachtWhite", (0.96, 0.97, 0.99), roughness=0.20, metallic=0.10)
    mat_dark = make_mat("YachtHullDark", (0.09, 0.14, 0.22), roughness=0.25, metallic=0.30)
    mat_glass = make_mat("YachtGlass", (0.08, 0.16, 0.26), roughness=0.08, metallic=0.55)
    mat_teak = make_mat("YachtTeak", (0.60, 0.38, 0.20), roughness=0.65)
    mat_glow = make_mat("YachtNeonBlue", (0.12, 0.85, 1.0), roughness=0.15, emission=(0.12, 0.85, 1.0), emission_strength=3.2)
    mat_warm = make_mat("YachtCabinWarm", (1.0, 0.82, 0.45), roughness=0.2, emission=(1.0, 0.80, 0.40), emission_strength=2.4)

    root = bpy.data.objects.new("YachtRoot", None)
    root.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(root)

    # Sleek 14m Multi-Deck Superyacht Hull (faces -Y = SAGE +Z)
    add_box("HullLower",  (0.0,  0.5, 0.55), (4.2, 11.2, 1.35), mat_dark, parent=root)
    add_box("HullBow",    (0.0, -6.2, 0.72), (2.6,  3.2, 1.25), mat_white, parent=root)
    add_box("HullTip",    (0.0, -8.2, 0.88), (1.1,  1.6, 1.05), mat_white, parent=root)
    add_box("HullUpper",  (0.0,  0.2, 1.45), (4.3, 11.8, 0.72), mat_white, parent=root)
    add_box("WaterNeon",  (0.0,  0.5, 0.28), (4.35, 10.6, 0.14), mat_glow, parent=root)
    add_box("MainDeck",   (0.0,  0.2, 1.84), (4.0, 11.2, 0.10), mat_teak, parent=root)

    # Lower & Upper Saloon Decks + Panoramic Windows
    add_box("SaloonL",    (0.0,  1.1, 2.55), (3.3,  6.8, 1.35), mat_white, parent=root)
    add_box("SaloonWin",  (0.0,  0.9, 2.65), (3.42, 5.4, 0.65), mat_glass, parent=root)
    add_box("SaloonGlow", (0.0,  0.9, 2.45), (3.38, 4.8, 0.22), mat_warm, parent=root)
    add_box("FlyBridge",  (0.0,  1.6, 3.65), (2.5,  4.2, 0.95), mat_white, parent=root)
    add_box("BridgeGlass",(0.0, -0.3, 3.70), (2.3,  0.65, 0.60), mat_glass, rot=(math.radians(-24.0), 0.0, 0.0), parent=root)
    add_box("RadarArch",  (0.0,  2.2, 4.75), (2.6,  0.55, 1.35), mat_white, parent=root)
    add_cyl("RadarDome",  (0.0,  2.2, 5.55), 0.42, 0.36, mat_white, verts=14, parent=root)

    # Gentle luxury yacht swell animation
    keys = [
        (1,   (0.0, 0.0,  0.00), ( 0.00,  0.00, 0.0)),
        (31,  (0.0, 0.0,  0.14), ( 0.02,  0.045, 0.0)),
        (61,  (0.0, 0.0,  0.00), ( 0.00,  0.00, 0.0)),
        (91,  (0.0, 0.0, -0.12), (-0.02, -0.045, 0.0)),
        (120, (0.0, 0.0,  0.00), ( 0.00,  0.00, 0.0)),
    ]
    for f, loc, rot in keys:
        bpy.context.scene.frame_set(f)
        root.location = loc
        root.rotation_euler = rot
        root.keyframe_insert(data_path="location", frame=f)
        root.keyframe_insert(data_path="rotation_euler", frame=f)

    export_glb("yacht.glb", has_anim=True)


# =============================================================================
# 4. GLOWING NEON HIGHWAY BILLBOARD TOWER (billboard.glb)
# =============================================================================
def build_billboard():
    clear_scene()
    mat_steel = make_mat("BoardSteel", (0.16, 0.18, 0.22), roughness=0.40, metallic=0.70)
    mat_screen = make_mat("BoardScreen", (0.10, 0.72, 0.96), roughness=0.20, emission=(0.12, 0.76, 1.0), emission_strength=2.6)
    mat_accent = make_mat("BoardMagenta", (1.0, 0.22, 0.65), roughness=0.15, emission=(1.0, 0.22, 0.65), emission_strength=3.4)
    mat_gold = make_mat("BoardGold", (1.0, 0.80, 0.18), roughness=0.15, emission=(1.0, 0.80, 0.18), emission_strength=3.2)

    # Tall Steel Monopole + Catwalk
    add_cyl("Pole", (0.0, 0.0, 4.6), 0.45, 9.2, mat_steel, verts=14)
    add_box("BasePad", (0.0, 0.0, 0.35), (2.2, 2.2, 0.70), mat_steel)
    add_box("Catwalk", (0.0, -0.9, 6.8), (11.2, 0.9, 0.18), mat_steel)

    # Wide Illuminated Billboard Frame + Geometric Neon Bars
    add_box("FrameBack", (0.0, 0.15, 9.5), (11.6, 0.55, 5.2), mat_steel)
    add_box("Screen",    (0.0, -0.16, 9.5), (10.8, 0.12, 4.4), mat_screen)
    add_box("TopTrim",   (0.0, -0.24, 11.9), (11.4, 0.16, 0.26), mat_accent)
    add_box("BotTrim",   (0.0, -0.24,  7.1), (11.4, 0.16, 0.26), mat_accent)
    add_box("LogoBar",   (0.0, -0.26,  9.5), (6.4, 0.14, 0.75), mat_gold)

    export_glb("billboard.glb", has_anim=False)


if __name__ == "__main__":
    build_suspension_bridge()
    build_stadium_arena()
    build_yacht()
    build_billboard()
    print("[SAGE][Blender] All 4 v1.0.0 landmark assets built successfully!")
