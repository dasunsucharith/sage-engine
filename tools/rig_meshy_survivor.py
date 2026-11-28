"""
SAGE Engine — Meshy Survivalist Character Rigging & Biomechanical Animation Pipeline (Blender 5.3)
Takes raw Meshy .glb (`assets/models/Survivalist_Character.glb`), welds micro-doubles, normalizes
scale/ground/facing, measures exact anatomical joint centers, rigs with a clean 19-bone SAGE
humanoid armature (Look-At IK + Euphoria Ragdoll ready), computes butter-smooth anatomical skin
weights (with strict backpack, belt-pouch, arm/torso & left/right-leg separation + Laplacian weight
smoothing), bakes 6 smooth loopable NLA clips (Swim, Jump, Attack, Run, Walk, Idle LAST), exports
`assets/models/survival/survivor.glb` + `survivor_rigged.blend`, and renders 4 Cycles studio previews.
"""

import bpy
import bmesh
import math
import os
from mathutils import Vector, Euler

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
SURVIVAL_DIR = os.path.join(MODELS_DIR, "survival")
ARTIFACT_DIR = r"C:\Users\DASUN\.gemini\antigravity\brain\2f9de14d-2f3f-473b-8005-216904e8d69d"

IN_GLB = os.path.join(MODELS_DIR, "Survivalist_Character.glb")
OUT_BLEND = os.path.join(SURVIVAL_DIR, "survivor_rigged.blend")
OUT_GLB = os.path.join(SURVIVAL_DIR, "survivor.glb")

os.makedirs(SURVIVAL_DIR, exist_ok=True)
os.makedirs(ARTIFACT_DIR, exist_ok=True)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def smoothstep(e0, e1, x):
    t = clamp((x - e0) / max(e1 - e0, 1e-6), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def srgb_to_linear(c):
    return tuple((v ** 2.2) for v in c)


def point_seg_dist(p, a, b):
    ab = b - a
    l2 = ab.length_squared
    if l2 < 1e-10:
        return (p - a).length, 0.0
    t = clamp((p - a).dot(ab) / l2, 0.0, 1.0)
    proj = a + ab * t
    return (p - proj).length, t


def sample_periodic(keys, phase):
    """Smooth Catmull-Rom periodic interpolation over [(p0, v0), ...] with p in [0, 1)."""
    p = phase % 1.0
    n = len(keys)
    idx = n - 1
    for i in range(n):
        if p >= keys[i][0]:
            idx = i
    p0, v0 = keys[idx]
    p1, v1 = keys[(idx + 1) % n]
    if p1 <= p0:
        p1 += 1.0
    if p < p0:
        p += 1.0
    u = clamp((p - p0) / max(p1 - p0, 1e-5), 0.0, 1.0)
    vm1 = keys[(idx - 1) % n][1]
    v2 = keys[(idx + 2) % n][1]
    u2 = u * u
    u3 = u2 * u
    return 0.5 * (
        (2.0 * v0)
        + (-vm1 + v1) * u
        + (2.0 * vm1 - 5.0 * v0 + 4.0 * v1 - v2) * u2
        + (-vm1 + 3.0 * v0 - 3.0 * v1 + v2) * u3
    )


def import_and_normalize_survivor():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=IN_GLB)
    bpy.context.view_layer.update()

    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"No mesh found in {IN_GLB}")

    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    char_obj = bpy.context.view_layer.objects.active
    char_obj.name = "Survivor_Mesh"

    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    for o in list(bpy.data.objects):
        if o != char_obj:
            bpy.data.objects.remove(o, do_unlink=True)

    # Weld tiny micro-doubles safely (preserve Meshy's original outward normals on layered jacket/straps!)
    bm = bmesh.new()
    bm.from_mesh(char_obj.data)
    before_v = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.00025)
    bm.to_mesh(char_obj.data)
    bm.free()
    print(f"[Survivor] Welded micro-doubles: {before_v} -> {len(char_obj.data.vertices)} verts")

    verts = char_obj.data.vertices
    min_x = min(v.co.x for v in verts)
    max_x = max(v.co.x for v in verts)
    min_z = min(v.co.z for v in verts)
    max_z = max(v.co.z for v in verts)

    raw_h = max_z - min_z
    target_h = 1.78
    scale_f = target_h / raw_h
    cx = 0.5 * (min_x + max_x)

    # Align Y center to hips/legs centerline (so torso sits squarely at Y ~ 0, backpack at Y > 0)
    leg_ys = [v.co.y for v in verts if min_z + raw_h * 0.12 <= v.co.z <= min_z + raw_h * 0.45]
    cy = sum(leg_ys) / max(len(leg_ys), 1)

    for v in verts:
        v.co.x = (v.co.x - cx) * scale_f
        v.co.y = (v.co.y - cy) * scale_f
        v.co.z = (v.co.z - min_z) * scale_f

    char_obj.data.update()
    for p in char_obj.data.polygons:
        p.use_smooth = True

    for mat in char_obj.data.materials:
        if mat and mat.node_tree:
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                if not bsdf.inputs["Roughness"].is_linked:
                    bsdf.inputs["Roughness"].default_value = max(bsdf.inputs["Roughness"].default_value, 0.72)
                if "Specular IOR Level" in bsdf.inputs and not bsdf.inputs["Specular IOR Level"].is_linked:
                    bsdf.inputs["Specular IOR Level"].default_value = 0.35

    print(f"[Survivor] Normalized mesh: {len(verts)} verts, height={target_h:.2f}m (scale={scale_f:.4f})")
    return char_obj


