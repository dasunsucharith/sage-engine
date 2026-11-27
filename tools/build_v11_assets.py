"""
SAGE Engine v1.1.0 — Blender 5.3 High-Detail Player & Animated World Asset Builder
Upgrades & generates 6 detailed glTF 2.0 (.glb) assets (32 total models in SAGE!):
  1. assets/models/character.glb  (Sculpted Pro Racing Hero with 4 NAMED Animation Clips: Idle, Walk, Run, Swim!)
  2. assets/models/speedboat.glb  (Drivable Luxury Racing Speedboat with spinning twin propellers!)
  3. assets/models/superbike.glb  (Futuristic Cyberpunk Sport Motorcycle with glowing wheel rims!)
  4. assets/models/fountain.glb   (Animated Multi-Tier Downtown Marble Plaza Fountain!)
  5. assets/models/seagull.glb    (Animated Wing-Flapping Coastal Seagull!)
  6. assets/models/jet_plane.glb  (Supersonic Coastal Patrol Jet with glowing afterburners!)

Run with:
  "D:/Program Files/Blender Foundation/Blender 5.3/blender.exe" --background --python tools/build_v11_assets.py
"""

import bpy
import math
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
os.makedirs(MODELS_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_mat(name, color, roughness=0.45, metallic=0.0, emission=None, emission_strength=1.0):
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


def smooth_shade(obj):
    if obj.type == "MESH" and obj.data and hasattr(obj.data, "polygons"):
        for poly in obj.data.polygons:
            poly.use_smooth = True


def add_box(name, loc, scale, mat, rot=(0.0, 0.0, 0.0), parent=None, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    assign_mat(obj, mat)
    if bevel > 0.0:
        mod = obj.modifiers.new(name="Bevel", type="BEVEL")
        mod.width = bevel
        mod.segments = 2
        smooth_shade(obj)
    if parent:
        obj.parent = parent
    return obj


def add_cyl(name, loc, radius, depth, mat, verts=20, rot=(0.0, 0.0, 0.0), parent=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    assign_mat(obj, mat)
    smooth_shade(obj)
    if parent:
        obj.parent = parent
    return obj


def add_sphere(name, loc, radius, mat, scale=(1.0, 1.0, 1.0), parent=None, segs=20, rings=14):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segs, ring_count=rings, radius=radius, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    assign_mat(obj, mat)
    smooth_shade(obj)
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
# 1. HIGH-DETAIL SCULPTED PLAYER HERO (character.glb — 4 Named Anim Clips!)
#    Named NLA Strips exported cleanly as separate glTF clips: Idle, Walk, Run, Swim!
# =============================================================================
def build_hero_character():
    clear_scene()
    bpy.context.scene.render.fps = 24

    mat_suit = make_mat("HeroSuitDark", (0.11, 0.14, 0.20), roughness=0.42, metallic=0.18)
    mat_jacket = make_mat("HeroJacketBlue", (0.10, 0.48, 0.90), roughness=0.35, metallic=0.25)
    mat_armor = make_mat("HeroWhiteArmor", (0.94, 0.96, 0.99), roughness=0.22, metallic=0.35)
    mat_glove = make_mat("HeroGloveRed", (0.88, 0.18, 0.20), roughness=0.45, metallic=0.10)
    mat_boot = make_mat("HeroBootDark", (0.07, 0.08, 0.10), roughness=0.40, metallic=0.25)
    mat_visor = make_mat("HeroVisorCyan", (0.08, 0.88, 1.0), roughness=0.06, metallic=0.80, emission=(0.08, 0.85, 1.0), emission_strength=2.4)
    mat_gold = make_mat("HeroTrimGold", (1.0, 0.78, 0.18), roughness=0.28, metallic=0.75, emission=(1.0, 0.76, 0.14), emission_strength=1.2)

    root = bpy.data.objects.new("HeroRoot", None)
    root.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(root)

    # Sculpted Pelvis & Torso Pivot (faces Blender -Y = SAGE +Z)
    torso_piv = bpy.data.objects.new("TorsoPivot", None)
    torso_piv.location = (0.0, 0.0, 0.94)
    torso_piv.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(torso_piv)
    torso_piv.parent = root

    add_sphere("Pelvis", (0.0, 0.0, -0.02), 0.18, mat_suit, scale=(1.15, 0.82, 0.75), parent=torso_piv)
    add_box("Belt", (0.0, 0.0, 0.03), (0.40, 0.27, 0.07), mat_boot, parent=torso_piv, bevel=0.02)
    add_box("BeltBuckle", (0.0, -0.14, 0.03), (0.11, 0.04, 0.07), mat_gold, parent=torso_piv)
    add_sphere("ChestCore", (0.0, 0.0, 0.28), 0.24, mat_jacket, scale=(1.12, 0.72, 1.18), parent=torso_piv)
    add_box("ChestPlate", (0.0, -0.14, 0.32), (0.38, 0.09, 0.24), mat_armor, parent=torso_piv, bevel=0.03)
    add_box("ChestGlow", (0.0, -0.19, 0.34), (0.24, 0.03, 0.045), mat_visor, parent=torso_piv)
    add_sphere("ShoulderL", (-0.29, 0.0, 0.44), 0.105, mat_armor, scale=(1.0, 0.95, 0.95), parent=torso_piv)
    add_sphere("ShoulderR", ( 0.29, 0.0, 0.44), 0.105, mat_armor, scale=(1.0, 0.95, 0.95), parent=torso_piv)

    # Sleek Aero Jetpack / Backpack
    add_box("AeroPack", (0.0, 0.19, 0.30), (0.30, 0.14, 0.38), mat_armor, parent=torso_piv, bevel=0.03)
    add_box("PackStrip", (0.0, 0.26, 0.30), (0.06, 0.03, 0.28), mat_visor, parent=torso_piv)

    # Head Pivot + Aerodynamic Racing Helmet & Curved Glossy Visor
    head_piv = bpy.data.objects.new("HeadPivot", None)
    head_piv.location = (0.0, 0.0, 0.56)
    head_piv.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(head_piv)
    head_piv.parent = torso_piv

    add_cyl("Neck", (0.0, 0.0, 0.04), 0.075, 0.10, mat_suit, verts=14, parent=head_piv)
    add_sphere("HelmetShell", (0.0, 0.01, 0.19), 0.20, mat_armor, scale=(0.94, 1.08, 1.02), parent=head_piv)
    add_sphere("HelmetVisor", (0.0, -0.09, 0.19), 0.165, mat_visor, scale=(0.96, 0.85, 0.52), parent=head_piv)
    add_box("HelmetFin", (0.0, 0.08, 0.36), (0.04, 0.24, 0.07), mat_jacket, parent=head_piv)

    # Articulated Arms (Upper Arm + Elbow Forearm + Racing Glove)
    def make_arm(side_name, sx):
        piv = bpy.data.objects.new(f"Arm{side_name}Pivot", None)
        piv.location = (sx * 0.31, 0.0, 0.43)
        piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(piv)
        piv.parent = torso_piv
        add_cyl(f"UpperArm{side_name}", (0.0, 0.0, -0.14), 0.068, 0.28, mat_jacket, verts=14, parent=piv)
        add_sphere(f"Elbow{side_name}", (0.0, 0.0, -0.28), 0.064, mat_suit, parent=piv)
        add_cyl(f"ForeArm{side_name}", (0.0, -0.02, -0.41), 0.056, 0.26, mat_armor, verts=14, rot=(math.radians(10), 0, 0), parent=piv)
        add_sphere(f"Hand{side_name}", (0.0, -0.05, -0.55), 0.062, mat_glove, scale=(0.85, 1.05, 1.15), parent=piv)
        return piv

    arm_l = make_arm("L", -1.0)
    arm_r = make_arm("R",  1.0)

    # Articulated Legs (Thigh + Knee Armor + Shin + Sculpted Racing Boot)
    def make_leg(side_name, sx):
        piv = bpy.data.objects.new(f"Leg{side_name}Pivot", None)
        piv.location = (sx * 0.13, 0.0, 0.88)
        piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(piv)
        piv.parent = root
        add_cyl(f"Thigh{side_name}", (0.0, 0.0, -0.21), 0.092, 0.40, mat_suit, verts=16, parent=piv)
        add_sphere(f"Knee{side_name}", (0.0, -0.04, -0.42), 0.075, mat_armor, scale=(0.9, 0.9, 1.1), parent=piv)
        add_cyl(f"Shin{side_name}", (0.0, 0.0, -0.62), 0.074, 0.38, mat_jacket, verts=16, parent=piv)
        add_box(f"Boot{side_name}", (0.0, -0.06, -0.82), (0.14, 0.27, 0.12), mat_boot, parent=piv, bevel=0.025)
        add_box(f"BootSole{side_name}", (0.0, -0.06, -0.87), (0.15, 0.28, 0.03), mat_gold, parent=piv)
        return piv

    leg_l = make_leg("L", -1.0)
    leg_r = make_leg("R",  1.0)

    # Helper to push a named animation clip onto NLA tracks across all animated nodes!
    animated_objs = [root, torso_piv, head_piv, arm_l, arm_r, leg_l, leg_r]

    def record_clip(clip_name, keyframes):
        # Create fresh action per object, record keyframes, then stash to NLA strip with clip_name
        for obj in animated_objs:
            if not obj.animation_data:
                obj.animation_data_create()
            act = bpy.data.actions.new(name=f"{clip_name}_{obj.name}")
            obj.animation_data.action = act

        for frame, pose in keyframes:
            bpy.context.scene.frame_set(frame)
            root.location = pose.get("root_loc", (0.0, 0.0, 0.0))
            root.rotation_euler = pose.get("root_rot", (0.0, 0.0, 0.0))
            torso_piv.rotation_euler = pose.get("torso_rot", (0.0, 0.0, 0.0))
            head_piv.rotation_euler = pose.get("head_rot", (0.0, 0.0, 0.0))
            arm_l.rotation_euler = pose.get("arm_l", (0.0, 0.0, 0.0))
            arm_r.rotation_euler = pose.get("arm_r", (0.0, 0.0, 0.0))
            leg_l.rotation_euler = pose.get("leg_l", (0.0, 0.0, 0.0))
            leg_r.rotation_euler = pose.get("leg_r", (0.0, 0.0, 0.0))

            root.keyframe_insert(data_path="location", frame=frame)
            root.keyframe_insert(data_path="rotation_euler", frame=frame)
            for o in (torso_piv, head_piv, arm_l, arm_r, leg_l, leg_r):
                o.keyframe_insert(data_path="rotation_euler", frame=frame)

        for obj in animated_objs:
            act = obj.animation_data.action
            track = obj.animation_data.nla_tracks.new()
            track.name = clip_name
            strip = track.strips.new(clip_name, 1, act)
            strip.name = clip_name
            obj.animation_data.action = None

    # Clip 1: "Idle" (relaxed breathing + subtle helmet scan)
    record_clip("Idle", [
        (1,  {"root_loc": (0, 0, 0.00), "torso_rot": (0.02, 0, 0), "head_rot": (0, 0,  0.00), "arm_l": (0.05, 0.08, 0), "arm_r": (0.05, -0.08, 0)}),
        (18, {"root_loc": (0, 0, 0.02), "torso_rot": (-0.02, 0, 0), "head_rot": (0, 0,  0.14), "arm_l": (0.09, 0.10, 0), "arm_r": (0.09, -0.10, 0)}),
        (36, {"root_loc": (0, 0, 0.00), "torso_rot": (0.02, 0, 0), "head_rot": (0, 0, -0.12), "arm_l": (0.05, 0.08, 0), "arm_r": (0.05, -0.08, 0)}),
        (48, {"root_loc": (0, 0, 0.00), "torso_rot": (0.02, 0, 0), "head_rot": (0, 0,  0.00), "arm_l": (0.05, 0.08, 0), "arm_r": (0.05, -0.08, 0)}),
    ])

    # Clip 2: "Walk" (smooth natural 24-frame stride)
    record_clip("Walk", [
        (1,  {"root_loc": (0, 0, 0.00), "torso_rot": (0.06, 0,  0.00), "leg_l": ( 0.00, 0, 0), "leg_r": ( 0.00, 0, 0), "arm_l": ( 0.00, 0.06, 0), "arm_r": ( 0.00, -0.06, 0)}),
        (7,  {"root_loc": (0, 0, 0.05), "torso_rot": (0.08, 0,  0.06), "leg_l": ( 0.52, 0, 0), "leg_r": (-0.48, 0, 0), "arm_l": (-0.42, 0.06, 0), "arm_r": ( 0.42, -0.06, 0)}),
        (13, {"root_loc": (0, 0, 0.00), "torso_rot": (0.06, 0,  0.00), "leg_l": ( 0.00, 0, 0), "leg_r": ( 0.00, 0, 0), "arm_l": ( 0.00, 0.06, 0), "arm_r": ( 0.00, -0.06, 0)}),
        (19, {"root_loc": (0, 0, 0.05), "torso_rot": (0.08, 0, -0.06), "leg_l": (-0.48, 0, 0), "leg_r": ( 0.52, 0, 0), "arm_l": ( 0.42, 0.06, 0), "arm_r": (-0.42, -0.06, 0)}),
        (24, {"root_loc": (0, 0, 0.00), "torso_rot": (0.06, 0,  0.00), "leg_l": ( 0.00, 0, 0), "leg_r": ( 0.00, 0, 0), "arm_l": ( 0.00, 0.06, 0), "arm_r": ( 0.00, -0.06, 0)}),
    ])

    # Clip 3: "Run" (athletic forward-leaning 16-frame sprint!)
    record_clip("Run", [
        (1,  {"root_loc": (0, 0, 0.02), "torso_rot": (0.24, 0,  0.00), "leg_l": ( 0.00, 0, 0), "leg_r": ( 0.00, 0, 0), "arm_l": ( 0.00, 0.12, 0), "arm_r": ( 0.00, -0.12, 0)}),
        (5,  {"root_loc": (0, 0, 0.11), "torso_rot": (0.28, 0,  0.12), "leg_l": ( 0.92, 0, 0), "leg_r": (-0.84, 0, 0), "arm_l": (-0.82, 0.12, 0), "arm_r": ( 0.88, -0.12, 0)}),
        (9,  {"root_loc": (0, 0, 0.02), "torso_rot": (0.24, 0,  0.00), "leg_l": ( 0.00, 0, 0), "leg_r": ( 0.00, 0, 0), "arm_l": ( 0.00, 0.12, 0), "arm_r": ( 0.00, -0.12, 0)}),
        (13, {"root_loc": (0, 0, 0.11), "torso_rot": (0.28, 0, -0.12), "leg_l": (-0.84, 0, 0), "leg_r": ( 0.92, 0, 0), "arm_l": ( 0.88, 0.12, 0), "arm_r": (-0.82, -0.12, 0)}),
        (16, {"root_loc": (0, 0, 0.02), "torso_rot": (0.24, 0,  0.00), "leg_l": ( 0.00, 0, 0), "leg_r": ( 0.00, 0, 0), "arm_l": ( 0.00, 0.12, 0), "arm_r": ( 0.00, -0.12, 0)}),
    ])

    # Clip 4: "Swim" (horizontal freestyle swimming stroke!)
    record_clip("Swim", [
        (1,  {"root_loc": (0, 0, 0.55), "root_rot": (1.18, 0, 0), "head_rot": (-0.75, 0, 0), "arm_l": ( 1.85, 0.45, 0), "arm_r": ( 0.35, -0.35, 0), "leg_l": ( 0.32, 0, 0), "leg_r": (-0.32, 0, 0)}),
        (13, {"root_loc": (0, 0, 0.62), "root_rot": (1.18, 0, 0), "head_rot": (-0.75, 0, 0), "arm_l": ( 0.35, 0.35, 0), "arm_r": ( 1.85, -0.45, 0), "leg_l": (-0.32, 0, 0), "leg_r": ( 0.32, 0, 0)}),
        (24, {"root_loc": (0, 0, 0.55), "root_rot": (1.18, 0, 0), "head_rot": (-0.75, 0, 0), "arm_l": ( 1.85, 0.45, 0), "arm_r": ( 0.35, -0.35, 0), "leg_l": ( 0.32, 0, 0), "leg_r": (-0.32, 0, 0)}),
    ])

    out_path = os.path.join(MODELS_DIR, "character.glb")
    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format="GLB",
        export_yup=True,
        export_animations=True,
        export_animation_mode="NLA_TRACKS",
        export_apply=False,
    )
    print(f"[SAGE][Blender] Exported 4-clip Hero {out_path}")


# =============================================================================
# 2. DRIVABLE LUXURY RACING SPEEDBOAT (speedboat.glb — spinning propellers!)
# =============================================================================
def build_speedboat():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 12
    bpy.context.scene.render.fps = 24

    mat_white = make_mat("BoatWhite", (0.96, 0.97, 0.99), roughness=0.16, metallic=0.15)
    mat_red = make_mat("BoatRacingRed", (0.92, 0.14, 0.18), roughness=0.20, metallic=0.45)
    mat_dark = make_mat("BoatCarbon", (0.10, 0.12, 0.16), roughness=0.32, metallic=0.55)
    mat_glass = make_mat("BoatWindshield", (0.06, 0.16, 0.24), roughness=0.06, metallic=0.65)
    mat_chrome = make_mat("BoatChrome", (0.86, 0.88, 0.92), roughness=0.14, metallic=0.95)
    mat_cyan = make_mat("BoatNeonCyan", (0.12, 0.90, 1.0), roughness=0.10, emission=(0.12, 0.90, 1.0), emission_strength=3.4)
    mat_seat = make_mat("BoatLeather", (0.84, 0.78, 0.68), roughness=0.55)

    # Sculpted V-Hull pointing along Blender -Y (= SAGE +Z)
    add_box("HullMain",   (0.0,  0.2, 0.35), (2.25, 4.8, 0.82), mat_white, bevel=0.14)
    add_box("HullBow",    (0.0, -2.5, 0.44), (1.55, 2.2, 0.72), mat_white, bevel=0.16)
    add_box("HullNose",   (0.0, -3.7, 0.56), (0.65, 1.2, 0.52), mat_red,   bevel=0.10)
    add_box("RacingStripe",(0.0,-0.4, 0.78), (0.75, 5.4, 0.08), mat_red)
    add_box("WaterNeon",  (0.0,  0.0, 0.42), (2.32, 5.1, 0.10), mat_cyan)
    add_box("CockpitWell",(0.0,  0.5, 0.65), (1.68, 2.5, 0.35), mat_dark)
    add_box("SeatL",      (-0.46, 0.6, 0.88), (0.56, 0.65, 0.52), mat_seat, bevel=0.06)
    add_box("SeatR",      ( 0.46, 0.6, 0.88), (0.56, 0.65, 0.52), mat_seat, bevel=0.06)
    add_box("RearLounge", (0.0,  1.55, 0.82), (1.62, 0.75, 0.36), mat_seat, bevel=0.06)
    add_box("Windshield", (0.0, -0.55, 1.04), (1.88, 0.78, 0.46), mat_glass, rot=(math.radians(-38.0), 0.0, 0.0))
    add_box("RadarRoll",  (0.0,  1.25, 1.48), (1.95, 0.24, 0.14), mat_chrome)
    add_box("RollPostL",  (-0.92, 1.25, 1.12), (0.14, 0.22, 0.75), mat_chrome)
    add_box("RollPostR",  ( 0.92, 1.25, 1.12), (0.14, 0.22, 0.75), mat_chrome)

    # Twin Outboard Racing Motors + Animated Spinning Propellers!
    for idx, mx in enumerate((-0.52, 0.52), 1):
        add_box(f"MotorHead_{idx}", (mx, 2.85, 0.76), (0.38, 0.55, 0.68), mat_dark, bevel=0.05)
        add_box(f"MotorTrim_{idx}", (mx, 2.85, 0.96), (0.40, 0.45, 0.12), mat_red)
        add_box(f"MotorLeg_{idx}",  (mx, 2.88, 0.25), (0.14, 0.26, 0.65), mat_dark)
        prop = bpy.data.objects.new(f"Prop_{idx}", None)
        prop.location = (mx, 3.08, 0.08)
        prop.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(prop)
        add_box(f"BladeA_{idx}", (0, 0, 0), (0.42, 0.04, 0.08), mat_chrome, parent=prop)
        add_box(f"BladeB_{idx}", (0, 0, 0), (0.08, 0.04, 0.42), mat_chrome, parent=prop)
        for f, deg in [(1, 0), (4, 90), (7, 180), (10, 270), (12, 360)]:
            bpy.context.scene.frame_set(f)
            prop.rotation_euler = (0.0, math.radians(deg), 0.0)
            prop.keyframe_insert(data_path="rotation_euler", frame=f)

    export_glb("speedboat.glb", has_anim=True)


# =============================================================================
# 3. FUTURISTIC CYBERPUNK SUPERBIKE (superbike.glb — Player-Drivable!)
# =============================================================================
def build_superbike():
    clear_scene()
    mat_body = make_mat("BikeBody", (0.95, 0.24, 0.14), roughness=0.18, metallic=0.65)
    mat_dark = make_mat("BikeFrame", (0.10, 0.11, 0.14), roughness=0.35, metallic=0.60)
    mat_tire = make_mat("BikeTire", (0.06, 0.06, 0.07), roughness=0.82)
    mat_neon = make_mat("BikeNeon", (0.10, 0.92, 1.0), roughness=0.10, emission=(0.10, 0.92, 1.0), emission_strength=3.8)
    mat_head = make_mat("BikeHead", (1.0, 0.96, 0.86), roughness=0.1, emission=(1.0, 0.96, 0.86), emission_strength=4.2)
    mat_tail = make_mat("BikeTail", (1.0, 0.12, 0.10), roughness=0.1, emission=(1.0, 0.12, 0.10), emission_strength=3.6)
    mat_glass = make_mat("BikeScreen", (0.06, 0.12, 0.18), roughness=0.06, metallic=0.55)

    # Thick Front & Rear Sport Tires + Glowing Cyber Wheel Rims (faces -Y = SAGE +Z)
    for wy in (-0.95, 0.95):
        add_cyl(f"Tire_{wy}", (0.0, wy, 0.36), 0.36, 0.26, mat_tire, verts=24, rot=(0.0, math.radians(90), 0.0))
        bpy.ops.mesh.primitive_torus_add(
            major_segments=24, minor_segments=10,
            major_radius=0.24, minor_radius=0.032,
            location=(0.0, wy, 0.36), rotation=(0.0, math.radians(90), 0.0)
        )
        rim = bpy.context.active_object
        assign_mat(rim, mat_neon)
        smooth_shade(rim)

    # Sculpted Chassis, Fuel Tank, Aero Nose & Tail
    add_box("EngineBlock", (0.0,  0.00, 0.44), (0.44, 1.15, 0.46), mat_dark, bevel=0.04)
    add_box("FairingMain", (0.0, -0.35, 0.72), (0.56, 1.18, 0.46), mat_body, rot=(math.radians(-14), 0, 0), bevel=0.08)
    add_box("AeroNose",    (0.0, -1.02, 0.76), (0.46, 0.56, 0.34), mat_body, rot=(math.radians(-28), 0, 0), bevel=0.06)
    add_box("Windscreen",  (0.0, -0.74, 0.98), (0.38, 0.52, 0.16), mat_glass, rot=(math.radians(-38), 0, 0))
    add_box("Seat",        (0.0,  0.34, 0.80), (0.36, 0.76, 0.14), mat_dark, bevel=0.03)
    add_box("TailCowling", (0.0,  0.94, 0.86), (0.38, 0.62, 0.22), mat_body, rot=(math.radians(14), 0, 0), bevel=0.05)
    add_box("Handlebar",   (0.0, -0.56, 0.94), (0.78, 0.08, 0.08), mat_dark)
    add_cyl("ForkL", (-0.16, -0.78, 0.62), 0.035, 0.66, mat_dark, verts=12, rot=(math.radians(24), 0, 0))
    add_cyl("ForkR", ( 0.16, -0.78, 0.62), 0.035, 0.66, mat_dark, verts=12, rot=(math.radians(24), 0, 0))
    add_cyl("ExhaustL", (-0.28, 0.72, 0.42), 0.055, 0.72, mat_dark, verts=12, rot=(math.radians(82), 0, 0))
    add_cyl("ExhaustR", ( 0.28, 0.72, 0.42), 0.055, 0.72, mat_dark, verts=12, rot=(math.radians(82), 0, 0))

    # Glowing Headlight, Taillight & Side Accent Strips
    add_box("Headlight", (0.0, -1.28, 0.74), (0.34, 0.08, 0.14), mat_head)
    add_box("Taillight", (0.0,  1.24, 0.88), (0.32, 0.08, 0.10), mat_tail)
    add_box("SideGlowL", (-0.30, -0.25, 0.68), (0.03, 0.85, 0.05), mat_neon)
    add_box("SideGlowR", ( 0.30, -0.25, 0.68), (0.03, 0.85, 0.05), mat_neon)

    export_glb("superbike.glb", has_anim=False)


# =============================================================================
# 4. ANIMATED MARBLE PLAZA FOUNTAIN (fountain.glb — pulsing water jets!)
# =============================================================================
def build_fountain():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 48
    bpy.context.scene.render.fps = 24

    mat_marble = make_mat("FountainMarble", (0.90, 0.89, 0.86), roughness=0.38)
    mat_water = make_mat("FountainWater", (0.14, 0.78, 0.96), roughness=0.08, metallic=0.2, emission=(0.10, 0.68, 0.92), emission_strength=1.4)
    mat_foam = make_mat("FountainSpray", (0.86, 0.96, 1.0), roughness=0.20, emission=(0.72, 0.90, 1.0), emission_strength=2.2)

    # Outer Marble Basin & 2 Tiered Bowls
    bpy.ops.mesh.primitive_torus_add(major_segments=28, minor_segments=12, major_radius=4.2, minor_radius=0.48, location=(0, 0, 0.48))
    rim = bpy.context.active_object
    assign_mat(rim, mat_marble); smooth_shade(rim)
    add_cyl("BasinFloor", (0, 0, 0.25), 4.2, 0.50, mat_marble, verts=28)
    add_cyl("BasinPool",  (0, 0, 0.62), 3.9, 0.14, mat_water,  verts=28)
    add_cyl("Pedestal1",  (0, 0, 1.45), 0.75, 1.9, mat_marble, verts=18)
    add_cyl("Bowl1",      (0, 0, 2.35), 2.15, 0.38, mat_marble, verts=24)
    add_cyl("Pedestal2",  (0, 0, 3.15), 0.42, 1.5, mat_marble, verts=16)
    add_cyl("Bowl2",      (0, 0, 3.85), 1.15, 0.30, mat_marble, verts=20)

    # Animated Pulsing Center Water Jet & Sparkling Plume!
    jet_root = bpy.data.objects.new("WaterJets", None)
    jet_root.location = (0, 0, 3.95)
    bpy.context.collection.objects.link(jet_root)
    add_cyl("MainJet", (0, 0, 1.15), 0.22, 2.3, mat_foam, verts=12, parent=jet_root)
    add_sphere("JetCrown", (0, 0, 2.35), 0.58, mat_foam, scale=(1.2, 1.2, 0.65), parent=jet_root)
    for i in range(6):
        ang = (2.0 * math.pi * i) / 6.0
        add_cyl(f"SideJet_{i}", (math.cos(ang) * 0.9, math.sin(ang) * 0.9, -0.4), 0.10, 1.8, mat_water, verts=10,
                rot=(math.sin(ang) * 0.35, -math.cos(ang) * 0.35, 0), parent=jet_root)

    for f, sz in [(1, 0.85), (13, 1.15), (25, 0.92), (37, 1.18), (48, 0.85)]:
        bpy.context.scene.frame_set(f)
        jet_root.scale = (1.0, 1.0, sz)
        jet_root.keyframe_insert(data_path="scale", frame=f)

    export_glb("fountain.glb", has_anim=True)


# =============================================================================
# 5. ANIMATED COASTAL SEAGULL (seagull.glb — graceful wing flapping!)
# =============================================================================
def build_seagull():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 24
    bpy.context.scene.render.fps = 24

    mat_white = make_mat("GullWhite", (0.96, 0.97, 0.99), roughness=0.55)
    mat_wing = make_mat("GullWingTip", (0.28, 0.32, 0.38), roughness=0.55)
    mat_beak = make_mat("GullBeak", (1.0, 0.72, 0.14), roughness=0.40)

    root = bpy.data.objects.new("GullRoot", None)
    bpy.context.collection.objects.link(root)

    add_sphere("Body", (0.0,  0.0, 0.0), 0.22, mat_white, scale=(0.65, 1.95, 0.65), parent=root)
    add_sphere("Head", (0.0, -0.46, 0.08), 0.13, mat_white, parent=root)
    add_box("Beak",    (0.0, -0.64, 0.06), (0.06, 0.20, 0.06), mat_beak, parent=root)
    add_box("Tail",    (0.0,  0.46, 0.04), (0.28, 0.28, 0.04), mat_white, parent=root)

    wing_l = bpy.data.objects.new("WingL", None)
    wing_l.location = (-0.12, -0.05, 0.06)
    wing_l.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(wing_l)
    wing_l.parent = root
    add_box("WingL_Inner", (-0.42, 0.0, 0.0), (0.82, 0.34, 0.04), mat_white, parent=wing_l)
    add_box("WingL_Tip",   (-0.98, 0.06, 0.0), (0.36, 0.24, 0.03), mat_wing, parent=wing_l)

    wing_r = bpy.data.objects.new("WingR", None)
    wing_r.location = (0.12, -0.05, 0.06)
    wing_r.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(wing_r)
    wing_r.parent = root
    add_box("WingR_Inner", ( 0.42, 0.0, 0.0), (0.82, 0.34, 0.04), mat_white, parent=wing_r)
    add_box("WingR_Tip",   ( 0.98, 0.06, 0.0), (0.36, 0.24, 0.03), mat_wing, parent=wing_r)

    for f, ang, bz in [(1, 0.48, 0.0), (7, -0.42, 0.12), (13, 0.48, 0.0), (19, -0.38, 0.10), (24, 0.48, 0.0)]:
        bpy.context.scene.frame_set(f)
        root.location = (0.0, 0.0, bz)
        root.keyframe_insert(data_path="location", frame=f)
        wing_l.rotation_euler = (0.0,  ang, 0.0); wing_l.keyframe_insert(data_path="rotation_euler", frame=f)
        wing_r.rotation_euler = (0.0, -ang, 0.0); wing_r.keyframe_insert(data_path="rotation_euler", frame=f)

    export_glb("seagull.glb", has_anim=True)


# =============================================================================
# 6. SUPERSONIC COASTAL JET (jet_plane.glb — glowing twin afterburners!)
# =============================================================================
def build_jet_plane():
    clear_scene()
    mat_fuselage = make_mat("JetTitanium", (0.76, 0.80, 0.86), roughness=0.22, metallic=0.78)
    mat_accent = make_mat("JetStripe", (0.12, 0.42, 0.88), roughness=0.28, metallic=0.45)
    mat_canopy = make_mat("JetCanopy", (0.08, 0.20, 0.30), roughness=0.06, metallic=0.75)
    mat_burner = make_mat("JetAfterburner", (0.20, 0.86, 1.0), roughness=0.1, emission=(0.20, 0.86, 1.0), emission_strength=4.8)

    # Swept-wing supersonic fuselage (faces -Y = SAGE +Z)
    add_cyl("Fuselage", (0.0,  0.0, 0.0), 0.72, 11.5, mat_fuselage, verts=18, rot=(math.radians(90), 0, 0))
    add_sphere("NoseCone", (0.0, -6.4, 0.0), 0.70, mat_accent, scale=(0.95, 2.4, 0.95))
    add_sphere("Canopy",   (0.0, -2.6, 0.62), 0.52, mat_canopy, scale=(0.85, 2.6, 0.75))
    # Delta Wings & Twin Tail Fins
    add_box("WingL", (-3.2, 1.2, -0.05), (5.8, 3.2, 0.14), mat_fuselage, rot=(0, 0, math.radians(-26)))
    add_box("WingR", ( 3.2, 1.2, -0.05), (5.8, 3.2, 0.14), mat_fuselage, rot=(0, 0, math.radians( 26)))
    add_box("TailL", (-1.1, 4.4,  0.95), (0.14, 2.0, 1.85), mat_accent, rot=(0, math.radians(-18), 0))
    add_box("TailR", ( 1.1, 4.4,  0.95), (0.14, 2.0, 1.85), mat_accent, rot=(0, math.radians( 18), 0))
    # Glowing Twin Afterburner Nozzles
    add_cyl("BurnerL", (-0.42, 5.95, 0.0), 0.30, 0.75, mat_burner, verts=14, rot=(math.radians(90), 0, 0))
    add_cyl("BurnerR", ( 0.42, 5.95, 0.0), 0.30, 0.75, mat_burner, verts=14, rot=(math.radians(90), 0, 0))

    export_glb("jet_plane.glb", has_anim=False)


if __name__ == "__main__":
    build_hero_character()
    build_speedboat()
    build_superbike()
    build_fountain()
    build_seagull()
    build_jet_plane()
    print("[SAGE][Blender] All 6 v1.1.0 high-detail & animated assets built successfully!")
