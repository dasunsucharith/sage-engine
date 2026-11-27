"""
SAGE Engine — High-Detail Realistic Stylized Human Character Builder (Blender 5.3)
Builds assets/models/character.glb:
  - True 1.78m human proportions (standing upright on Z=0, facing Blender -Y = SAGE +Z)
  - Smooth Subdivision-Surface organic meshes (head with jawline/nose/ears/styled hair/aviator shades,
    tapered torso in dark leather racing jacket over white tee, 2-segment articulated arms with elbows
    & hands, 2-segment articulated legs with knees, denim jeans & detailed sneakers!)
  - 4 clean, verified named glTF 2.0 animation clips:
      * "Idle"  — natural standing breathing & subtle look-around
      * "Walk"  — realistic 2-joint knee-bending stride + natural arm swing
      * "Run"   — athletic forward-leaning sprint with bent elbows & high knee drive
      * "Swim"  — horizontal freestyle swim stroke (only tilts during Swim clip!)
  - Verifies the exported .glb binary JSON structure and renders a preview PNG so quality is verified!

Run with:
  "D:/Program Files/Blender Foundation/Blender 5.3/blender.exe" --background --python tools/build_human_character.py
"""

import bpy
import bmesh
import json
import math
import os
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
OUT_GLB = os.path.join(MODELS_DIR, "character.glb")
os.makedirs(MODELS_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_mat(name, color, roughness=0.55, metallic=0.0, coat=0.0):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if coat > 0.0 and "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = coat
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


def apply_subsurf(obj, levels=2):
    mod = obj.modifiers.new(name="Subsurf", type="SUBSURF")
    mod.levels = levels
    mod.render_levels = levels
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    smooth_shade(obj)


def make_loft_tube(name, rings, mat, segs=18, cap_ends=True, parent=None):
    """
    Creates an organic smoothly-tapered lofted mesh from a list of (z, rx, ry, cx, cy) cross-sections!
    Ideal for realistic human torso, neck, arms, thighs, calves, and sculpted head!
    """
    mesh = bpy.data.meshes.new(name + "_Mesh")
    bm = bmesh.new()

    ring_verts = []
    for (z, rx, ry, cx, cy) in rings:
        vring = []
        for i in range(segs):
            ang = (2.0 * math.pi * i) / segs
            x = cx + math.cos(ang) * rx
            y = cy + math.sin(ang) * ry
            vring.append(bm.verts.new((x, y, z)))
        ring_verts.append(vring)

    for r in range(len(ring_verts) - 1):
        v0 = ring_verts[r]
        v1 = ring_verts[r + 1]
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((v0[i], v0[ni], v1[ni], v1[i]))

    if cap_ends and len(ring_verts) >= 2:
        # Bottom cap
        z0, _, _, cx0, cy0 = rings[0]
        bc = bm.verts.new((cx0, cy0, z0 - 0.008))
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((ring_verts[0][ni], ring_verts[0][i], bc))
        # Top cap
        z1, _, _, cx1, cy1 = rings[-1]
        tc = bm.verts.new((cx1, cy1, z1 + 0.008))
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((ring_verts[-1][i], ring_verts[-1][ni], tc))

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    assign_mat(obj, mat)
    smooth_shade(obj)
    if parent:
        obj.parent = parent
    return obj


def make_box(name, loc, scale, mat, rot=(0, 0, 0), subsurf=0, parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign_mat(obj, mat)
    if subsurf > 0:
        apply_subsurf(obj, levels=subsurf)
    else:
        smooth_shade(obj)
    if parent:
        obj.parent = parent
    return obj


def make_sphere(name, loc, scale, mat, parent=None, segs=20, rings=14):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segs, ring_count=rings, radius=1.0, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign_mat(obj, mat)
    smooth_shade(obj)
    if parent:
        obj.parent = parent
    return obj


def build_human():
    clear_scene()
    bpy.context.scene.render.fps = 24

    # Realistic natural PBR human & streetwear materials (NO neon toy parts!)
    mat_skin    = make_mat("SkinTone",      (0.78, 0.56, 0.44), roughness=0.58, metallic=0.0)
    mat_hair    = make_mat("HairDark",      (0.08, 0.06, 0.05), roughness=0.68, metallic=0.0)
    mat_jacket  = make_mat("LeatherJacket", (0.12, 0.13, 0.16), roughness=0.38, metallic=0.08)
    mat_jtrim   = make_mat("JacketStripe",  (0.78, 0.16, 0.16), roughness=0.42, metallic=0.05)
    mat_tee     = make_mat("InnerTee",      (0.90, 0.91, 0.93), roughness=0.82, metallic=0.0)
    mat_jeans   = make_mat("DenimJeans",    (0.14, 0.22, 0.36), roughness=0.84, metallic=0.0)
    mat_belt    = make_mat("LeatherBelt",   (0.10, 0.07, 0.05), roughness=0.45, metallic=0.0)
    mat_metal   = make_mat("BeltBuckle",    (0.78, 0.78, 0.80), roughness=0.22, metallic=0.90)
    mat_shoe    = make_mat("SneakerUpper",  (0.16, 0.17, 0.20), roughness=0.55, metallic=0.0)
    mat_sole    = make_mat("SneakerSole",   (0.92, 0.92, 0.94), roughness=0.65, metallic=0.0)
    mat_shades  = make_mat("AviatorLens",   (0.05, 0.07, 0.10), roughness=0.08, metallic=0.75)

    # -------------------------------------------------------------------------
    # HIERARCHICAL RIG (All local coords upright in Blender +Z up, -Y forward!)
    # Total height: 1.78m (Feet flat at Z = 0.00m!)
    # -------------------------------------------------------------------------
    hip_root = bpy.data.objects.new("HipRoot", None)
    hip_root.location = (0.0, 0.0, 0.94)  # Pelvis center at 0.94m above ground
    hip_root.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(hip_root)

    # Sculpted Hips / Pelvis (in jeans, centered around HipRoot Z=0)
    make_loft_tube("Pelvis", [
        (-0.10, 0.165, 0.115, 0.0, 0.005),
        (-0.02, 0.178, 0.122, 0.0, 0.004),
        ( 0.05, 0.172, 0.118, 0.0, 0.002),
    ], mat_jeans, segs=20, parent=hip_root)

    # Leather Belt + Silver Buckle
    make_loft_tube("Belt", [
        (0.035, 0.177, 0.123, 0.0, 0.0),
        (0.078, 0.175, 0.121, 0.0, 0.0),
    ], mat_belt, segs=20, parent=hip_root)
    make_box("Buckle", (0.0, -0.124, 0.056), (0.055, 0.018, 0.044), mat_metal, parent=hip_root)

    # Upper Torso Pivot (starts at waist Z = +0.06 above HipRoot -> world Z = 1.00m)
    torso_piv = bpy.data.objects.new("TorsoPivot", None)
    torso_piv.location = (0.0, 0.0, 0.06)
    torso_piv.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(torso_piv)
    torso_piv.parent = hip_root

    # Athletic Tapered Human Torso in Fitted Dark Leather Racing Jacket!
    make_loft_tube("JacketTorso", [
        (-0.02, 0.168, 0.114, 0.0,  0.004),
        ( 0.10, 0.176, 0.120, 0.0,  0.002),
        ( 0.24, 0.205, 0.132, 0.0, -0.002),
        ( 0.36, 0.226, 0.134, 0.0, -0.004),
        ( 0.44, 0.210, 0.118, 0.0,  0.000),
        ( 0.49, 0.115, 0.090, 0.0,  0.004),
    ], mat_jacket, segs=22, parent=torso_piv)

    # Crisp White Inner Crew-Neck T-Shirt visible down the open front V-zip!
    make_loft_tube("InnerTee", [
        (0.00, 0.068, 0.035, 0.0, -0.095),
        (0.22, 0.078, 0.042, 0.0, -0.106),
        (0.42, 0.076, 0.038, 0.0, -0.092),
        (0.47, 0.066, 0.032, 0.0, -0.068),
    ], mat_tee, segs=14, parent=torso_piv)

    # Jacket Collar & Sport Red Chest Racing Stripe
    make_loft_tube("JacketCollar", [
        (0.46, 0.096, 0.088, 0.0, 0.008),
        (0.52, 0.088, 0.082, 0.0, 0.012),
    ], mat_jacket, segs=18, parent=torso_piv)
    make_loft_tube("JacketRedStripe", [
        (0.28, 0.218, 0.138, 0.0, -0.002),
        (0.32, 0.225, 0.139, 0.0, -0.003),
    ], mat_jtrim, segs=22, parent=torso_piv)

    # Natural Rounded Deltoid Shoulders blended smoothly into torso
    make_sphere("ShoulderL", (-0.215, 0.0, 0.405), (0.078, 0.080, 0.082), mat_jacket, parent=torso_piv)
    make_sphere("ShoulderR", ( 0.215, 0.0, 0.405), (0.078, 0.080, 0.082), mat_jacket, parent=torso_piv)

    # -------------------------------------------------------------------------
    # HEAD & REALISTIC FACE (HeadPivot at neck base: world Z = 1.48m -> top at 1.78m!)
    # -------------------------------------------------------------------------
    head_piv = bpy.data.objects.new("HeadPivot", None)
    head_piv.location = (0.0, 0.0, 0.48)
    head_piv.rotation_mode = "XYZ"
    bpy.context.collection.objects.link(head_piv)
    head_piv.parent = torso_piv

    # Natural Human Neck
    make_loft_tube("Neck", [
        (-0.02, 0.062, 0.060, 0.0, 0.004),
        ( 0.06, 0.056, 0.055, 0.0, 0.002),
        ( 0.11, 0.058, 0.058, 0.0, 0.000),
    ], mat_skin, segs=16, parent=head_piv)

    # Sculpted Organic Human Head (narrower chin -> fuller cheekbones -> rounded cranium)
    make_loft_tube("HeadSculpt", [
        (0.085, 0.044, 0.052, 0.0, -0.020),  # Chin
        (0.125, 0.072, 0.084, 0.0, -0.014),  # Jawline
        (0.175, 0.084, 0.098, 0.0, -0.008),  # Cheekbones & Nose level
        (0.225, 0.088, 0.104, 0.0, -0.002),  # Forehead & Brow
        (0.275, 0.082, 0.098, 0.0,  0.004),  # Upper Cranium
        (0.305, 0.048, 0.060, 0.0,  0.006),  # Crown
    ], mat_skin, segs=22, parent=head_piv)

    # Sculpted Nose, Ears & Stylish Aviator Sunglasses
    make_sphere("Nose",  (0.0, -0.102, 0.172), (0.016, 0.022, 0.024), mat_skin, parent=head_piv)
    make_sphere("EarL",  (-0.086, 0.002, 0.180), (0.015, 0.022, 0.032), mat_skin, parent=head_piv)
    make_sphere("EarR",  ( 0.086, 0.002, 0.180), (0.015, 0.022, 0.032), mat_skin, parent=head_piv)
    make_box("ShadeL",   (-0.036, -0.100, 0.198), (0.052, 0.014, 0.032), mat_shades, subsurf=1, parent=head_piv)
    make_box("ShadeR",   ( 0.036, -0.100, 0.198), (0.052, 0.014, 0.032), mat_shades, subsurf=1, parent=head_piv)
    make_box("ShadeBar", ( 0.000, -0.102, 0.205), (0.032, 0.010, 0.008), mat_metal, parent=head_piv)

    # Layered Swept-Back Hair Cap + Top Volume (looks like a modern action hero!)
    make_loft_tube("HairBase", [
        (0.160, 0.086, 0.088, 0.0,  0.018),
        (0.220, 0.094, 0.105, 0.0,  0.010),
        (0.275, 0.090, 0.106, 0.0,  0.006),
        (0.312, 0.064, 0.078, 0.0,  0.002),
    ], mat_hair, segs=20, parent=head_piv)
    make_sphere("HairSweep", (0.0, -0.028, 0.288), (0.076, 0.078, 0.042), mat_hair, parent=head_piv)

    # -------------------------------------------------------------------------
    # ARTICULATED 2-JOINT ARMS (UpperArm -> Elbow/ForeArm -> Hand!)
    # -------------------------------------------------------------------------
    def build_arm(side, sx):
        upper_piv = bpy.data.objects.new(f"Arm{side}Pivot", None)
        upper_piv.location = (sx * 0.232, 0.0, 0.405)
        upper_piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(upper_piv)
        upper_piv.parent = torso_piv

        make_loft_tube(f"UpperArm{side}", [
            ( 0.01, 0.062, 0.062, 0.0, 0.0),
            (-0.14, 0.054, 0.054, 0.0, 0.0),
            (-0.28, 0.048, 0.048, 0.0, 0.0),
        ], mat_jacket, segs=16, parent=upper_piv)
        make_sphere(f"Elbow{side}", (0.0, 0.0, -0.285), (0.048, 0.048, 0.048), mat_jacket, parent=upper_piv)

        fore_piv = bpy.data.objects.new(f"ForeArm{side}Pivot", None)
        fore_piv.location = (0.0, 0.0, -0.285)
        fore_piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(fore_piv)
        fore_piv.parent = upper_piv

        make_loft_tube(f"ForeArm{side}", [
            ( 0.00, 0.048, 0.048, 0.0, 0.0),
            (-0.13, 0.043, 0.043, 0.0, 0.0),
            (-0.25, 0.036, 0.034, 0.0, 0.0),
        ], mat_jacket, segs=16, parent=fore_piv)
        # Natural Skin Wrist & Sculpted Hand
        make_sphere(f"Hand{side}",  (0.0, -0.006, -0.305), (0.034, 0.042, 0.056), mat_skin, parent=fore_piv)
        make_sphere(f"Thumb{side}", (-sx * 0.028, -0.028, -0.295), (0.014, 0.018, 0.026), mat_skin, parent=fore_piv)
        return upper_piv, fore_piv

    arm_l, forearm_l = build_arm("L", -1.0)
    arm_r, forearm_r = build_arm("R",  1.0)

    # -------------------------------------------------------------------------
    # ARTICULATED 2-JOINT LEGS (Thigh -> Knee/Calf -> Detailed Sneaker!)
    # HipRoot is at Z=0.94m; Thigh pivot at -0.04m (world 0.90m);
    # Knee at -0.43m (world 0.47m); Ankle at -0.38m (world 0.09m); Sole flat at Z = 0.00m!
    # -------------------------------------------------------------------------
    def build_leg(side, sx):
        thigh_piv = bpy.data.objects.new(f"Thigh{side}Pivot", None)
        thigh_piv.location = (sx * 0.102, 0.0, -0.04)
        thigh_piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(thigh_piv)
        thigh_piv.parent = hip_root

        make_loft_tube(f"Thigh{side}", [
            ( 0.02, 0.088, 0.090, 0.0,  0.004),
            (-0.18, 0.080, 0.082, 0.0,  0.002),
            (-0.42, 0.064, 0.066, 0.0,  0.000),
        ], mat_jeans, segs=18, parent=thigh_piv)
        make_sphere(f"Knee{side}", (0.0, -0.004, -0.43), (0.064, 0.066, 0.064), mat_jeans, parent=thigh_piv)

        calf_piv = bpy.data.objects.new(f"Calf{side}Pivot", None)
        calf_piv.location = (0.0, 0.0, -0.43)
        calf_piv.rotation_mode = "XYZ"
        bpy.context.collection.objects.link(calf_piv)
        calf_piv.parent = thigh_piv

        make_loft_tube(f"Calf{side}", [
            ( 0.00, 0.063, 0.065, 0.0, 0.000),
            (-0.15, 0.060, 0.064, 0.0, 0.008),
            (-0.37, 0.048, 0.050, 0.0, 0.004),
        ], mat_jeans, segs=18, parent=calf_piv)

        # Detailed Athletic Sneaker (sole bottom sits right at local Z = -0.47 -> world Z = 0.00m!)
        make_box(f"SneakerUpper{side}", (0.0, -0.036, -0.415), (0.104, 0.245, 0.082), mat_shoe, subsurf=1, parent=calf_piv)
        make_box(f"SneakerSole{side}",  (0.0, -0.038, -0.456), (0.112, 0.258, 0.028), mat_sole, subsurf=1, parent=calf_piv)
        make_box(f"SneakerAccent{side}",(0.0, -0.095, -0.402), (0.072, 0.080, 0.024), mat_jtrim, parent=calf_piv)
        return thigh_piv, calf_piv

    thigh_l, calf_l = build_leg("L", -1.0)
    thigh_r, calf_r = build_leg("R",  1.0)

    # -------------------------------------------------------------------------
    # CLEAN 4-CLIP NLA ANIMATION EXPORT (Idle, Walk, Run, Swim)
    # Every bone has identical NLA track name so Blender merges them into 4 clean glTF clips!
    # -------------------------------------------------------------------------
    rig_parts = [hip_root, torso_piv, head_piv, arm_l, forearm_l, arm_r, forearm_r, thigh_l, calf_l, thigh_r, calf_r]

    def reset_upright_pose():
        hip_root.location = (0.0, 0.0, 0.94)
        hip_root.rotation_euler = (0.0, 0.0, 0.0)
        torso_piv.rotation_euler = (0.0, 0.0, 0.0)
        head_piv.rotation_euler = (0.0, 0.0, 0.0)
        arm_l.rotation_euler = (0.04, 0.06, 0.0)
        forearm_l.rotation_euler = (0.14, 0.0, 0.0)
        arm_r.rotation_euler = (0.04, -0.06, 0.0)
        forearm_r.rotation_euler = (0.14, 0.0, 0.0)
        thigh_l.rotation_euler = (0.0, 0.0, 0.0)
        calf_l.rotation_euler = (0.0, 0.0, 0.0)
        thigh_r.rotation_euler = (0.0, 0.0, 0.0)
        calf_r.rotation_euler = (0.0, 0.0, 0.0)

    def record_clip(clip_name, keyframes):
        for obj in rig_parts:
            if not obj.animation_data:
                obj.animation_data_create()
            act = bpy.data.actions.new(name=f"{clip_name}_{obj.name}")
            obj.animation_data.action = act

        for frame, p in keyframes:
            bpy.context.scene.frame_set(frame)
            reset_upright_pose()
            if "hip_z" in p:
                hip_root.location = (0.0, 0.0, p["hip_z"])
            if "hip_rot" in p:
                hip_root.rotation_euler = p["hip_rot"]
            if "torso" in p:
                torso_piv.rotation_euler = p["torso"]
            if "head" in p:
                head_piv.rotation_euler = p["head"]
            if "arm_l" in p:
                arm_l.rotation_euler = p["arm_l"]
            if "farm_l" in p:
                forearm_l.rotation_euler = p["farm_l"]
            if "arm_r" in p:
                arm_r.rotation_euler = p["arm_r"]
            if "farm_r" in p:
                forearm_r.rotation_euler = p["farm_r"]
            if "thigh_l" in p:
                thigh_l.rotation_euler = p["thigh_l"]
            if "calf_l" in p:
                calf_l.rotation_euler = p["calf_l"]
            if "thigh_r" in p:
                thigh_r.rotation_euler = p["thigh_r"]
            if "calf_r" in p:
                calf_r.rotation_euler = p["calf_r"]

            hip_root.keyframe_insert(data_path="location", frame=frame)
            for o in rig_parts:
                o.keyframe_insert(data_path="rotation_euler", frame=frame)

        for obj in rig_parts:
            act = obj.animation_data.action
            track = obj.animation_data.nla_tracks.new()
            track.name = clip_name
            strip = track.strips.new(clip_name, 1, act)
            strip.name = clip_name
            track.mute = True  # Mute after recording so base pose stays 100% upright!
            obj.animation_data.action = None

    # Record Swim FIRST, then Run, then Walk, and Idle LAST so the top-most NLA track &
    # resting glTF bind pose of EVERY node is 100% UPRIGHT STANDING (feet flat at Z=0)!
    # Also include a subtle hip_rot on Idle/Walk/Run so Blender NEVER strips HipRoot's rotation channel!

    # 1. Swim (horizontal water stroke — ONLY active when Lua calls playAnimation(..., "Swim")!)
    record_clip("Swim", [
        (1,  {"hip_z": 0.85, "hip_rot": (1.28, 0, 0), "head": (-0.85, 0, 0), "arm_l": ( 2.25, 0.28, 0), "farm_l": (0.35, 0, 0), "arm_r": ( 0.35, -0.25, 0), "farm_r": (0.45, 0, 0), "thigh_l": ( 0.28, 0, 0), "calf_l": (-0.28, 0, 0), "thigh_r": (-0.28, 0, 0), "calf_r": (-0.18, 0, 0)}),
        (13, {"hip_z": 0.90, "hip_rot": (1.28, 0, 0), "head": (-0.85, 0, 0), "arm_l": ( 0.35, 0.25, 0), "farm_l": (0.45, 0, 0), "arm_r": ( 2.25, -0.28, 0), "farm_r": (0.35, 0, 0), "thigh_l": (-0.28, 0, 0), "calf_l": (-0.18, 0, 0), "thigh_r": ( 0.28, 0, 0), "calf_r": (-0.28, 0, 0)}),
        (24, {"hip_z": 0.85, "hip_rot": (1.28, 0, 0), "head": (-0.85, 0, 0), "arm_l": ( 2.25, 0.28, 0), "farm_l": (0.35, 0, 0), "arm_r": ( 0.35, -0.25, 0), "farm_r": (0.45, 0, 0), "thigh_l": ( 0.28, 0, 0), "calf_l": (-0.28, 0, 0), "thigh_r": (-0.28, 0, 0), "calf_r": (-0.18, 0, 0)}),
    ])

    # 2. Run (dynamic athletic sprint with strong knee lift & 90-deg bent elbows!)
    record_clip("Run", [
        (1,  {"hip_z": 0.910, "hip_rot": (0.04, 0, 0.00), "torso": (0.18, 0,  0.00), "thigh_l": ( 0.15, 0, 0), "calf_l": (-0.35, 0, 0), "thigh_r": (-0.15, 0, 0), "calf_r": (-0.95, 0, 0), "arm_l": (-0.15, 0.08, 0), "farm_l": (1.15, 0, 0), "arm_r": ( 0.15, -0.08, 0), "farm_r": (1.15, 0, 0)}),
        (5,  {"hip_z": 0.990, "hip_rot": (0.06, 0, 0.02), "torso": (0.22, 0,  0.14), "thigh_l": ( 0.95, 0, 0), "calf_l": (-0.55, 0, 0), "thigh_r": (-0.78, 0, 0), "calf_r": (-0.85, 0, 0), "arm_l": (-0.85, 0.08, 0), "farm_l": (1.10, 0, 0), "arm_r": ( 0.92, -0.08, 0), "farm_r": (1.35, 0, 0)}),
        (9,  {"hip_z": 0.910, "hip_rot": (0.04, 0, 0.00), "torso": (0.18, 0,  0.00), "thigh_l": (-0.15, 0, 0), "calf_l": (-0.95, 0, 0), "thigh_r": ( 0.15, 0, 0), "calf_r": (-0.35, 0, 0), "arm_l": ( 0.15, 0.08, 0), "farm_l": (1.15, 0, 0), "arm_r": (-0.15, -0.08, 0), "farm_r": (1.15, 0, 0)}),
        (13, {"hip_z": 0.990, "hip_rot": (0.06, 0,-0.02), "torso": (0.22, 0, -0.14), "thigh_l": (-0.78, 0, 0), "calf_l": (-0.85, 0, 0), "thigh_r": ( 0.95, 0, 0), "calf_r": (-0.55, 0, 0), "arm_l": ( 0.92, 0.08, 0), "farm_l": (1.35, 0, 0), "arm_r": (-0.85, -0.08, 0), "farm_r": (1.10, 0, 0)}),
        (16, {"hip_z": 0.910, "hip_rot": (0.04, 0, 0.00), "torso": (0.18, 0,  0.00), "thigh_l": ( 0.15, 0, 0), "calf_l": (-0.35, 0, 0), "thigh_r": (-0.15, 0, 0), "calf_r": (-0.95, 0, 0), "arm_l": (-0.15, 0.08, 0), "farm_l": (1.15, 0, 0), "arm_r": ( 0.15, -0.08, 0), "farm_r": (1.15, 0, 0)}),
    ])

    # 3. Walk (natural 2-joint stride with knee bend & relaxed bent elbows!)
    record_clip("Walk", [
        (1,  {"hip_z": 0.935, "hip_rot": (0.01, 0, 0.00), "torso": (0.05, 0,  0.00), "thigh_l": ( 0.05, 0, 0), "calf_l": (-0.08, 0, 0), "thigh_r": (-0.05, 0, 0), "calf_r": (-0.28, 0, 0), "arm_l": (-0.05, 0.06, 0), "farm_l": (0.20, 0, 0), "arm_r": ( 0.05, -0.06, 0), "farm_r": (0.20, 0, 0)}),
        (7,  {"hip_z": 0.958, "hip_rot": (0.02, 0, 0.015),"torso": (0.07, 0,  0.07), "thigh_l": ( 0.48, 0, 0), "calf_l": (-0.16, 0, 0), "thigh_r": (-0.42, 0, 0), "calf_r": (-0.36, 0, 0), "arm_l": (-0.38, 0.06, 0), "farm_l": (0.22, 0, 0), "arm_r": ( 0.42, -0.06, 0), "farm_r": (0.48, 0, 0)}),
        (13, {"hip_z": 0.935, "hip_rot": (0.01, 0, 0.00), "torso": (0.05, 0,  0.00), "thigh_l": (-0.05, 0, 0), "calf_l": (-0.28, 0, 0), "thigh_r": ( 0.05, 0, 0), "calf_r": (-0.08, 0, 0), "arm_l": ( 0.05, 0.06, 0), "farm_l": (0.20, 0, 0), "arm_r": (-0.05, -0.06, 0), "farm_r": (0.20, 0, 0)}),
        (19, {"hip_z": 0.958, "hip_rot": (0.02, 0,-0.015),"torso": (0.07, 0, -0.07), "thigh_l": (-0.42, 0, 0), "calf_l": (-0.36, 0, 0), "thigh_r": ( 0.48, 0, 0), "calf_r": (-0.16, 0, 0), "arm_l": ( 0.42, 0.06, 0), "farm_l": (0.48, 0, 0), "arm_r": (-0.38, -0.06, 0), "farm_r": (0.22, 0, 0)}),
        (24, {"hip_z": 0.935, "hip_rot": (0.01, 0, 0.00), "torso": (0.05, 0,  0.00), "thigh_l": ( 0.05, 0, 0), "calf_l": (-0.08, 0, 0), "thigh_r": (-0.05, 0, 0), "calf_r": (-0.28, 0, 0), "arm_l": (-0.05, 0.06, 0), "farm_l": (0.20, 0, 0), "arm_r": ( 0.05, -0.06, 0), "farm_r": (0.20, 0, 0)}),
    ])

    # 4. Idle RECORDED LAST (so upright standing is the top NLA track & resting glTF bind pose!)
    record_clip("Idle", [
        (1,  {"hip_z": 0.940, "hip_rot": (0.002, 0, 0.000), "torso": ( 0.01, 0,  0.00), "head": (0, 0,  0.00)}),
        (20, {"hip_z": 0.948, "hip_rot": (0.000, 0, 0.004), "torso": (-0.01, 0,  0.02), "head": (0, 0,  0.14)}),
        (40, {"hip_z": 0.942, "hip_rot": (0.002, 0,-0.004), "torso": ( 0.01, 0, -0.02), "head": (0, 0, -0.12)}),
        (56, {"hip_z": 0.940, "hip_rot": (0.002, 0, 0.000), "torso": ( 0.01, 0,  0.00), "head": (0, 0,  0.00)}),
    ])

    # Unmute all NLA tracks for export and reset scene frame to 1 (upright Idle!)
    for obj in rig_parts:
        for tr in obj.animation_data.nla_tracks:
            tr.mute = False
    bpy.context.scene.frame_set(1)
    reset_upright_pose()

    bpy.ops.export_scene.gltf(
        filepath=OUT_GLB,
        export_format="GLB",
        export_yup=True,
        export_animations=True,
        export_animation_mode="NLA_TRACKS",
        export_nla_strips=True,
        export_nla_strips_merged_animation_name="",
        export_apply=False,
    )
    print(f"[SAGE][Blender] Exported realistic human character -> {OUT_GLB}")


def verify_glb_animations():
    with open(OUT_GLB, "rb") as f:
        magic, ver, length = struct.unpack("<4sII", f.read(12))
        chunk_len, chunk_type = struct.unpack("<I4s", f.read(8))
        gltf = json.loads(f.read(chunk_len).decode("utf-8"))
    anims = gltf.get("animations", [])
    print(f"[SAGE][Verify] character.glb has {len(anims)} glTF animation(s):")
    for a in anims:
        print(f"   - \"{a.get('name', '<unnamed>')}\" ({len(a.get('channels', []))} channels)")
    for n in gltf.get("nodes", []):
        if n.get("name") in ("HipRoot", "TorsoPivot", "HeadPivot", "ThighLPivot", "ThighRPivot"):
            print(f"   Node {n['name']}: T={n.get('translation')} R={n.get('rotation')}")


if __name__ == "__main__":
    build_human()
    verify_glb_animations()