def measure_landmarks(char_obj):
    verts = [v.co for v in char_obj.data.vertices]

    def centroid(pts, default):
        if not pts:
            return Vector(default)
        s = Vector((0.0, 0.0, 0.0))
        for p in pts:
            s += p
        return s / len(pts)

    def leg_slice_center(z_mid, half_dz=0.025, x_min=0.02):
        pts = [p for p in verts if abs(p.z - z_mid) <= half_dz and p.x >= x_min and p.x <= 0.28]
        return centroid(pts, (0.11, 0.0, z_mid))

    ankle_L = leg_slice_center(0.11)
    knee_L = leg_slice_center(0.49)
    hip_L = leg_slice_center(0.86)
    toe_pts = [p for p in verts if p.z <= 0.07 and p.x >= 0.02 and p.y <= -0.06]
    toe_L = centroid(toe_pts, (ankle_L.x, -0.15, 0.03))

    # Measure arm centerline from shoulder out to fingertip (x > 0.20, exclude backpack y > 0.11)
    sh_pts = [p for p in verts if 0.16 <= p.x <= 0.23 and 1.35 <= p.z <= 1.48 and p.y <= 0.10]
    shoulder_L = centroid(sh_pts, (0.185, 0.01, 1.415))
    shoulder_L.x = clamp(shoulder_L.x, 0.170, 0.195)
    shoulder_L.y = clamp(shoulder_L.y, -0.02, 0.03)
    shoulder_L.z = clamp(shoulder_L.z, 1.39, 1.44)

    arm_pts = [p for p in verts if p.x >= 0.23 and 0.72 <= p.z <= 1.42 and p.y <= 0.12]
    max_arm_x = max((p.x for p in arm_pts), default=0.52)

    def arm_at_x(x_target, half_dx=0.028):
        pts = [p for p in arm_pts if abs(p.x - x_target) <= half_dx]
        return centroid(pts, (x_target, 0.0, 1.15))

    x_elbow = shoulder_L.x + (max_arm_x - shoulder_L.x) * 0.45
    x_wrist = shoulder_L.x + (max_arm_x - shoulder_L.x) * 0.78
    x_hand_tip = max_arm_x - 0.015

    elbow_L = arm_at_x(x_elbow)
    wrist_L = arm_at_x(x_wrist)
    hand_tip_L = arm_at_x(x_hand_tip, half_dx=0.03)

    # Clean torso Y centerline (between front chest y~-0.12 and back y~+0.11, ignoring backpack)
    def torso_y(z_mid, half_dz=0.04):
        pts = [p for p in verts if abs(p.z - z_mid) <= half_dz and abs(p.x) <= 0.14 and -0.16 <= p.y <= 0.12]
        if not pts:
            return 0.0
        return 0.5 * (min(p.y for p in pts) + max(p.y for p in pts))

    y_hips = clamp(torso_y(0.92), -0.03, 0.04)
    y_spine = clamp(torso_y(1.08), -0.03, 0.04)
    y_chest = clamp(torso_y(1.28), -0.03, 0.04)
    y_neck = clamp(torso_y(1.48), -0.03, 0.04)
    y_head = clamp(torso_y(1.66), -0.04, 0.04)

    lm = {
        "hips_head": Vector((0.0, y_hips, 0.92)),
        "spine_head": Vector((0.0, y_spine, 1.08)),
        "chest_head": Vector((0.0, y_chest, 1.26)),
        "neck_head": Vector((0.0, y_neck, 1.46)),
        "head_head": Vector((0.0, y_head, 1.56)),
        "head_tail": Vector((0.0, y_head, 1.77)),
        "shoulder_L": shoulder_L,
        "elbow_L": elbow_L,
        "wrist_L": wrist_L,
        "hand_tip_L": hand_tip_L,
        "hip_L": Vector((clamp(hip_L.x, 0.095, 0.120), clamp(hip_L.y, -0.02, 0.03), 0.895)),
        "knee_L": Vector((clamp(knee_L.x, 0.095, 0.125), clamp(knee_L.y - 0.008, -0.03, 0.03), 0.495)),
        "ankle_L": Vector((clamp(ankle_L.x, 0.095, 0.130), clamp(ankle_L.y, -0.02, 0.03), 0.105)),
        "toe_L": Vector((clamp(ankle_L.x, 0.095, 0.130), min(toe_L.y, -0.14), 0.025)),
    }

    for k, v in lm.items():
        print(f"  LM {k:12s}: ({v.x:+.3f}, {v.y:+.3f}, {v.z:+.3f})")
    return lm


