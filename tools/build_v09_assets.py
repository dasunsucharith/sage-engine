"""
SAGE Engine v0.9 — Blender 5.3 Procedural 3D Asset Builder
Generates 6 brand-new game-ready glTF 2.0 (.glb) assets (24 total in SAGE!):
  1. assets/models/character.glb     (Playable 3rd-person driver avatar with walking animation!)
  2. assets/models/ferris_wheel.glb  (24m glowing Coastal Ferris Wheel with spinning animation!)
  3. assets/models/ai_sedan.glb      (Sleek AI Traffic Sedan with glowing headlights & taillights!)
  4. assets/models/helicopter.glb    (Patrol Helicopter with spinning main & tail rotors!)
  5. assets/models/cactus.glb        (Desert Saguaro Cactus for Red Rock Canyon)
  6. assets/models/alpine_cabin.glb  (Warm-lit Pine Highlands A-Frame Mountain Lodge)

Run with:
  "D:/Program Files/Blender Foundation/Blender 5.3/blender.exe" --background --python tools/build_v09_assets.py
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


def add_sphere(name, loc, radius, mat, scale=(1.0, 1.0, 1.0), parent=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=12, radius=radius, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
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
# 1. PLAYABLE 3RD-PERSON DRIVER CHARACTER (character.glb — animated walk!)
# =============================================================================
def build_character():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 24
    bpy.context.scene.render.fps = 24

    mat_jacket = make_mat("RacingJacket", (0.12, 0.56, 0.92), roughness=0.45, metallic=0.1)
    mat_stripe = make_mat("JacketAccent", (0.98, 0.80, 0.16), roughness=0.35, emission=(0.95, 0.72, 0.12), emission_strength=1.2)
    mat_pants = make_mat("DarkPants", (0.14, 0.16, 0.21), roughness=0.75)
    mat_boots = make_mat("Boots", (0.08, 0.08, 0.10), roughness=0.55)
    mat_helmet = make_mat("HelmetWhite", (0.94, 0.95, 0.98), roughness=0.22, metallic=0.15)
    mat_visor = make_mat("HelmetVisor", (0.06, 0.85, 0.98), roughness=0.10, metallic=0.6, emission=(0.08, 0.75, 0.95), emission_strength=1.8)

    root = bpy.data.objects.new("CharRoot", None)
    bpy.context.collection.objects.link(root)

    # Torso & Helmet (faces Blender -Y = SAGE +Z)
    torso = add_box("Torso", (0.0, 0.0, 1.15), (0.50, 0.28, 0.58), mat_jacket, parent=root)
    add_box("ChestStripe", (0.0, -0.15, 1.22), (0.52, 0.04, 0.10), mat_stripe, parent=root)
    add_box("Backpack", (0.0, 0.18, 1.18), (0.34, 0.14, 0.42), mat_pants, parent=root)
    add_sphere("Helmet", (0.0, 0.0, 1.66), 0.21, mat_helmet, parent=root)
    add_box("Visor", (0.0, -0.17, 1.67), (0.28, 0.10, 0.12), mat_visor, parent=root)

    # Pivoted Limbs for smooth looping walk cycle!
    def make_limb(name, pivot_loc, offset_z, scale, mat, shoe=False):
        piv = bpy.data.objects.new(name + "_Pivot", None)
        piv.location = pivot_loc
        piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(piv)
        piv.parent = root
        add_box(name, (0.0, 0.0, offset_z), scale, mat, parent=piv)
        if shoe:
            add_box(name + "_Boot", (0.0, -0.05, offset_z - scale[2] * 0.48), (scale[0] * 1.1, scale[1] * 1.65, 0.14), mat_boots, parent=piv)
        return piv

    leg_l = make_limb("LegL", (-0.13, 0.0, 0.84), -0.38, (0.16, 0.18, 0.72), mat_pants, shoe=True)
    leg_r = make_limb("LegR", ( 0.13, 0.0, 0.84), -0.38, (0.16, 0.18, 0.72), mat_pants, shoe=True)
    arm_l = make_limb("ArmL", (-0.33, 0.0, 1.38), -0.26, (0.13, 0.14, 0.52), mat_jacket)
    arm_r = make_limb("ArmR", ( 0.33, 0.0, 1.38), -0.26, (0.13, 0.14, 0.52), mat_jacket)

    # Smooth 24-frame walking cycle
    swing = math.radians(32.0)
    arm_sw = math.radians(26.0)
    keys = [
        (1,   0.0,     0.0,   0.00),
        (7,   swing,  -arm_sw, 0.05),
        (13,  0.0,     0.0,   0.00),
        (19, -swing,   arm_sw, 0.05),
        (24,  0.0,     0.0,   0.00),
    ]
    for f, leg_a, arm_a, bob in keys:
        bpy.context.scene.frame_set(f)
        root.location = (0.0, 0.0, bob)
        root.keyframe_insert(data_path="location", frame=f)
        leg_l.rotation_euler = ( leg_a, 0.0, 0.0); leg_l.keyframe_insert(data_path="rotation_euler", frame=f)
        leg_r.rotation_euler = (-leg_a, 0.0, 0.0); leg_r.keyframe_insert(data_path="rotation_euler", frame=f)
        arm_l.rotation_euler = ( arm_a, 0.0, 0.0); arm_l.keyframe_insert(data_path="rotation_euler", frame=f)
        arm_r.rotation_euler = (-arm_a, 0.0, 0.0); arm_r.keyframe_insert(data_path="rotation_euler", frame=f)

    export_glb("character.glb", has_anim=True)


# =============================================================================
# 2. 24M GLOWING COASTAL FERRIS WHEEL (ferris_wheel.glb — animated rotation!)
# =============================================================================
def build_ferris_wheel():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 180
    bpy.context.scene.render.fps = 24

    mat_steel = make_mat("WheelSteel", (0.82, 0.85, 0.90), roughness=0.32, metallic=0.75)
    mat_base = make_mat("WheelBase", (0.22, 0.25, 0.30), roughness=0.75)
    mat_neon_c = make_mat("WheelCyan", (0.12, 0.92, 1.0), roughness=0.15, emission=(0.12, 0.92, 1.0), emission_strength=3.2)
    mat_neon_p = make_mat("WheelPink", (1.0, 0.25, 0.68), roughness=0.15, emission=(1.0, 0.25, 0.68), emission_strength=3.2)
    gondola_mats = [
        make_mat("GonRed",    (0.95, 0.22, 0.24), roughness=0.3, emission=(0.95, 0.22, 0.24), emission_strength=0.9),
        make_mat("GonGold",   (1.00, 0.78, 0.16), roughness=0.3, emission=(1.00, 0.78, 0.16), emission_strength=0.9),
        make_mat("GonCyan",   (0.14, 0.86, 0.98), roughness=0.3, emission=(0.14, 0.86, 0.98), emission_strength=0.9),
        make_mat("GonGreen",  (0.18, 0.92, 0.48), roughness=0.3, emission=(0.18, 0.92, 0.48), emission_strength=0.9),
    ]

    # Concrete Base & Tall A-Frame Support Legs
    hub_z = 13.5
    add_box("BasePad", (0.0, 0.0, 0.35), (14.0, 6.0, 0.70), mat_base)
    for side_y in (-2.0, 2.0):
        add_box(f"LegL_{side_y}", (-3.2, side_y, hub_z * 0.5), (0.65, 0.55, hub_z + 1.0), mat_steel, rot=(0.0, math.radians(24.0), 0.0))
        add_box(f"LegR_{side_y}", ( 3.2, side_y, hub_z * 0.5), (0.65, 0.55, hub_z + 1.0), mat_steel, rot=(0.0, math.radians(-24.0), 0.0))
    add_cyl("Axle", (0.0, 0.0, hub_z), 0.55, 5.2, mat_steel, verts=16, rot=(math.radians(90), 0.0, 0.0))

    # Spinning Wheel Hub
    rotor = bpy.data.objects.new("WheelRotor", None)
    rotor.location = (0.0, 0.0, hub_z)
    rotor.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(rotor)

    radius = 10.5
    # Dual Glowing Rim Toruses
    for ry, rmat in [(-1.1, mat_neon_c), (1.1, mat_neon_p)]:
        bpy.ops.mesh.primitive_torus_add(
            major_segments=32, minor_segments=10,
            major_radius=radius, minor_radius=0.22,
            location=(0.0, ry, 0.0), rotation=(math.radians(90), 0.0, 0.0)
        )
        rim = bpy.context.active_object
        assign_mat(rim, rmat)
        rim.parent = rotor

    # 8 Spokes & Hanging Colored Gondola Cabins
    num_gondolas = 8
    for i in range(num_gondolas):
        ang = (2.0 * math.pi * i) / num_gondolas
        gx = math.cos(ang) * radius
        gz = math.sin(ang) * radius
        # Spoke
        add_box(f"SpokeA_{i}", (gx * 0.5, -1.1, gz * 0.5), (radius, 0.18, 0.22), mat_steel, rot=(0.0, -ang, 0.0), parent=rotor)
        add_box(f"SpokeB_{i}", (gx * 0.5,  1.1, gz * 0.5), (radius, 0.18, 0.22), mat_steel, rot=(0.0, -ang, 0.0), parent=rotor)
        # Gondola Pod
        gmat = gondola_mats[i % len(gondola_mats)]
        add_box(f"Gondola_{i}", (gx, 0.0, gz), (1.65, 2.0, 1.65), gmat, parent=rotor)

    # Continuous 360-degree rotation around Y axis!
    for f, deg in [(1, 0.0), (46, 90.0), (91, 180.0), (136, 270.0), (180, 360.0)]:
        bpy.context.scene.frame_set(f)
        rotor.rotation_euler = (0.0, math.radians(deg), 0.0)
        rotor.keyframe_insert(data_path="rotation_euler", frame=f)

    if rotor.animation_data and rotor.animation_data.action:
        for fcurve in rotor.animation_data.action.fcurves if hasattr(rotor.animation_data.action, "fcurves") else []:
            for kp in fcurve.keyframe_points:
                kp.interpolation = "LINEAR"

    export_glb("ferris_wheel.glb", has_anim=True)


# =============================================================================
# 3. SLEEK AI TRAFFIC SEDAN (ai_sedan.glb)
# =============================================================================
def build_ai_sedan():
    clear_scene()
    mat_body = make_mat("SedanPaint", (0.95, 0.95, 0.96), roughness=0.22, metallic=0.65)
    mat_glass = make_mat("SedanGlass", (0.07, 0.10, 0.15), roughness=0.10, metallic=0.35)
    mat_tire = make_mat("SedanTire", (0.07, 0.07, 0.08), roughness=0.85)
    mat_rim = make_mat("SedanRim", (0.78, 0.80, 0.84), roughness=0.25, metallic=0.90)
    mat_head = make_mat("SedanHeadlight", (1.0, 0.95, 0.82), roughness=0.1, emission=(1.0, 0.95, 0.82), emission_strength=3.6)
    mat_tail = make_mat("SedanTaillight", (1.0, 0.12, 0.10), roughness=0.1, emission=(1.0, 0.12, 0.10), emission_strength=3.2)

    # Lower Chassis + Sculpted Cabin (faces Blender -Y = SAGE +Z)
    add_box("Chassis", (0.0,  0.0, 0.56), (1.88, 4.35, 0.62), mat_body)
    add_box("Hood",    (0.0, -1.5, 0.64), (1.76, 1.30, 0.42), mat_body)
    add_box("Cabin",   (0.0,  0.18, 1.12), (1.62, 2.05, 0.58), mat_body)
    add_box("Windsh",  (0.0, -0.78, 1.10), (1.54, 0.40, 0.46), mat_glass, rot=(math.radians(-26.0), 0.0, 0.0))
    add_box("RearWin", (0.0,  1.12, 1.10), (1.52, 0.36, 0.44), mat_glass, rot=(math.radians(24.0), 0.0, 0.0))
    add_box("SideWin", (0.0,  0.18, 1.13), (1.66, 1.72, 0.38), mat_glass)

    # Glowing Headlights (-Y front) & Taillight Bar (+Y rear)
    add_box("HeadL", (-0.64, -2.18, 0.62), (0.42, 0.08, 0.16), mat_head)
    add_box("HeadR", ( 0.64, -2.18, 0.62), (0.42, 0.08, 0.16), mat_head)
    add_box("TailBar", (0.0,  2.18, 0.66), (1.66, 0.08, 0.14), mat_tail)

    # 4 Wheels with metallic rims
    for wx in (-0.94, 0.94):
        for wy in (-1.35, 1.35):
            add_cyl(f"Tire_{wx}_{wy}", (wx, wy, 0.34), 0.34, 0.26, mat_tire, verts=18, rot=(0.0, math.radians(90), 0.0))
            add_cyl(f"Rim_{wx}_{wy}",  (wx * 1.03, wy, 0.34), 0.20, 0.24, mat_rim, verts=12, rot=(0.0, math.radians(90), 0.0))

    export_glb("ai_sedan.glb", has_anim=False)


# =============================================================================
# 4. PATROL HELICOPTER (helicopter.glb — spinning main & tail rotors!)
# =============================================================================
def build_helicopter():
    clear_scene()
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 16
    bpy.context.scene.render.fps = 24

    mat_body = make_mat("HeliNavy", (0.10, 0.18, 0.32), roughness=0.30, metallic=0.45)
    mat_accent = make_mat("HeliOrange", (1.0, 0.45, 0.12), roughness=0.35)
    mat_glass = make_mat("HeliCockpit", (0.10, 0.78, 0.95), roughness=0.10, metallic=0.4, emission=(0.08, 0.55, 0.75), emission_strength=0.9)
    mat_blade = make_mat("HeliBlade", (0.14, 0.15, 0.18), roughness=0.45, metallic=0.6)
    mat_lamp = make_mat("HeliSearch", (1.0, 0.92, 0.65), roughness=0.1, emission=(1.0, 0.92, 0.65), emission_strength=4.0)

    # Fuselage + Nose Cockpit + Tail Boom + Skids (faces -Y = SAGE +Z)
    add_box("Fuselage", (0.0,  0.0, 1.45), (1.75, 3.80, 1.55), mat_body)
    add_box("AccentStripe", (0.0, 0.0, 1.25), (1.80, 3.84, 0.26), mat_accent)
    add_sphere("Cockpit", (0.0, -1.75, 1.45), 0.85, mat_glass, scale=(0.95, 1.15, 0.85))
    add_box("TailBoom", (0.0,  3.55, 1.65), (0.42, 3.80, 0.48), mat_body)
    add_box("TailFin",  (0.0,  5.30, 2.05), (0.14, 0.75, 1.35), mat_accent)
    add_box("SearchLamp", (0.0, -2.35, 0.85), (0.34, 0.24, 0.24), mat_lamp)

    for sx in (-0.95, 0.95):
        add_cyl(f"Skid_{sx}", (sx, -0.1, 0.18), 0.07, 3.8, mat_blade, verts=10, rot=(math.radians(90), 0.0, 0.0))
        add_box(f"StrutF_{sx}", (sx * 0.8, -0.9, 0.55), (0.10, 0.10, 0.75), mat_blade)
        add_box(f"StrutB_{sx}", (sx * 0.8,  0.9, 0.55), (0.10, 0.10, 0.75), mat_blade)

    # Main Rotor Hub & 4 Blades
    add_cyl("RotorShaft", (0.0, 0.0, 2.45), 0.12, 0.65, mat_blade, verts=10)
    main_rotor = bpy.data.objects.new("MainRotor", None)
    main_rotor.location = (0.0, 0.0, 2.75)
    main_rotor.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(main_rotor)
    add_box("Blade1", (0.0, 0.0, 0.0), (8.6, 0.28, 0.06), mat_blade, parent=main_rotor)
    add_box("Blade2", (0.0, 0.0, 0.0), (0.28, 8.6, 0.06), mat_blade, parent=main_rotor)

    # Tail Rotor
    tail_rotor = bpy.data.objects.new("TailRotor", None)
    tail_rotor.location = (0.28, 5.25, 2.15)
    tail_rotor.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(tail_rotor)
    add_box("TailBlade1", (0.0, 0.0, 0.0), (0.05, 1.45, 0.14), mat_accent, parent=tail_rotor)
    add_box("TailBlade2", (0.0, 0.0, 0.0), (0.05, 0.14, 1.45), mat_accent, parent=tail_rotor)

    for f, deg in [(1, 0.0), (5, 90.0), (9, 180.0), (13, 270.0), (16, 360.0)]:
        bpy.context.scene.frame_set(f)
        main_rotor.rotation_euler = (0.0, 0.0, math.radians(deg))
        main_rotor.keyframe_insert(data_path="rotation_euler", frame=f)
        tail_rotor.rotation_euler = (math.radians(deg * 2.0), 0.0, 0.0)
        tail_rotor.keyframe_insert(data_path="rotation_euler", frame=f)

    export_glb("helicopter.glb", has_anim=True)


# =============================================================================
# 5. DESERT SAGUARO CACTUS (cactus.glb)
# =============================================================================
def build_cactus():
    clear_scene()
    mat_green = make_mat("CactusGreen", (0.18, 0.46, 0.22), roughness=0.80)
    mat_flower = make_mat("CactusBloom", (1.0, 0.85, 0.24), roughness=0.45, emission=(0.9, 0.72, 0.15), emission_strength=0.6)

    # Tall ribbed main trunk
    add_cyl("Trunk", (0.0, 0.0, 2.3), 0.34, 4.6, mat_green, verts=12)
    add_sphere("TrunkCap", (0.0, 0.0, 4.6), 0.34, mat_green)
    add_sphere("BloomTop", (0.0, 0.0, 4.95), 0.14, mat_flower)

    # Left Arm (horizontal elbow + vertical rise)
    add_cyl("ArmL_H", (-0.65, 0.0, 2.35), 0.24, 1.1, mat_green, verts=10, rot=(0.0, math.radians(90), 0.0))
    add_cyl("ArmL_V", (-1.15, 0.0, 3.10), 0.24, 1.5, mat_green, verts=10)
    add_sphere("ArmL_Cap", (-1.15, 0.0, 3.85), 0.24, mat_green)

    # Right Arm (slightly higher)
    add_cyl("ArmR_H", (0.60, 0.0, 2.85), 0.22, 1.0, mat_green, verts=10, rot=(0.0, math.radians(90), 0.0))
    add_cyl("ArmR_V", (1.05, 0.0, 3.45), 0.22, 1.25, mat_green, verts=10)
    add_sphere("ArmR_Cap", (1.05, 0.0, 4.08), 0.22, mat_green)

    export_glb("cactus.glb", has_anim=False)


# =============================================================================
# 6. PINE HIGHLANDS A-FRAME MOUNTAIN LODGE (alpine_cabin.glb)
# =============================================================================
def build_alpine_cabin():
    clear_scene()
    mat_log = make_mat("CabinTimber", (0.38, 0.22, 0.12), roughness=0.82)
    mat_roof = make_mat("AlpineRoof", (0.16, 0.22, 0.28), roughness=0.65)
    mat_stone = make_mat("StoneChimney", (0.44, 0.45, 0.48), roughness=0.90)
    mat_glow = make_mat("CabinWindow", (1.0, 0.76, 0.32), roughness=0.15, emission=(1.0, 0.74, 0.28), emission_strength=3.0)

    # Raised timber porch foundation
    add_box("PorchBase", (0.0, -0.6, 0.25), (8.6, 11.4, 0.50), mat_stone)
    # Main Timber Cabin Walls
    add_box("CabinBody", (0.0,  0.4, 2.10), (7.4, 8.6, 3.20), mat_log)
    # Steep A-Frame Slanted Roof Panels
    add_box("RoofL", (-2.25, 0.3, 4.65), (5.5, 9.8, 0.36), mat_roof, rot=(0.0, math.radians(-42.0), 0.0))
    add_box("RoofR", ( 2.25, 0.3, 4.65), (5.5, 9.8, 0.36), mat_roof, rot=(0.0, math.radians( 42.0), 0.0))
    # Stone Chimney
    add_box("Chimney", (2.4, 2.2, 4.2), (1.35, 1.35, 7.6), mat_stone)
    # Glowing Cozy Cabin Windows & Front Door Window
    add_box("WinFrontL", (-1.9, -3.92, 2.1), (1.6, 0.12, 1.4), mat_glow)
    add_box("WinFrontR", ( 1.9, -3.92, 2.1), (1.6, 0.12, 1.4), mat_glow)
    add_box("WinLoft",   ( 0.0, -3.92, 4.3), (2.0, 0.12, 1.5), mat_glow)
    add_box("WinSideL",  (-3.72, 0.4,  2.1), (0.12, 3.8, 1.3), mat_glow)
    add_box("WinSideR",  ( 3.72, 0.4,  2.1), (0.12, 3.8, 1.3), mat_glow)

    export_glb("alpine_cabin.glb", has_anim=False)


if __name__ == "__main__":
    build_character()
    build_ferris_wheel()
    build_ai_sedan()
    build_helicopter()
    build_cactus()
    build_alpine_cabin()
    print("[SAGE][Blender] All 6 v0.9.0 assets built successfully!")
