"""
SAGE Engine — Meshy Forest Raider Rigging & Aggressive Combat Animation Pipeline (Blender 5.3)
Takes raw Meshy .glb (`assets/models/GameReady_Scavenger_W.glb`), removes the stray floating web
polygon stretching between left hand & hip machete scabbard, welds micro-doubles, normalizes to
1.84m intimidating marauder height (feet at Z=0, facing -Y -> +Z glTF forward), rigs with a clean
19-bone SAGE humanoid armature (Look-At IK + Euphoria Ragdoll ready), protects hood/skull mask,
belt pouches, and machete scabbard from arm-swing stretching, bakes 5 combat NLA clips
(Jump, Attack, Run, Walk, Idle LAST), exports `assets/models/survival/forest_raider.glb` +
`forest_raider_rigged.blend`, and renders 4 Cycles studio previews.
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

IN_GLB = os.path.join(MODELS_DIR, "GameReady_Scavenger_W.glb")
OUT_BLEND = os.path.join(SURVIVAL_DIR, "forest_raider_rigged.blend")
OUT_GLB = os.path.join(SURVIVAL_DIR, "forest_raider.glb")

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


def import_clean_and_normalize_raider():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=IN_GLB)
    bpy.context.view_layer.update()

    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    char_obj = bpy.context.view_layer.objects.active
    char_obj.name = "ForestRaider_Mesh"

    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in list(bpy.data.objects):
        if o != char_obj:
            bpy.data.objects.remove(o, do_unlink=True)

    # 1. Scale & center FIRST so we can precisely snip the stray web between left hand & machete scabbard!
    verts = char_obj.data.vertices
    min_x = min(v.co.x for v in verts)
    max_x = max(v.co.x for v in verts)
    min_z = min(v.co.z for v in verts)
    max_z = max(v.co.z for v in verts)

    raw_h = max_z - min_z
    target_h = 1.84  # Slightly taller, imposing hooded marauder!
    scale_f = target_h / raw_h
    cx = 0.5 * (min_x + max_x)
    leg_ys = [v.co.y for v in verts if min_z + raw_h * 0.12 <= v.co.z <= min_z + raw_h * 0.45]
    cy = sum(leg_ys) / max(len(leg_ys), 1)

    for v in verts:
        v.co.x = (v.co.x - cx) * scale_f
        v.co.y = (v.co.y - cy) * scale_f
        v.co.z = (v.co.z - min_z) * scale_f
    char_obj.data.update()

    # 2. Clean stray stretched spider-web triangles between left hand (+X ~0.48) and left hip scabbard (+X ~0.24)
    #    AND weld micro-doubles cleanly!
    bm = bmesh.new()
    bm.from_mesh(char_obj.data)
    before_v = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)

    # Remove thin elongated web triangles bridging across the empty gap between hand and scabbard!
    bad_faces = []
    for f in bm.faces:
        c = f.calc_center_median()
        max_e = max(e.calc_length() for e in f.edges)
        xs = [v.co.x for v in f.verts]
        # Any face in the open armpit/hand-to-scabbard air gap (x in 0.24..0.52, z in 0.65..1.05) with a stretched edge > 0.065m!
        if 0.22 <= c.x <= 0.52 and 0.62 <= c.z <= 1.06 and max_e > 0.065:
            bad_faces.append(f)
        # Also catch any face whose center sits squarely in the empty air gap between scabbard (x<0.31) and hand (x>0.44)
        elif 0.305 <= c.x <= 0.445 and 0.64 <= c.z <= 0.98 and max_e > 0.025:
            bad_faces.append(f)
        # Also remove any single-triangle spike bridging wide X span (>0.12m) below waist
        elif (max(xs) - min(xs)) > 0.11 and c.z < 1.10 and c.x > 0.18:
            bad_faces.append(f)

    if bad_faces:
        bmesh.ops.delete(bm, geom=bad_faces, context="FACES")
        # Remove isolated loose vertices left behind
        loose_v = [v for v in bm.verts if not v.link_faces]
        if loose_v:
            bmesh.ops.delete(bm, geom=loose_v, context="VERTS")
        print(f"[Raider] Removed {len(bad_faces)} stray hand-to-scabbard web face(s) & {len(loose_v)} loose vert(s)!")

    bm.to_mesh(char_obj.data)
    bm.free()

    for p in char_obj.data.polygons:
        p.use_smooth = True

    for mat in char_obj.data.materials:
        if mat and mat.node_tree:
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                if not bsdf.inputs["Roughness"].is_linked:
                    bsdf.inputs["Roughness"].default_value = max(bsdf.inputs["Roughness"].default_value, 0.74)
                if "Specular IOR Level" in bsdf.inputs and not bsdf.inputs["Specular IOR Level"].is_linked:
                    bsdf.inputs["Specular IOR Level"].default_value = 0.34

    print(f"[Raider] Clean normalized mesh: {before_v} -> {len(char_obj.data.vertices)} verts, height={target_h:.2f}m")
    return char_obj


def measure_raider_landmarks(char_obj):
    verts = [v.co for v in char_obj.data.vertices]

    def centroid(pts, default):
        if not pts:
            return Vector(default)
        s = Vector((0.0, 0.0, 0.0))
        for p in pts:
            s += p
        return s / len(pts)

    # Use RIGHT side (x < 0) for legs/arms symmetry so left-hip machete scabbard doesn't skew leg centroid!
    def leg_slice_center_R(z_mid, half_dz=0.025):
        pts = [p for p in verts if abs(p.z - z_mid) <= half_dz and -0.26 <= p.x <= -0.02]
        c = centroid(pts, (-0.11, 0.0, z_mid))
        return Vector((abs(c.x), c.y, c.z))

    ankle_L = leg_slice_center_R(0.11)
    knee_L = leg_slice_center_R(0.50)
    hip_L = leg_slice_center_R(0.89)
    toe_pts = [p for p in verts if p.z <= 0.07 and p.x <= -0.02 and p.y <= -0.06]
    toe_c = centroid(toe_pts, (-ankle_L.x, -0.15, 0.03))

    # Measure RIGHT arm centerline (free of scabbard) then mirror cleanly!
    sh_pts = [p for p in verts if -0.24 <= p.x <= -0.16 and 1.38 <= p.z <= 1.52]
    sh_R = centroid(sh_pts, (-0.19, 0.0, 1.44))
    shoulder_L = Vector((clamp(abs(sh_R.x), 0.175, 0.205), clamp(sh_R.y, -0.02, 0.03), clamp(sh_R.z, 1.41, 1.47)))

    arm_pts_R = [p for p in verts if p.x <= -0.24 and 0.74 <= p.z <= 1.46]
    max_arm_x = max((abs(p.x) for p in arm_pts_R), default=0.54)

    def arm_at_ax(ax_target, half_dx=0.028):
        pts = [p for p in arm_pts_R if abs(abs(p.x) - ax_target) <= half_dx]
        c = centroid(pts, (-ax_target, 0.0, 1.15))
        return Vector((abs(c.x), c.y, c.z))

    elbow_L = arm_at_ax(shoulder_L.x + (max_arm_x - shoulder_L.x) * 0.45)
    wrist_L = arm_at_ax(shoulder_L.x + (max_arm_x - shoulder_L.x) * 0.78)
    hand_tip_L = arm_at_ax(max_arm_x - 0.015, half_dx=0.03)

    def torso_y(z_mid, half_dz=0.04):
        pts = [p for p in verts if abs(p.z - z_mid) <= half_dz and abs(p.x) <= 0.14]
        return 0.5 * (min(p.y for p in pts) + max(p.y for p in pts)) if pts else 0.0

    lm = {
        "hips_head": Vector((0.0, clamp(torso_y(0.95), -0.03, 0.03), 0.94)),
        "spine_head": Vector((0.0, clamp(torso_y(1.12), -0.03, 0.03), 1.11)),
        "chest_head": Vector((0.0, clamp(torso_y(1.32), -0.03, 0.03), 1.30)),
        "neck_head": Vector((0.0, clamp(torso_y(1.50), -0.03, 0.03), 1.49)),
        "head_head": Vector((0.0, clamp(torso_y(1.62), -0.03, 0.03), 1.59)),
        "head_tail": Vector((0.0, clamp(torso_y(1.74), -0.03, 0.03), 1.83)),
        "shoulder_L": shoulder_L,
        "elbow_L": elbow_L,
        "wrist_L": wrist_L,
        "hand_tip_L": hand_tip_L,
        "hip_L": Vector((clamp(hip_L.x, 0.095, 0.125), clamp(hip_L.y, -0.02, 0.03), 0.915)),
        "knee_L": Vector((clamp(knee_L.x, 0.095, 0.130), clamp(knee_L.y - 0.008, -0.03, 0.03), 0.505)),
        "ankle_L": Vector((clamp(ankle_L.x, 0.095, 0.135), clamp(ankle_L.y, -0.02, 0.03), 0.110)),
        "toe_L": Vector((clamp(ankle_L.x, 0.095, 0.135), min(toe_c.y, -0.14), 0.025)),
    }
    for k, v in lm.items():
        print(f"  LM {k:12s}: ({v.x:+.3f}, {v.y:+.3f}, {v.z:+.3f})")
    return lm


def build_raider_armature(lm):
    arm_data = bpy.data.armatures.new("ForestRaiderArmature_Data")
    arm_obj = bpy.data.objects.new("ForestRaiderArmature", arm_data)
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
    print(f"[Raider] Created {len(arm_data.bones)}-bone anatomical armature")
    return arm_obj


def skin_raider_anatomically(char_obj, arm_obj, lm):
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
    arm_slope = (el_z - sh_z) / max(el_x - sh_x, 0.05)

    def is_arm_vertex(p):
        ax = abs(p.x)
        # Keep hip belt pouches AND left-hip machete scabbard (x > 0, z < 1.06, x < 0.36) strictly OUT of arms!
        if p.z < 1.10 and ax < 0.36:
            return False
        if ax >= 0.255 and 0.72 <= p.z <= 1.54:
            return True
        if ax >= 0.195 and 1.28 <= p.z <= 1.54:
            expected_z = sh_z + (ax - sh_x) * arm_slope
            if abs(p.z - expected_z) < 0.145:
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

        # A: HOOD & SKULL MASK (z >= 1.50, |x| < 0.20)
        if p.z >= 1.50 and ax < 0.20:
            t_head = smoothstep(1.535, 1.595, p.z)
            t_neck = smoothstep(1.485, 1.535, p.z) * (1.0 - t_head)
            t_chest = 1.0 - smoothstep(1.485, 1.545, p.z)
            add_w(wmap, "Head", t_head)
            add_w(wmap, "Neck", t_neck)
            add_w(wmap, "Chest", t_chest)

        # B: ARMS, LEATHER WRAPS & HANDS
        elif is_arm_vertex(p):
            d_ua, _ = point_seg_dist(p, *bone_segs[f"UpperArm.{sfx}"])
            d_fa, _ = point_seg_dist(p, *bone_segs[f"Forearm.{sfx}"])
            d_ha, _ = point_seg_dist(p, *bone_segs[f"Hand.{sfx}"])
            w_ua = math.exp(-((d_ua / 0.072) ** 2))
            w_fa = math.exp(-((d_fa / 0.060) ** 2))
            w_ha = math.exp(-((d_ha / 0.056) ** 2))

            arm_blend = smoothstep(0.190, 0.260, ax)
            if arm_blend < 0.999:
                add_w(wmap, "Chest", (1.0 - arm_blend) * 0.72)
                add_w(wmap, f"Shoulder.{sfx}", (1.0 - arm_blend) * 0.28)
                add_w(wmap, f"UpperArm.{sfx}", arm_blend * max(w_ua, 0.2))
            else:
                add_w(wmap, f"UpperArm.{sfx}", w_ua * (1.0 if d_ua <= d_fa + 0.03 else 0.15))
                add_w(wmap, f"Forearm.{sfx}",  w_fa)
                add_w(wmap, f"Hand.{sfx}",     w_ha * (1.4 if d_ha < d_fa else 0.25))

        # C: LEFT-HIP MACHETE SCABBARD (rides cleanly with Hips + slight Thigh.L follow so it never bends!)
        elif p.x > 0.16 and p.x < 0.36 and 0.45 <= p.z <= 1.08 and p.y < 0.04:
            t_low = smoothstep(0.85, 0.55, p.z)
            add_w(wmap, "Hips", 0.72 * (1.0 - t_low * 0.45))
            add_w(wmap, "Thigh.L", 0.28 + 0.32 * t_low)

        # D: LEGS, RAGGED SKIRT TAILS & BOOTS
        elif p.z <= 0.915:
            pelvis_w = smoothstep(0.80, 0.915, p.z)
            crotch_w = smoothstep(0.76, 0.89, p.z) * (1.0 - smoothstep(0.03, 0.11, ax))
            hips_total = clamp(pelvis_w * 0.88 + crotch_w * 0.65, 0.0, 0.96)
            leg_total = 1.0 - hips_total

            z_knee = lm["knee_L"].z
            z_ankle = lm["ankle_L"].z
            w_foot = 1.0 - smoothstep(z_ankle - 0.025, z_ankle + 0.045, p.z)
            w_shin = smoothstep(z_ankle - 0.025, z_ankle + 0.045, p.z) * (1.0 - smoothstep(z_knee - 0.065, z_knee + 0.065, p.z))
            w_thigh = smoothstep(z_knee - 0.065, z_knee + 0.065, p.z)

            add_w(wmap, "Hips", hips_total)
            add_w(wmap, f"Thigh.{sfx}", leg_total * w_thigh)
            add_w(wmap, f"Shin.{sfx}",  leg_total * w_shin)
            add_w(wmap, f"Foot.{sfx}",  leg_total * w_foot)

        # E: TORSO, FUR MANTLE & BELT POUCHES
        else:
            w_hips = 1.0 - smoothstep(0.97, 1.11, p.z)
            w_chest = smoothstep(1.19, 1.33, p.z)
            w_spine = clamp(1.0 - w_hips - w_chest, 0.0, 1.0)
            w_neck = smoothstep(1.45, 1.51, p.z) * (1.0 - smoothstep(0.07, 0.12, ax)) * 0.45
            add_w(wmap, "Hips", w_hips)
            add_w(wmap, "Spine", w_spine)
            add_w(wmap, "Chest", w_chest * (1.0 - w_neck))
            add_w(wmap, "Neck", w_neck)

        items = sorted(wmap.items(), key=lambda kv: kv[1], reverse=True)[:4]
        tot = sum(w for _, w in items)
        if tot < 1e-6:
            vgroups["Chest"].add([v.index], 1.0, "REPLACE")
        else:
            for bname, w in items:
                vgroups[bname].add([v.index], w / tot, "REPLACE")

    bpy.ops.object.select_all(action="DESELECT")
    char_obj.select_set(True)
    bpy.context.view_layer.objects.active = char_obj
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.35, repeat=3)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    char_obj.data.validate(verbose=False)
    print(f"[Raider] Applied anatomical capsule skinning + 3x smoothing ({len(char_obj.data.vertices)} verts)")


def build_raider_animations(arm_obj, lm):
    bpy.ops.object.select_all(action="DESELECT")
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="POSE")

    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "XYZ"

    arm_vec = lm["elbow_L"] - lm["shoulder_L"]
    rest_abduction = math.atan2(abs(arm_vec.x), max(0.05, -arm_vec.z))
    # Clear the hip machete & pouches naturally (~14 deg outward)
    ARM_DROP = max(0.14, rest_abduction - 0.24)
    print(f"[Raider] A-pose abduction={math.degrees(rest_abduction):.1f} deg -> ARM_DROP={math.degrees(ARM_DROP):.1f} deg")

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

    # --- 1. JUMP ---
    def jump_pose(phase):
        t = phase * 2.0 * math.pi
        sway = math.sin(t)
        set_loc("Hips", 0.0, 0.04 + 0.015 * sway, 0.0)
        set_rot("Hips", 0.14, -0.06, 0.0)
        set_rot("Spine", 0.10, 0.04, 0.0)
        set_rot("Chest", 0.08, 0.06, 0.0)
        set_rot("Neck", -0.14, -0.04, 0.0)
        set_rot("Head", -0.10, -0.04, 0.0)
        set_rot("Thigh.L", 0.62 + 0.06 * sway, 0.0, 0.0)
        set_rot("Shin.L", -1.10 - 0.08 * sway, 0.0, 0.0)
        set_rot("Thigh.R", -0.18 - 0.05 * sway, 0.0, 0.0)
        set_rot("Shin.R", -0.78 + 0.06 * sway, 0.0, 0.0)
        set_rot("UpperArm.L", 0.55, 0.0, -ARM_DROP + 0.25)
        set_rot("Forearm.L",  0.88, 0.0, 0.0)
        set_rot("UpperArm.R", 0.45, 0.0,  ARM_DROP - 0.25)
        set_rot("Forearm.R",  0.95, 0.0, 0.0)

    # --- 2. ATTACK (Brutal Raider Diagonal Cleave Lunge!) ---
    atk_armR_x = [(0.00, 0.20), (0.26, 2.15), (0.48, 0.90), (0.68, -0.40), (0.88, 0.12)]
    atk_armR_z = [(0.00, ARM_DROP - 0.05), (0.26, ARM_DROP - 0.46), (0.48, ARM_DROP + 0.10), (0.68, ARM_DROP + 0.08), (0.88, ARM_DROP - 0.03)]
    atk_foreR_x = [(0.00, 0.36), (0.26, 1.15), (0.48, 0.24), (0.68, 0.52), (0.88, 0.30)]
    atk_tyaw = [(0.00, 0.0), (0.26, -0.52), (0.48, 0.48), (0.68, 0.28), (0.88, 0.04)]
    atk_tpitch = [(0.00, 0.08), (0.26, -0.06), (0.48, 0.28), (0.68, 0.16), (0.88, 0.08)]

    def attack_pose(phase):
        tyaw = sample_periodic(atk_tyaw, phase)
        tpitch = sample_periodic(atk_tpitch, phase)
        lunge = math.sin(clamp((phase - 0.18) / 0.58, 0.0, 1.0) * math.pi)
        set_loc("Hips", 0.0, -0.065 * lunge, -0.03 * lunge)
        set_rot("Hips",  0.12 + 0.08 * lunge, tyaw * 0.45, 0.0)
        set_rot("Spine", tpitch * 0.55, tyaw * 0.55, 0.0)
        set_rot("Chest", tpitch * 0.65, tyaw * 0.72, 0.0)
        set_rot("Neck", -tpitch * 0.50, -tyaw * 0.55, 0.0)
        set_rot("Head", -tpitch * 0.45, -tyaw * 0.62, 0.0)
        set_rot("UpperArm.R", sample_periodic(atk_armR_x, phase), 0.12 * lunge, sample_periodic(atk_armR_z, phase))
        set_rot("Forearm.R",  sample_periodic(atk_foreR_x, phase), 0.0, 0.0)
        set_rot("UpperArm.L", 0.35 - 0.45 * lunge, 0.0, -ARM_DROP + 0.14)
        set_rot("Forearm.L",  0.80 + 0.22 * lunge, 0.0, 0.0)
        set_rot("Thigh.L",  0.38 * lunge + 0.10, 0.0, 0.0)
        set_rot("Shin.L",  -0.48 * lunge - 0.12, 0.0, 0.0)
        set_rot("Thigh.R", -0.32 * lunge - 0.06, 0.0, 0.0)
        set_rot("Shin.R",  -0.24 * lunge - 0.10, 0.0, 0.0)

    # --- 3. RUN (Hunched Predatory Raider Charge!) ---
    run_thigh = [(0.00, 0.64), (0.14, 0.18), (0.30, -0.58), (0.45, -0.72), (0.62, 0.14), (0.78, 0.96), (0.90, 0.80)]
    run_knee  = [(0.00, -0.30), (0.14, -0.56), (0.32, -0.14), (0.52, -1.70), (0.72, -1.26), (0.88, -0.46)]
    run_foot  = [(0.00, 0.16), (0.15, 0.20), (0.34, -0.48), (0.58, -0.22), (0.82, 0.18)]
    run_arm   = [(0.00, -0.68), (0.16, -0.40), (0.34, 0.20), (0.50, 0.82), (0.68, 0.50), (0.84, -0.16)]
    run_elbow = [(0.00, 1.22), (0.25, 1.38), (0.50, 1.66), (0.75, 1.42)]

    def run_pose(phase):
        pL, pR = phase, (phase + 0.5) % 1.0
        t = phase * 2.0 * math.pi
        bounce = -0.045 * math.sin((phase * 2.0 + 0.22) * 2.0 * math.pi) - 0.018
        sway_x = 0.018 * math.sin((phase - 0.14) * 2.0 * math.pi)
        pelv_yaw = 0.18 * math.cos(t)
        pelv_roll = 0.068 * math.sin((phase - 0.14) * 2.0 * math.pi)
        set_loc("Hips", sway_x, bounce, 0.0)
        set_rot("Hips",  0.22, pelv_yaw, pelv_roll)  # Extra menacing forward hunch!
        set_rot("Spine", 0.16, -pelv_yaw * 0.85, -pelv_roll * 0.6)
        set_rot("Chest", 0.14, -pelv_yaw * 1.15, -pelv_roll * 0.4)
        set_rot("Neck", -0.24, pelv_yaw * 0.55, 0.0)
        set_rot("Head", -0.18, pelv_yaw * 0.45, 0.0)
        set_rot("Thigh.L", sample_periodic(run_thigh, pL), 0.0, 0.0)
        set_rot("Shin.L",  sample_periodic(run_knee,  pL), 0.0, 0.0)
        set_rot("Foot.L",  sample_periodic(run_foot,  pL), 0.0, 0.0)
        set_rot("Thigh.R", sample_periodic(run_thigh, pR), 0.0, 0.0)
        set_rot("Shin.R",  sample_periodic(run_knee,  pR), 0.0, 0.0)
        set_rot("Foot.R",  sample_periodic(run_foot,  pR), 0.0, 0.0)
        set_rot("UpperArm.L", sample_periodic(run_arm, pL), 0.0, -ARM_DROP + 0.08)
        set_rot("Forearm.L",  sample_periodic(run_elbow, pL), 0.0, 0.0)
        set_rot("UpperArm.R", sample_periodic(run_arm, pR), 0.0,  ARM_DROP - 0.08)
        set_rot("Forearm.R",  sample_periodic(run_elbow, pR), 0.0, 0.0)

    # --- 4. WALK (Menacing Stalking Patrol Gait) ---
    walk_thigh = [(0.00, 0.42), (0.12, 0.32), (0.28, 0.02), (0.48, -0.34), (0.62, -0.22), (0.78, 0.24), (0.90, 0.46)]
    walk_knee  = [(0.00, -0.10), (0.12, -0.26), (0.30, -0.10), (0.50, -0.28), (0.66, -0.92), (0.80, -0.64), (0.92, -0.12)]
    walk_foot  = [(0.00, 0.20), (0.12, -0.03), (0.32, 0.07), (0.52, -0.32), (0.68, -0.12), (0.86, 0.16)]
    walk_arm   = [(0.00, -0.28), (0.18, -0.18), (0.36, 0.08), (0.52, 0.34), (0.72, 0.18), (0.88, -0.12)]
    walk_elbow = [(0.00, 0.28), (0.25, 0.38), (0.52, 0.56), (0.78, 0.38)]

    def walk_pose(phase):
        pL, pR = phase, (phase + 0.5) % 1.0
        t = phase * 2.0 * math.pi
        bounce = -0.025 * math.cos((phase * 2.0 - 0.08) * 2.0 * math.pi)
        sway_x = 0.022 * math.sin((phase - 0.04) * 2.0 * math.pi)
        pelv_yaw = 0.12 * math.cos(t)
        pelv_roll = 0.050 * math.sin((phase - 0.04) * 2.0 * math.pi)
        set_loc("Hips", sway_x, bounce, 0.0)
        set_rot("Hips",  0.09, pelv_yaw, pelv_roll)
        set_rot("Spine", 0.07, -pelv_yaw * 0.75, -pelv_roll * 0.7)
        set_rot("Chest", 0.06, -pelv_yaw * 1.05, -pelv_roll * 0.4)
        set_rot("Neck", -0.10, pelv_yaw * 0.45, 0.0)
        set_rot("Head", -0.08, pelv_yaw * 0.35, 0.0)
        set_rot("Thigh.L", sample_periodic(walk_thigh, pL), 0.0, 0.0)
        set_rot("Shin.L",  sample_periodic(walk_knee,  pL), 0.0, 0.0)
        set_rot("Foot.L",  sample_periodic(walk_foot,  pL), 0.0, 0.0)
        set_rot("Thigh.R", sample_periodic(walk_thigh, pR), 0.0, 0.0)
        set_rot("Shin.R",  sample_periodic(walk_knee,  pR), 0.0, 0.0)
        set_rot("Foot.R",  sample_periodic(walk_foot,  pR), 0.0, 0.0)
        set_rot("UpperArm.L", sample_periodic(walk_arm, pL), 0.0, -ARM_DROP + 0.04)
        set_rot("Forearm.L",  sample_periodic(walk_elbow, pL), 0.0, 0.0)
        set_rot("UpperArm.R", sample_periodic(walk_arm, pR), 0.0,  ARM_DROP - 0.04)
        set_rot("Forearm.R",  sample_periodic(walk_elbow, pR), 0.0, 0.0)

    # --- 5. IDLE (Menacing Hunched Marauder Breathing & Head Scan — LAST!) ---
    def idle_pose(phase):
        t = phase * 2.0 * math.pi
        b = math.sin(t)
        s2 = math.sin(t * 0.5)
        set_loc("Hips", 0.008 * s2, -0.015 + 0.005 * b, 0.0)
        set_rot("Hips",  0.07, 0.03 * s2, 0.015 * s2)
        set_rot("Spine", 0.05 - 0.016 * b, -0.02 * s2, 0.0)
        set_rot("Chest", 0.05 - 0.024 * b, -0.02 * s2, 0.0)
        set_rot("Neck", -0.08, 0.0, 0.0)
        set_rot("Head", -0.04 + 0.02 * b, 0.14 * s2, 0.02 * s2)
        set_rot("Thigh.L",  0.06, 0.0, 0.0)
        set_rot("Shin.L",  -0.12, 0.0, 0.0)
        set_rot("Thigh.R", -0.04, 0.0, 0.0)
        set_rot("Shin.R",  -0.08, 0.0, 0.0)
        set_rot("UpperArm.L", 0.12 + 0.02 * b, 0.0, -ARM_DROP + 0.04)
        set_rot("Forearm.L",  0.34 + 0.03 * b, 0.0, 0.0)
        set_rot("UpperArm.R", 0.14 - 0.02 * b, 0.0,  ARM_DROP - 0.04)
        set_rot("Forearm.R",  0.38 + 0.03 * b, 0.0, 0.0)

    record_clip("Jump", 24, jump_pose)
    attack_action = record_clip("Attack", 20, attack_pose)
    run_action    = record_clip("Run",    24, run_pose)
    walk_action   = record_clip("Walk",   32, walk_pose)
    idle_action   = record_clip("Idle",   48, idle_pose)

    bpy.ops.object.mode_set(mode="OBJECT")
    print("[Raider] Baked 5 NLA clips: Jump, Attack, Run, Walk, Idle (LAST)")
    return idle_action, walk_action, run_action, attack_action


def render_raider_previews(arm_obj, idle_action, walk_action, run_action, attack_action):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 22
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 640
    scene.render.resolution_y = 768
    scene.render.image_settings.file_format = "PNG"

    world = bpy.data.worlds.new("StudioWorld_TMP")
    scene.world = world
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (*srgb_to_linear((0.16, 0.18, 0.22)), 1.0)
    bg.inputs["Strength"].default_value = 0.6

    bpy.ops.mesh.primitive_plane_add(size=10.0, location=(0.0, 0.0, 0.0))
    floor = bpy.context.active_object
    fmat = bpy.data.materials.new("FloorMat_TMP")
    fmat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (*srgb_to_linear((0.22, 0.24, 0.28)), 1.0)
    fmat.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.80
    floor.data.materials.append(fmat)

    def add_area_light(name, loc, target, energy, size, color):
        ld = bpy.data.lights.new(name=name, type="AREA")
        ld.energy = energy; ld.size = size; ld.color = color
        lo = bpy.data.objects.new(name=name, object_data=ld)
        bpy.context.collection.objects.link(lo)
        lo.location = Vector(loc)
        lo.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        return lo

    lights = [
        add_area_light("KeyLight_TMP",  (-2.0, -2.2, 2.55), (0.0, 0.0, 1.15), 240.0, 1.2, (1.0, 0.94, 0.88)),
        add_area_light("FillLight_TMP", ( 2.3, -1.6, 1.55), (0.0, 0.0, 1.10),  80.0, 2.2, (0.80, 0.88, 1.0)),
        add_area_light("RimLight_TMP",  ( 0.7,  2.2, 2.35), (0.0, 0.0, 1.40), 210.0, 1.0, (0.92, 0.96, 1.0)),
    ]

    cam_data = bpy.data.cameras.new("StudioCam_TMP")
    cam_obj = bpy.data.objects.new("StudioCam_TMP", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    def render_shot(filename, action, frame, cam_loc, target_loc, lens=54.0):
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

    render_shot("meshy_raider_idle.png",   idle_action,   1, (-1.45, -2.55, 1.15), (0.0, 0.0, 0.94), lens=54.0)
    render_shot("meshy_raider_run.png",    run_action,   19, (-2.35, -1.70, 1.08), (0.0, 0.0, 0.94), lens=52.0)
    render_shot("meshy_raider_attack.png", attack_action, 8, (-1.65, -2.35, 1.18), (0.0, 0.0, 0.98), lens=48.0)

    arm_obj.animation_data.action = None
    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = False
    scene.frame_set(1)

    for tmp in [floor, cam_obj] + lights:
        bpy.data.objects.remove(tmp, do_unlink=True)
    bpy.data.materials.remove(fmat, do_unlink=True)


def main():
    char_obj = import_clean_and_normalize_raider()
    lm = measure_raider_landmarks(char_obj)
    arm_obj = build_raider_armature(lm)
    skin_raider_anatomically(char_obj, arm_obj, lm)
    idle_action, walk_action, run_action, attack_action = build_raider_animations(arm_obj, lm)
    render_raider_previews(arm_obj, idle_action, walk_action, run_action, attack_action)

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
    print(f"[Raider] Saved editable rig -> {OUT_BLEND}")
    print(f"[Raider] Exported skinned glTF -> {OUT_GLB} ({sz_mb:.2f} MB)")


if __name__ == "__main__":
    main()