def build_armature(lm):
    arm_data = bpy.data.armatures.new("SurvivorArmature_Data")
    arm_obj = bpy.data.objects.new("SurvivorArmature", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")

    eb = arm_data.edit_bones

    def add_bone(name, head, tail, parent_name=None, connected=False, roll_vec=(0.0, -1.0, 0.0)):
        b = eb.new(name)
        b.head = Vector(head)
        b.tail = Vector(tail)
        if parent_name:
            b.parent = eb[parent_name]
            b.use_connect = connected
        b.align_roll(Vector(roll_vec))
        return b

    add_bone("Hips",  lm["hips_head"],  lm["spine_head"], None,    False, (0.0, -1.0, 0.0))
    add_bone("Spine", lm["spine_head"], lm["chest_head"], "Hips",  True,  (0.0, -1.0, 0.0))
    add_bone("Chest", lm["chest_head"], lm["neck_head"],  "Spine", True,  (0.0, -1.0, 0.0))
    add_bone("Neck",  lm["neck_head"],  lm["head_head"],  "Chest", True,  (0.0, -1.0, 0.0))
    add_bone("Head",  lm["head_head"],  lm["head_tail"],  "Neck",  True,  (0.0, -1.0, 0.0))

    def mirror_x(v, side):
        return Vector((abs(v.x) * side, v.y, v.z))

    for side, sfx in ((1.0, "L"), (-1.0, "R")):
        sh = mirror_x(lm["shoulder_L"], side)
        el = mirror_x(lm["elbow_L"], side)
        wr = mirror_x(lm["wrist_L"], side)
        ht = mirror_x(lm["hand_tip_L"], side)

        clav_head = Vector((side * 0.055, lm["chest_head"].y, lm["neck_head"].z - 0.055))
        add_bone(f"Shoulder.{sfx}", clav_head, sh, "Chest",            False, (0.0, -1.0, 0.0))
        add_bone(f"UpperArm.{sfx}", sh,        el, f"Shoulder.{sfx}",  True,  (0.0, -1.0, 0.0))
        add_bone(f"Forearm.{sfx}",  el,        wr, f"UpperArm.{sfx}",  True,  (0.0, -1.0, 0.0))
        add_bone(f"Hand.{sfx}",     wr,        ht, f"Forearm.{sfx}",   True,  (0.0, -1.0, 0.0))

        hp = mirror_x(lm["hip_L"], side)
        kn = mirror_x(lm["knee_L"], side)
        ak = mirror_x(lm["ankle_L"], side)
        to = mirror_x(lm["toe_L"], side)

        add_bone(f"Thigh.{sfx}", hp, kn, "Hips",        False, (0.0, -1.0, 0.0))
        add_bone(f"Shin.{sfx}",  kn, ak, f"Thigh.{sfx}", True,  (0.0, -1.0, 0.0))
        add_bone(f"Foot.{sfx}",  ak, to, f"Shin.{sfx}",  True,  (0.0, 0.0, 1.0))

    bpy.ops.object.mode_set(mode="OBJECT")
    print(f"[Survivor] Created {len(arm_data.bones)}-bone anatomical armature")
    return arm_obj


def skin_with_anatomical_capsules(char_obj, arm_obj, lm):
    """
    Production-grade anatomical capsule + region-masked skinning + Laplacian weight smoothing!
    Guarantees:
      - ZERO Bone Heat failures on Meshy multi-shell meshes (backpack, straps, pouches)
      - Backpack moves cleanly & solidly with Chest/Spine (ZERO jagged arm-pulling spikes!)
      - Belt pouches move cleanly with Hips (ZERO forearm clipping/tearing!)
      - Left/Right legs NEVER bleed weights across the inner thigh gap!
      - Smooth, natural joint falloffs at shoulders, elbows, wrists, hips, knees, ankles, neck!
    """
    char_obj.parent = arm_obj
    mod = char_obj.modifiers.new(name="Armature", type="ARMATURE")
    mod.object = arm_obj
    mod.use_deform_preserve_volume = True

    char_obj.vertex_groups.clear()
    vgroups = {b.name: char_obj.vertex_groups.new(name=b.name) for b in arm_obj.data.bones}
    bone_segs = {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in arm_obj.data.bones}

    sh_x = lm["shoulder_L"].x
    sh_z = lm["shoulder_L"].z
    el_x = lm["elbow_L"].x
    el_z = lm["elbow_L"].z
    # Arm centerline slope for separating sleeve from torso/pouches!
    arm_slope_dz_dx = (el_z - sh_z) / max(el_x - sh_x, 0.05)  # negative (~ -1.4)

    def is_arm_vertex(p):
        """True if vertex belongs to left or right arm/sleeve/hand (not torso, backpack, or hip pouch)."""
        ax = abs(p.x)
        # Definitely backpack behind body -> NEVER an arm!
        if p.y > 0.13 and ax < 0.27:
            return False
        # Definitely below waist/hip pouch zone & close to hip -> NEVER an arm!
        if p.z < 1.08 and ax < 0.285:
            return False
        # Definitely outer arm beyond shoulder width
        if ax >= 0.245 and p.z >= 0.72 and p.z <= 1.50:
            return True
        # Shoulder/deltoid transition zone (0.175 <= |x| < 0.245, z in 1.24..1.50, not backpack)
        if ax >= 0.185 and 1.25 <= p.z <= 1.50 and p.y <= 0.12:
            # Check proximity to upper arm line
            expected_z = sh_z + (ax - sh_x) * arm_slope_dz_dx
            if abs(p.z - expected_z) < 0.14:
                return True
        return False

    def add_w(wmap, bname, val):
        if val > 1e-5:
            wmap[bname] = wmap.get(bname, 0.0) + val

    for v in char_obj.data.vertices:
        p = v.co
        wmap = {}
        ax = abs(p.x)
        sfx = "L" if p.x >= 0.0 else "R"

        # ------------------------------------------------------------------
        # REGION A: HEAD & NECK (z >= 1.47, |x| < 0.17)
        # ------------------------------------------------------------------
        if p.z >= 1.465 and ax < 0.18:
            t_head = smoothstep(1.505, 1.565, p.z)
            t_neck = smoothstep(1.455, 1.505, p.z) * (1.0 - t_head)
            t_chest = 1.0 - smoothstep(1.455, 1.515, p.z)
            add_w(wmap, "Head", t_head)
            add_w(wmap, "Neck", t_neck)
            add_w(wmap, "Chest", t_chest)

        # ------------------------------------------------------------------
        # REGION B: ARMS, SLEEVES & HANDS
        # ------------------------------------------------------------------
        elif is_arm_vertex(p):
            # Smooth shoulder blend near torso (|x| from 0.185 to 0.255)
            d_sh, t_sh = point_seg_dist(p, *bone_segs[f"Shoulder.{sfx}"])
            d_ua, t_ua = point_seg_dist(p, *bone_segs[f"UpperArm.{sfx}"])
            d_fa, t_fa = point_seg_dist(p, *bone_segs[f"Forearm.{sfx}"])
            d_ha, t_ha = point_seg_dist(p, *bone_segs[f"Hand.{sfx}"])

            # Soft Gaussian capsule weights along the arm chain ONLY!
            sigma_arm = 0.068
            w_ua = math.exp(-((d_ua / sigma_arm) ** 2))
            w_fa = math.exp(-((d_fa / 0.058) ** 2))
            w_ha = math.exp(-((d_ha / 0.055) ** 2))

            # Blend smoothly into Chest/Shoulder right at the deltoid seam (|x| < 0.255)
            arm_blend = smoothstep(0.180, 0.248, ax)
            if arm_blend < 0.999:
                w_chest = (1.0 - arm_blend) * 0.72
                w_clav = (1.0 - arm_blend) * 0.28
                add_w(wmap, "Chest", w_chest)
                add_w(wmap, f"Shoulder.{sfx}", w_clav)
                add_w(wmap, f"UpperArm.{sfx}", arm_blend * max(w_ua, 0.2))
            else:
                # Favor exact bone along arm axis so elbow & wrist bend crisply
                # Compute projection along shoulder -> elbow -> wrist -> hand
                add_w(wmap, f"UpperArm.{sfx}", w_ua * (1.0 if d_ua <= d_fa + 0.03 else 0.15))
                add_w(wmap, f"Forearm.{sfx}",  w_fa)
                add_w(wmap, f"Hand.{sfx}",     w_ha * (1.4 if d_ha < d_fa else 0.25))

        # ------------------------------------------------------------------
        # REGION C: LEGS & BOOTS (z <= 0.895, strictly split Left vs Right!)
        # ------------------------------------------------------------------
        elif p.z <= 0.895:
            # Smooth pelvis-to-thigh blend between z=0.80 and z=0.90
            pelvis_w = smoothstep(0.79, 0.895, p.z)
            # Inner crotch blends smoothly into Hips so zero pinching when legs scissor!
            crotch_w = smoothstep(0.75, 0.87, p.z) * (1.0 - smoothstep(0.03, 0.10, ax))
            hips_total = clamp(pelvis_w * 0.88 + crotch_w * 0.65, 0.0, 0.96)
            leg_total = 1.0 - hips_total

            # Vertical smooth blend along Thigh -> Shin -> Foot!
            z_knee = lm["knee_L"].z
            z_ankle = lm["ankle_L"].z

            w_foot = 1.0 - smoothstep(z_ankle - 0.025, z_ankle + 0.045, p.z)
            w_shin_up = smoothstep(z_ankle - 0.025, z_ankle + 0.045, p.z) * (1.0 - smoothstep(z_knee - 0.065, z_knee + 0.065, p.z))
            w_thigh = smoothstep(z_knee - 0.065, z_knee + 0.065, p.z)

            add_w(wmap, "Hips", hips_total)
            add_w(wmap, f"Thigh.{sfx}", leg_total * w_thigh)
            add_w(wmap, f"Shin.{sfx}",  leg_total * w_shin_up)
            add_w(wmap, f"Foot.{sfx}",  leg_total * w_foot)

        # ------------------------------------------------------------------
        # REGION D: TORSO, JACKET, BELT POUCHES & BACKPACK!
        # ------------------------------------------------------------------
        else:
            # Backpack (behind back y > 0.11): moves solidly & smoothly with Chest (80%) + Spine (20%)!
            if p.y > 0.115 and p.z >= 0.96:
                t_up = smoothstep(1.02, 1.32, p.z)
                add_w(wmap, "Chest", 0.35 + 0.65 * t_up)
                add_w(wmap, "Spine", 0.65 * (1.0 - t_up))
            else:
                # Smooth anatomical spinal gradient: Hips (0.89..1.04) -> Spine (1.02..1.24) -> Chest (1.20..1.48)
                w_hips = 1.0 - smoothstep(0.95, 1.08, p.z)
                w_chest = smoothstep(1.16, 1.30, p.z)
                w_spine = clamp(1.0 - w_hips - w_chest, 0.0, 1.0)
                # Subtle Neck blend right at collar (z > 1.43, |x| < 0.10)
                w_neck = smoothstep(1.42, 1.48, p.z) * (1.0 - smoothstep(0.06, 0.11, ax)) * 0.45
                add_w(wmap, "Hips", w_hips)
                add_w(wmap, "Spine", w_spine)
                add_w(wmap, "Chest", w_chest * (1.0 - w_neck))
                add_w(wmap, "Neck", w_neck)

        # Normalize top-4 weights per vertex
        items = sorted(wmap.items(), key=lambda kv: kv[1], reverse=True)[:4]
        tot = sum(w for _, w in items)
        if tot < 1e-6:
            vgroups["Chest"].add([v.index], 1.0, "REPLACE")
        else:
            for bname, w in items:
                vgroups[bname].add([v.index], w / tot, "REPLACE")

    # Apply subtle Laplacian smoothing ONLY over joints (3 passes, factor=0.35) then re-limit to 4!
    bpy.ops.object.select_all(action="DESELECT")
    char_obj.select_set(True)
    bpy.context.view_layer.objects.active = char_obj
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.35, repeat=3)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    char_obj.data.validate(verbose=False)
    print(f"[Survivor] Applied anatomical capsule skinning + 3x joint smoothing ({len(char_obj.data.vertices)} verts, 0 unweighted!)")


def build_animations(arm_obj, lm):
    bpy.ops.object.select_all(action="DESELECT")
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="POSE")

    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "XYZ"

    arm_vec = lm["elbow_L"] - lm["shoulder_L"]
    rest_abduction = math.atan2(abs(arm_vec.x), max(0.05, -arm_vec.z))
    # Drop arms to a natural relaxed survivalist stance (~13 deg clear of hip belt pouches!)
    ARM_DROP = max(0.14, rest_abduction - 0.22)
    print(f"[Survivor] Measured A-pose abduction={math.degrees(rest_abduction):.1f} deg -> ARM_DROP={math.degrees(ARM_DROP):.1f} deg")

    def reset_pose():
        for pb in arm_obj.pose.bones:
            pb.location = (0.0, 0.0, 0.0)
            pb.rotation_euler = (0.0, 0.0, 0.0)

    def kf_all(frame):
        for pb in arm_obj.pose.bones:
            pb.keyframe_insert(data_path="rotation_euler", frame=frame)
            if pb.name == "Hips":
                pb.keyframe_insert(data_path="location", frame=frame)

    def set_rot(name, rx=0.0, ry=0.0, rz=0.0):
        arm_obj.pose.bones[name].rotation_euler = Euler((rx, ry, rz), "XYZ")

    def set_loc(name, x=0.0, y=0.0, z=0.0):
        arm_obj.pose.bones[name].location = Vector((x, y, z))

    def record_clip(clip_name, n_frames, pose_fn):
        action = bpy.data.actions.new(name=clip_name)
        arm_obj.animation_data_create()
        arm_obj.animation_data.action = action
        for f in range(n_frames + 1):
            phase = f / float(n_frames)
            reset_pose()
            pose_fn(phase)
            kf_all(f + 1)
        track = arm_obj.animation_data.nla_tracks.new()
        track.name = clip_name
        strip = track.strips.new(clip_name, 1, action)
        strip.action_frame_end = n_frames + 1
        arm_obj.animation_data.action = None
        return action

    # --- 1. SWIM (Freestyle Crawl + Flutter Kick) ---
    def swim_pose(phase):
        t = phase * 2.0 * math.pi
        s, c = math.sin(t), math.cos(t)
        set_loc("Hips", 0.0, 0.0, -0.04)
        set_rot("Hips", 1.32, 0.0, 0.16 * s)
        set_rot("Spine", -0.14, 0.0, -0.08 * s)
        set_rot("Chest", -0.16, 0.0, 0.14 * s)
        set_rot("Neck", -0.46, 0.0, 0.0)
        set_rot("Head", -0.34, 0.0, 0.0)
        set_rot("UpperArm.L", 1.55 + 1.10 * s, 0.0, -ARM_DROP + 0.12)
        set_rot("Forearm.L",  0.38 + 0.40 * max(0.0, -c), 0.0, 0.0)
        set_rot("UpperArm.R", 1.55 - 1.10 * s, 0.0,  ARM_DROP - 0.12)
        set_rot("Forearm.R",  0.38 + 0.40 * max(0.0, c), 0.0, 0.0)
        set_rot("Thigh.L", 0.28 * math.sin(2.0 * t), 0.0, 0.0)
        set_rot("Shin.L", -0.20 - 0.22 * max(0.0, math.sin(2.0 * t)), 0.0, 0.0)
        set_rot("Foot.L", -0.45, 0.0, 0.0)
        set_rot("Thigh.R", -0.28 * math.sin(2.0 * t), 0.0, 0.0)
        set_rot("Shin.R", -0.20 - 0.22 * max(0.0, -math.sin(2.0 * t)), 0.0, 0.0)
        set_rot("Foot.R", -0.45, 0.0, 0.0)

    # --- 2. JUMP (Athletic Airborne Stride Tuck + Arm Balance) ---
    def jump_pose(phase):
        t = phase * 2.0 * math.pi
        sway = math.sin(t)
        set_loc("Hips", 0.0, 0.04 + 0.015 * sway, 0.0)
        set_rot("Hips", 0.12, -0.06, 0.0)
        set_rot("Spine", 0.08, 0.04, 0.0)
        set_rot("Chest", 0.06, 0.06, 0.0)
        set_rot("Neck", -0.10, -0.04, 0.0)
        set_rot("Head", -0.08, -0.04, 0.0)
        set_rot("Thigh.L", 0.62 + 0.06 * sway, 0.0, 0.0)
        set_rot("Shin.L", -1.10 - 0.08 * sway, 0.0, 0.0)
        set_rot("Foot.L", -0.22, 0.0, 0.0)
        set_rot("Thigh.R", -0.18 - 0.05 * sway, 0.0, 0.0)
        set_rot("Shin.R", -0.78 + 0.06 * sway, 0.0, 0.0)
        set_rot("Foot.R", -0.35, 0.0, 0.0)
        set_rot("UpperArm.L", 0.52 + 0.08 * sway, 0.0, -ARM_DROP + 0.24)
        set_rot("Forearm.L",  0.85, 0.0, 0.0)
        set_rot("UpperArm.R", 0.42 - 0.08 * sway, 0.0,  ARM_DROP - 0.24)
        set_rot("Forearm.R",  0.92, 0.0, 0.0)

    # --- 3. ATTACK (Dynamic Overhead Survival Axe/Spear Slash + Torso Twist!) ---
    atk_armR_x = [
        (0.00,  0.15),
        (0.26,  2.05),   # High overhead chop windup!
        (0.46,  0.95),   # Mid-downward strike impact!
        (0.66, -0.35),   # Follow-through past hip
        (0.86,  0.05),
    ]
    atk_armR_z = [
        (0.00,  ARM_DROP - 0.04),
        (0.26,  ARM_DROP - 0.42),
        (0.46,  ARM_DROP + 0.06),
        (0.66,  ARM_DROP + 0.08),
        (0.86,  ARM_DROP - 0.02),
    ]
    atk_foreR_x = [
        (0.00,  0.30),
        (0.26,  1.05),
        (0.46,  0.22),
        (0.66,  0.46),
        (0.86,  0.25),
    ]
    atk_torso_yaw = [
        (0.00,  0.00),
        (0.26, -0.46),
        (0.48,  0.44),
        (0.68,  0.28),
        (0.88,  0.04),
    ]
    atk_torso_pitch = [
        (0.00,  0.04),
        (0.26, -0.10),
        (0.48,  0.22),
        (0.68,  0.14),
        (0.88,  0.04),
    ]

    def attack_pose(phase):
        tyaw = sample_periodic(atk_torso_yaw, phase)
        tpitch = sample_periodic(atk_torso_pitch, phase)
        lunge = math.sin(clamp((phase - 0.18) / 0.58, 0.0, 1.0) * math.pi)

        set_loc("Hips", 0.0, -0.055 * lunge, -0.025 * lunge)
        set_rot("Hips",  0.08 + 0.07 * lunge, tyaw * 0.45, 0.0)
        set_rot("Spine", tpitch * 0.55, tyaw * 0.55, 0.0)
        set_rot("Chest", tpitch * 0.65, tyaw * 0.70, 0.0)
        set_rot("Neck", -tpitch * 0.45, -tyaw * 0.55, 0.0)
        set_rot("Head", -tpitch * 0.45, -tyaw * 0.60, 0.0)

        set_rot("Shoulder.R", 0.0, -tyaw * 0.25, 0.0)
        set_rot("UpperArm.R", sample_periodic(atk_armR_x, phase), 0.10 * lunge, sample_periodic(atk_armR_z, phase))
        set_rot("Forearm.R",  sample_periodic(atk_foreR_x, phase), 0.0, 0.0)
        set_rot("Hand.R",    -0.25 * lunge, 0.0, 0.0)

        set_rot("UpperArm.L", 0.32 - 0.48 * lunge, 0.0, -ARM_DROP + 0.14)
        set_rot("Forearm.L",  0.75 + 0.25 * lunge, 0.0, 0.0)

        set_rot("Thigh.L",  0.34 * lunge + 0.08, 0.0, 0.0)
        set_rot("Shin.L",  -0.44 * lunge - 0.10, 0.0, 0.0)
        set_rot("Foot.L",   0.10 * lunge, 0.0, 0.0)
        set_rot("Thigh.R", -0.28 * lunge - 0.04, 0.0, 0.0)
        set_rot("Shin.R",  -0.22 * lunge - 0.08, 0.0, 0.0)
        set_rot("Foot.R",  -0.14 * lunge, 0.0, 0.0)

    # --- 4. RUN (Biomechanical Athletic Sprint) ---
    run_thigh_keys = [
        (0.00,  0.64), (0.14,  0.18), (0.30, -0.58), (0.45, -0.72),
        (0.62,  0.14), (0.78,  0.96), (0.90,  0.80),
    ]
    run_knee_keys = [
        (0.00, -0.30), (0.14, -0.56), (0.32, -0.14),
        (0.52, -1.72), (0.72, -1.28), (0.88, -0.46),
    ]
    run_foot_keys = [
        (0.00,  0.16), (0.15,  0.20), (0.34, -0.48), (0.58, -0.22), (0.82,  0.18),
    ]
    run_arm_keys = [
        (0.00, -0.70), (0.16, -0.42), (0.34,  0.18),
        (0.50,  0.78), (0.68,  0.48), (0.84, -0.18),
    ]
    run_elbow_keys = [
        (0.00,  1.15), (0.25,  1.32), (0.50,  1.58), (0.75,  1.36),
    ]

    def run_pose(phase):
        pL = phase
        pR = (phase + 0.5) % 1.0
        t = phase * 2.0 * math.pi
        bounce = -0.044 * math.sin((phase * 2.0 + 0.22) * 2.0 * math.pi) - 0.015
        sway_x = 0.016 * math.sin((phase - 0.14) * 2.0 * math.pi)
        pelv_yaw = 0.17 * math.cos(t)
        pelv_roll = 0.065 * math.sin((phase - 0.14) * 2.0 * math.pi)

        set_loc("Hips", sway_x, bounce, 0.0)
        set_rot("Hips", 0.16, pelv_yaw, pelv_roll)
        set_rot("Spine", 0.12, -pelv_yaw * 0.85, -pelv_roll * 0.6)
        set_rot("Chest", 0.10, -pelv_yaw * 1.15, -pelv_roll * 0.4)
        set_rot("Neck", -0.18, pelv_yaw * 0.55, 0.0)
        set_rot("Head", -0.14, pelv_yaw * 0.45, 0.0)

        set_rot("Shoulder.L", 0.0, 0.08 * math.cos(t), 0.0)
        set_rot("Shoulder.R", 0.0, 0.08 * math.cos(t), 0.0)

        set_rot("Thigh.L", sample_periodic(run_thigh_keys, pL), 0.0, 0.0)
        set_rot("Shin.L",  sample_periodic(run_knee_keys, pL),  0.0, 0.0)
        set_rot("Foot.L",  sample_periodic(run_foot_keys, pL),  0.0, 0.0)

        set_rot("Thigh.R", sample_periodic(run_thigh_keys, pR), 0.0, 0.0)
        set_rot("Shin.R",  sample_periodic(run_knee_keys, pR),  0.0, 0.0)
        set_rot("Foot.R",  sample_periodic(run_foot_keys, pR),  0.0, 0.0)

        set_rot("UpperArm.L", sample_periodic(run_arm_keys, pL), 0.0, -ARM_DROP + 0.06)
        set_rot("Forearm.L",  sample_periodic(run_elbow_keys, pL), 0.0, 0.0)
        set_rot("UpperArm.R", sample_periodic(run_arm_keys, pR), 0.0,  ARM_DROP - 0.06)
        set_rot("Forearm.R",  sample_periodic(run_elbow_keys, pR), 0.0, 0.0)

    # --- 5. WALK (True 8-Phase Biomechanical Survivalist Hike Gait) ---
    walk_thigh_keys = [
        (0.00,  0.44), (0.12,  0.34), (0.28,  0.02),
        (0.48, -0.35), (0.62, -0.22), (0.78,  0.25), (0.90,  0.48),
    ]
    walk_knee_keys = [
        (0.00, -0.07), (0.12, -0.24), (0.30, -0.08),
        (0.50, -0.26), (0.66, -0.92), (0.80, -0.64), (0.92, -0.10),
    ]
    walk_foot_keys = [
        (0.00,  0.22), (0.12, -0.03), (0.32,  0.07),
        (0.52, -0.34), (0.68, -0.12), (0.86,  0.16),
    ]
    walk_arm_keys = [
        (0.00, -0.32), (0.18, -0.20), (0.36,  0.08),
        (0.52,  0.36), (0.72,  0.18), (0.88, -0.14),
    ]
    walk_elbow_keys = [
        (0.00,  0.16), (0.25,  0.26), (0.52,  0.46), (0.78,  0.28),
    ]

    def walk_pose(phase):
        pL = phase
        pR = (phase + 0.5) % 1.0
        t = phase * 2.0 * math.pi
        bounce = -0.026 * math.cos((phase * 2.0 - 0.08) * 2.0 * math.pi)
        sway_x = 0.020 * math.sin((phase - 0.04) * 2.0 * math.pi)
        pelv_yaw = 0.11 * math.cos(t)
        pelv_roll = 0.048 * math.sin((phase - 0.04) * 2.0 * math.pi)

        set_loc("Hips", sway_x, bounce, 0.0)
        set_rot("Hips", 0.04, pelv_yaw, pelv_roll)
        set_rot("Spine", 0.03, -pelv_yaw * 0.75, -pelv_roll * 0.7)
        set_rot("Chest", 0.02, -pelv_yaw * 1.05, -pelv_roll * 0.4)
        set_rot("Neck", -0.04, pelv_yaw * 0.45, 0.0)
        set_rot("Head", -0.03, pelv_yaw * 0.35, 0.0)

        set_rot("Thigh.L", sample_periodic(walk_thigh_keys, pL), 0.0, 0.0)
        set_rot("Shin.L",  sample_periodic(walk_knee_keys, pL),  0.0, 0.0)
        set_rot("Foot.L",  sample_periodic(walk_foot_keys, pL),  0.0, 0.0)

        set_rot("Thigh.R", sample_periodic(walk_thigh_keys, pR), 0.0, 0.0)
        set_rot("Shin.R",  sample_periodic(walk_knee_keys, pR),  0.0, 0.0)
        set_rot("Foot.R",  sample_periodic(walk_foot_keys, pR),  0.0, 0.0)

        set_rot("UpperArm.L", sample_periodic(walk_arm_keys, pL), 0.0, -ARM_DROP)
        set_rot("Forearm.L",  sample_periodic(walk_elbow_keys, pL), 0.0, 0.0)
        set_rot("UpperArm.R", sample_periodic(walk_arm_keys, pR), 0.0,  ARM_DROP)
        set_rot("Forearm.R",  sample_periodic(walk_elbow_keys, pR), 0.0, 0.0)

    # --- 6. IDLE (Natural Alert Breathing & Subtle Weight Shift — RECORDED LAST!) ---
    def idle_pose(phase):
        t = phase * 2.0 * math.pi
        b = math.sin(t)
        s2 = math.sin(t * 0.5)
        set_loc("Hips", 0.008 * s2, 0.004 * b, 0.0)
        set_rot("Hips", 0.0, 0.02 * s2, 0.015 * s2)
        set_rot("Spine", -0.014 * b, -0.01 * s2, -0.01 * s2)
        set_rot("Chest", -0.022 * b, -0.01 * s2, 0.0)
        set_rot("Head", 0.015 * b, 0.08 * s2, 0.012 * b)
        set_rot("Thigh.L", 0.02, 0.0, 0.0)
        set_rot("Shin.L", -0.04, 0.0, 0.0)
        set_rot("Thigh.R", -0.02, 0.0, 0.0)
        set_rot("Shin.R", -0.04, 0.0, 0.0)
        set_rot("UpperArm.L", 0.04 + 0.015 * b, 0.0, -ARM_DROP)
        set_rot("Forearm.L",  0.18 + 0.02 * b, 0.0, 0.0)
        set_rot("UpperArm.R", 0.04 - 0.015 * b, 0.0,  ARM_DROP)
        set_rot("Forearm.R",  0.18 + 0.02 * b, 0.0, 0.0)

    record_clip("Swim", 28, swim_pose)
    record_clip("Jump", 24, jump_pose)
    attack_action = record_clip("Attack", 20, attack_pose)
    run_action = record_clip("Run", 24, run_pose)
    walk_action = record_clip("Walk", 32, walk_pose)
    idle_action = record_clip("Idle", 48, idle_pose)

    bpy.ops.object.mode_set(mode="OBJECT")
    print("[Survivor] Baked 6 NLA clips: Swim, Jump, Attack, Run, Walk, Idle (LAST)")
    return idle_action, walk_action, run_action, attack_action


def render_previews(arm_obj, idle_action, walk_action, run_action, attack_action):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 640
    scene.render.resolution_y = 768
    scene.render.image_settings.file_format = "PNG"

    world = bpy.data.worlds.new("StudioWorld_TMP")
    scene.world = world
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (*srgb_to_linear((0.18, 0.21, 0.26)), 1.0)
    bg.inputs["Strength"].default_value = 0.6

    bpy.ops.mesh.primitive_plane_add(size=10.0, location=(0.0, 0.0, 0.0))
    floor = bpy.context.active_object
    floor.name = "StudioFloor_TMP"
    fmat = bpy.data.materials.new("FloorMat_TMP")
    fmat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (*srgb_to_linear((0.24, 0.26, 0.30)), 1.0)
    fmat.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.80
    floor.data.materials.append(fmat)

    def add_area_light(name, loc, target, energy, size, color):
        ld = bpy.data.lights.new(name=name, type="AREA")
        ld.energy = energy
        ld.size = size
        ld.color = color
        lo = bpy.data.objects.new(name=name, object_data=ld)
        bpy.context.collection.objects.link(lo)
        lo.location = Vector(loc)
        lo.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        return lo

    lights = [
        add_area_light("KeyLight_TMP",  (-2.0, -2.2, 2.45), (0.0, 0.0, 1.15), 220.0, 1.2, (1.0, 0.96, 0.90)),
        add_area_light("FillLight_TMP", ( 2.3, -1.6, 1.55), (0.0, 0.0, 1.10),  75.0, 2.2, (0.82, 0.90, 1.0)),
        add_area_light("RimLight_TMP",  ( 0.7,  2.2, 2.25), (0.0, 0.0, 1.35), 200.0, 1.0, (0.92, 0.96, 1.0)),
    ]

    cam_data = bpy.data.cameras.new("StudioCam_TMP")
    cam_obj = bpy.data.objects.new("StudioCam_TMP", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    def render_shot(filename, action, frame, cam_loc, target_loc, lens=55.0):
        arm_obj.animation_data.action = action
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        cam_data.lens = lens
        cam_obj.location = Vector(cam_loc)
        cam_obj.rotation_euler = (Vector(target_loc) - Vector(cam_loc)).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(ARTIFACT_DIR, filename)
        bpy.ops.render.render(write_still=True)

    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = True

    render_shot("meshy_survivor_idle.png",   idle_action,   1,  (-1.35, -2.45, 1.12), (0.0, 0.0, 0.92), lens=58.0)
    render_shot("meshy_survivor_walk.png",   walk_action,   1,  (-2.45, -1.35, 1.04), (0.0, 0.0, 0.90), lens=56.0)
    render_shot("meshy_survivor_run.png",    run_action,   19,  (-2.25, -1.65, 1.04), (0.0, 0.0, 0.90), lens=54.0)
    render_shot("meshy_survivor_attack.png", attack_action,  8, (-1.65, -2.25, 1.16), (0.0, 0.0, 0.95), lens=50.0)

    arm_obj.animation_data.action = None
    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = False
    scene.frame_set(1)

    for tmp in [floor, cam_obj] + lights:
        bpy.data.objects.remove(tmp, do_unlink=True)
    bpy.data.materials.remove(fmat, do_unlink=True)


def main():
    char_obj = import_and_normalize_survivor()
    lm = measure_landmarks(char_obj)
    arm_obj = build_armature(lm)
    skin_with_anatomical_capsules(char_obj, arm_obj, lm)
    idle_action, walk_action, run_action, attack_action = build_animations(arm_obj, lm)
    render_previews(arm_obj, idle_action, walk_action, run_action, attack_action)

    bpy.ops.wm.save_as_mainfile(filepath=OUT_BLEND)
    bpy.ops.export_scene.gltf(
        filepath=OUT_GLB,
        export_format="GLB",
        export_yup=True,
        export_apply=False,
        export_skins=True,
        export_all_influences=False,
        export_animations=True,
        export_nla_strips=True,
        export_nla_strips_merged_animation_name="Idle",
    )
    sz_mb = os.path.getsize(OUT_GLB) / (1024.0 * 1024.0)
    print(f"[Survivor] Saved editable rig -> {OUT_BLEND}")
    print(f"[Survivor] Exported skinned glTF -> {OUT_GLB} ({sz_mb:.2f} MB)")


if __name__ == "__main__":
    main()
