"""
SAGE Engine — Meshy Island Stag / Deer Rigging & Quadruped Animation Pipeline (Blender 5.3)
Takes raw Meshy .glb (`assets/models/GameReady_Deer.glb`), welds micro-doubles, normalizes to
life-size Island Red Stag proportions (~1.22m shoulder, ~2.08m antler crown, hooves at Z=0, facing -Y
-> +Z forward in glTF), measures exact leg/spine/neck/head/antler landmarks from the mesh, builds a
clean 25-bone SAGE-compatible quadruped armature (Look-At IK + Euphoria Ragdoll ready), applies
region-masked anatomical capsule skinning (locking branching antlers 100% rigidly to Head and strictly
isolating all 4 slender legs + 3x Laplacian joint smoothing), bakes 4 realistic NLA clips
(Run, Walk, Graze, Idle LAST), exports `assets/models/survival/island_deer.glb` +
`island_deer_rigged.blend`, and renders 4 Cycles studio previews.
"""

import bpy
import bmesh
import math
import os
from mathutils import Vector, Euler, Quaternion

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
SURVIVAL_DIR = os.path.join(MODELS_DIR, "survival")
ARTIFACT_DIR = r"C:\Users\DASUN\.gemini\antigravity\brain\2f9de14d-2f3f-473b-8005-216904e8d69d"

IN_GLB = os.path.join(MODELS_DIR, "GameReady_Deer.glb")
OUT_BLEND = os.path.join(SURVIVAL_DIR, "island_deer_rigged.blend")
OUT_GLB = os.path.join(SURVIVAL_DIR, "island_deer.glb")

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


def import_and_normalize_deer():
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
    deer_obj = bpy.context.view_layer.objects.active
    deer_obj.name = "IslandDeer_Mesh"

    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    for o in list(bpy.data.objects):
        if o != deer_obj:
            bpy.data.objects.remove(o, do_unlink=True)

    # Weld micro-doubles cleanly while preserving Meshy's original outward normals on layered neck fur cards!
    bm = bmesh.new()
    bm.from_mesh(deer_obj.data)
    before_v = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.00025)
    bm.to_mesh(deer_obj.data)
    bm.free()
    print(f"[Deer] Welded micro-doubles: {before_v} -> {len(deer_obj.data.vertices)} verts")

    # Downscale 2048px -> 1024px? Keep 2048px or check size (raw GLB is only 4.5MB — super fast & crisp!)
    verts = deer_obj.data.vertices
    min_y = min(v.co.y for v in verts)
    max_y = max(v.co.y for v in verts)
    min_z = min(v.co.z for v in verts)
    max_z = max(v.co.z for v in verts)

    raw_h = max_z - min_z
    # Mature Island Red Stag: ~2.06m antler-tip height (~1.20m shoulder/withers, ~1.80m nose-to-tail)
    target_h = 2.06
    scale_f = target_h / raw_h

    # Center X cleanly using hoof/leg centerline (ignoring asymmetric antler spread!)
    leg_xs = [v.co.x for v in verts if v.co.z < min_z + raw_h * 0.35]
    cx = sum(leg_xs) / max(len(leg_xs), 1)
    # Center Y around mid-torso between front & rear hooves
    hoof_ys = [v.co.y for v in verts if v.co.z < min_z + raw_h * 0.08]
    cy = 0.5 * (min(hoof_ys) + max(hoof_ys)) if hoof_ys else 0.5 * (min_y + max_y)

    for v in verts:
        v.co.x = (v.co.x - cx) * scale_f
        v.co.y = (v.co.y - cy) * scale_f
        v.co.z = (v.co.z - min_z) * scale_f

    deer_obj.data.update()
    for p in deer_obj.data.polygons:
        p.use_smooth = True

    for mat in deer_obj.data.materials:
        if mat and mat.node_tree:
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                if not bsdf.inputs["Roughness"].is_linked:
                    bsdf.inputs["Roughness"].default_value = max(bsdf.inputs["Roughness"].default_value, 0.78)
                if "Specular IOR Level" in bsdf.inputs and not bsdf.inputs["Specular IOR Level"].is_linked:
                    bsdf.inputs["Specular IOR Level"].default_value = 0.28

    nv = [v.co for v in deer_obj.data.vertices]
    print(f"[Deer] Normalized bbox: X[{min(v.x for v in nv):.3f}..{max(v.x for v in nv):.3f}] "
          f"Y[{min(v.y for v in nv):.3f}..{max(v.y for v in nv):.3f}] "
          f"Z[{min(v.z for v in nv):.3f}..{max(v.z for v in nv):.3f}] (scale={scale_f:.4f})")
    return deer_obj


def measure_deer_landmarks(deer_obj):
    verts = [v.co for v in deer_obj.data.vertices]

    def centroid(pts, default):
        if not pts:
            return Vector(default)
        s = Vector((0.0, 0.0, 0.0))
        for p in pts:
            s += p
        return s / len(pts)

    # Split hooves into Front (y < 0) and Rear (y > 0), Left (x > 0) and Right (x < 0)
    def leg_center(is_front, z_mid, half_dz=0.04):
        pts = [p for p in verts
               if abs(p.z - z_mid) <= half_dz
               and (p.y < 0.05 if is_front else p.y > 0.05)
               and p.x > 0.01]
        return centroid(pts, (0.11, -0.35 if is_front else 0.45, z_mid))

    # Measure front leg joints (hoof -> fetlock/knee -> elbow -> shoulder)
    f_hoof = leg_center(True, 0.06, 0.05)
    f_knee = leg_center(True, 0.44, 0.04)
    f_elbow = leg_center(True, 0.76, 0.04)
    f_sh = leg_center(True, 1.00, 0.05)
    f_sh.x = clamp(f_sh.x, 0.10, 0.16)

    # Measure rear leg joints (hoof -> hock -> stifle -> hip)
    r_hoof = leg_center(False, 0.06, 0.05)
    r_hock = leg_center(False, 0.50, 0.04)
    r_stifle = leg_center(False, 0.80, 0.04)
    r_hip = leg_center(False, 1.04, 0.05)
    r_hip.x = clamp(r_hip.x, 0.10, 0.17)

    # Measure torso top/bottom heights at chest and rump
    def torso_z_mid(y_mid, half_dy=0.07):
        pts = [p for p in verts if abs(p.y - y_mid) <= half_dy and p.z >= 0.72 and p.z <= 1.38]
        if not pts:
            return 1.06
        return 0.35 * min(p.z for p in pts) + 0.65 * max(p.z for p in pts)

    y_hips = r_hip.y + 0.04
    y_chest = f_sh.y - 0.02
    y_mid1 = y_hips * 0.65 + y_chest * 0.35
    y_mid2 = y_hips * 0.30 + y_chest * 0.70

    z_hips = torso_z_mid(y_hips)
    z_mid1 = torso_z_mid(y_mid1)
    z_mid2 = torso_z_mid(y_mid2)
    z_chest = torso_z_mid(y_chest)

    # Measure neck & head landmarks (y < y_chest - 0.05, z between 1.08 and 1.66 — below antlers!)
    neck_mid_pts = [p for p in verts if p.y < y_chest - 0.05 and 1.24 <= p.z <= 1.38]
    neck_mid = centroid(neck_mid_pts, (0.0, y_chest - 0.16, 1.31))
    neck_mid.x = 0.0

    # Head pivot (atlas/skull base behind jaw, below ears/antlers)
    head_base_pts = [p for p in verts if p.y < neck_mid.y - 0.04 and 1.44 <= p.z <= 1.56]
    head_base = centroid(head_base_pts, (0.0, neck_mid.y - 0.12, 1.49))
    head_base.x = 0.0

    # Nose tip (most -Y point below z=1.60)
    nose_pts = [p for p in verts if 1.25 <= p.z <= 1.58]
    nose_tip = min(nose_pts, key=lambda p: p.y) if nose_pts else Vector((0.0, head_base.y - 0.30, 1.40))
    nose_tip = Vector((0.0, nose_tip.y, nose_tip.z))

    # Tail base & tip (most +Y points in z in 0.85..1.25)
    rump_max_y = max(p.y for p in verts if 0.85 <= p.z <= 1.26)
    tail_pts = [p for p in verts if p.y > rump_max_y - 0.14 and 0.85 <= p.z <= 1.22]
    tail_base = Vector((0.0, rump_max_y - 0.14, clamp(z_hips + 0.02, 1.02, 1.22)))
    tail_tip = centroid(tail_pts, (0.0, rump_max_y, tail_base.z - 0.10))
    tail_tip.x = 0.0

    # Mid-belly Y split separating front legs from rear legs cleanly!
    y_leg_split = 0.5 * (f_hoof.y + r_hoof.y)

    lm = {
        "hips": Vector((0.0, y_hips, z_hips)),
        "spine1": Vector((0.0, y_mid1, z_mid1)),
        "spine2": Vector((0.0, y_mid2, z_mid2)),
        "chest": Vector((0.0, y_chest, z_chest)),
        "neck_base": Vector((0.0, y_chest - 0.06, z_chest + 0.04)),
        "neck_mid": neck_mid,
        "head_base": head_base,
        "nose_tip": nose_tip,
        "tail_base": tail_base,
        "tail_mid": (tail_base + tail_tip) * 0.5,
        "tail_tip": Vector((0.0, rump_max_y + 0.02, min(tail_tip.z, tail_base.z - 0.08))),
        "f_sh": f_sh,
        "f_elbow": f_elbow,
        "f_knee": f_knee,
        "f_hoof": Vector((f_hoof.x, f_hoof.y, 0.08)),
        "f_toe": Vector((f_hoof.x, f_hoof.y - 0.06, 0.01)),
        "r_hip": r_hip,
        "r_stifle": r_stifle,
        "r_hock": r_hock,
        "r_hoof": Vector((r_hoof.x, r_hoof.y, 0.08)),
        "r_toe": Vector((r_hoof.x, r_hoof.y - 0.06, 0.01)),
        "y_leg_split": y_leg_split,
        "z_belly": min(f_elbow.z, r_stifle.z) + 0.04,
    }

    for k, v in lm.items():
        if isinstance(v, Vector):
            print(f"  LM {k:11s}: ({v.x:+.3f}, {v.y:+.3f}, {v.z:+.3f})")
        else:
            print(f"  LM {k:11s}: {v:+.3f}")
    return lm


def build_deer_armature(lm):
    arm_data = bpy.data.armatures.new("IslandDeerArmature_Data")
    arm_obj = bpy.data.objects.new("IslandDeerArmature", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")

    eb = arm_data.edit_bones

    def add_bone(name, head, tail, parent_name=None, connected=False):
        b = eb.new(name)
        b.head = Vector(head)
        b.tail = Vector(tail)
        if (b.tail - b.head).length < 0.02:
            b.tail = b.head + Vector((0.0, -0.05, 0.0))
        if parent_name:
            b.parent = eb[parent_name]
            b.use_connect = connected
        return b

    # Spine & Head chain (exact SAGE classifyBoneRole names: Hips, Spine, Chest, Neck, Head!)
    add_bone("Hips",  lm["hips"],      lm["spine1"],    None,    False)
    add_bone("Spine", lm["spine1"],    lm["spine2"],    "Hips",  True)
    add_bone("Chest", lm["spine2"],    lm["neck_base"], "Spine", True)
    add_bone("Neck",  lm["neck_base"], lm["neck_mid"],  "Chest", True)
    add_bone("Neck1", lm["neck_mid"],  lm["head_base"], "Neck",  True)
    add_bone("Head",  lm["head_base"], lm["nose_tip"],  "Neck1", True)

    # White-tipped alarm tail!
    add_bone("Tail",    lm["tail_base"], lm["tail_mid"], "Hips", False)
    add_bone("TailTip", lm["tail_mid"],  lm["tail_tip"], "Tail", True)

    def mirror_x(v, side):
        return Vector((abs(v.x) * side, v.y, v.z))

    for side, sfx in ((1.0, "L"), (-1.0, "R")):
        # Front leg chain
        fs = mirror_x(lm["f_sh"], side)
        fe = mirror_x(lm["f_elbow"], side)
        fk = mirror_x(lm["f_knee"], side)
        fh = mirror_x(lm["f_hoof"], side)
        ft = mirror_x(lm["f_toe"], side)

        add_bone(f"FrontUpper.{sfx}", fs, fe, "Chest",            False)
        add_bone(f"FrontMid.{sfx}",   fe, fk, f"FrontUpper.{sfx}", True)
        add_bone(f"FrontLower.{sfx}", fk, fh, f"FrontMid.{sfx}",   True)
        add_bone(f"FrontHoof.{sfx}",  fh, ft, f"FrontLower.{sfx}", True)

        # Rear leg chain
        rh = mirror_x(lm["r_hip"], side)
        rs = mirror_x(lm["r_stifle"], side)
        rk = mirror_x(lm["r_hock"], side)
        rf = mirror_x(lm["r_hoof"], side)
        rt = mirror_x(lm["r_toe"], side)

        add_bone(f"RearThigh.{sfx}", rh, rs, "Hips",             False)
        add_bone(f"RearShin.{sfx}",  rs, rk, f"RearThigh.{sfx}", True)
        add_bone(f"RearHock.{sfx}",  rk, rf, f"RearShin.{sfx}",  True)
        add_bone(f"RearHoof.{sfx}",  rf, rt, f"RearHock.{sfx}",  True)

    bpy.ops.object.mode_set(mode="OBJECT")
    print(f"[Deer] Created {len(arm_data.bones)}-bone anatomical quadruped stag armature")
    return arm_obj


def skin_deer_anatomically(deer_obj, arm_obj, lm):
    """
    Region-masked anatomical capsule skinning + 3x Laplacian joint smoothing!
    Guarantees:
      - Antlers & Skull are 100% locked to Head (ZERO antler bending or warping!)
      - Neck & dark ruff bend smoothly all the way down to the ground for grazing!
      - All 4 slender legs are strictly isolated (Front.L, Front.R, Rear.L, Rear.R) — ZERO cross-leg bleed!
      - Tail flags cleanly upward during fleeing gallop!
    """
    deer_obj.parent = arm_obj
    mod = deer_obj.modifiers.new(name="Armature", type="ARMATURE")
    mod.object = arm_obj
    mod.use_deform_preserve_volume = True

    deer_obj.vertex_groups.clear()
    vgroups = {b.name: deer_obj.vertex_groups.new(name=b.name) for b in arm_obj.data.bones}
    bone_segs = {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in arm_obj.data.bones}

    y_split = lm["y_leg_split"]
    z_belly = lm["z_belly"]
    y_neck_start = lm["neck_base"].y
    y_head_start = lm["head_base"].y
    z_head_base = lm["head_base"].z
    y_tail_start = lm["tail_base"].y - 0.02

    def add_w(wmap, bname, val):
        if val > 1e-5:
            wmap[bname] = wmap.get(bname, 0.0) + val

    for v in deer_obj.data.vertices:
        p = v.co
        wmap = {}
        sfx = "L" if p.x >= 0.0 else "R"

        # ------------------------------------------------------------------
        # REGION A: ANTLERS & SKULL (high above neck or forward of atlas!)
        # ------------------------------------------------------------------
        if p.z >= z_head_base + 0.06 or (p.y <= y_head_start - 0.03 and p.z >= 1.24):
            # 100% rigid Head on antlers & muzzle so antlers NEVER deform!
            t_jaw = smoothstep(y_head_start + 0.04, y_head_start - 0.06, p.y)
            t_hi = smoothstep(z_head_base - 0.04, z_head_base + 0.05, p.z)
            w_head = clamp(max(t_hi, t_jaw), 0.0, 1.0)
            add_w(wmap, "Head", w_head)
            add_w(wmap, "Neck1", 1.0 - w_head)

        # ------------------------------------------------------------------
        # REGION B: NECK & DARK MANE RUFF (forward of chest, above legs!)
        # ------------------------------------------------------------------
        elif p.y <= y_neck_start + 0.04 and p.z >= lm["f_elbow"].z + 0.08:
            # Smooth projection along Chest -> Neck -> Neck1 -> Head
            d_ch, _ = point_seg_dist(p, *bone_segs["Chest"])
            d_n0, _ = point_seg_dist(p, *bone_segs["Neck"])
            d_n1, _ = point_seg_dist(p, *bone_segs["Neck1"])
            d_hd, _ = point_seg_dist(p, *bone_segs["Head"])
            sig = 0.14
            add_w(wmap, "Chest", math.exp(-((d_ch / sig) ** 2)))
            add_w(wmap, "Neck",  math.exp(-((d_n0 / sig) ** 2)) * 1.15)
            add_w(wmap, "Neck1", math.exp(-((d_n1 / sig) ** 2)) * 1.15)
            add_w(wmap, "Head",  math.exp(-((d_hd / 0.11) ** 2)))

        # ------------------------------------------------------------------
        # REGION C: TAIL (behind rump y > y_tail_start)
        # ------------------------------------------------------------------
        elif p.y >= y_tail_start and 0.82 <= p.z <= 1.26 and abs(p.x) <= 0.14:
            t_tail = smoothstep(y_tail_start, y_tail_start + 0.08, p.y)
            t_tip = smoothstep(lm["tail_mid"].y, lm["tail_tip"].y, p.y)
            add_w(wmap, "Hips", 1.0 - t_tail)
            add_w(wmap, "Tail", t_tail * (1.0 - t_tip * 0.65))
            add_w(wmap, "TailTip", t_tail * t_tip * 0.65)

        # ------------------------------------------------------------------
        # REGION D: FOUR SLENDER LEGS (strictly split Front/Rear AND Left/Right!)
        # ------------------------------------------------------------------
        elif p.z <= z_belly + 0.14 and (
            (p.y < y_split and abs(p.y - lm["f_elbow"].y) < 0.24)
            or (p.y >= y_split and abs(p.y - lm["r_stifle"].y) < 0.28)
        ):
            is_front = (p.y < y_split)
            # Smooth blend into torso at upper shoulder/thigh (z between z_belly - 0.04 and z_belly + 0.14)
            torso_blend = smoothstep(z_belly - 0.02, z_belly + 0.14, p.z)
            leg_w = 1.0 - torso_blend * 0.85

            if is_front:
                add_w(wmap, "Chest", torso_blend * 0.85)
                z_k = lm["f_knee"].z
                z_e = lm["f_elbow"].z
                z_h = lm["f_hoof"].z
                w_hoof  = 1.0 - smoothstep(z_h - 0.02, z_h + 0.06, p.z)
                w_lower = smoothstep(z_h - 0.02, z_h + 0.06, p.z) * (1.0 - smoothstep(z_k - 0.07, z_k + 0.07, p.z))
                w_mid   = smoothstep(z_k - 0.07, z_k + 0.07, p.z) * (1.0 - smoothstep(z_e - 0.07, z_e + 0.07, p.z))
                w_upper = smoothstep(z_e - 0.07, z_e + 0.07, p.z)
                add_w(wmap, f"FrontUpper.{sfx}", leg_w * w_upper)
                add_w(wmap, f"FrontMid.{sfx}",   leg_w * w_mid)
                add_w(wmap, f"FrontLower.{sfx}", leg_w * w_lower)
                add_w(wmap, f"FrontHoof.{sfx}",  leg_w * w_hoof)
            else:
                add_w(wmap, "Hips", torso_blend * 0.85)
                z_hk = lm["r_hock"].z
                z_st = lm["r_stifle"].z
                z_hf = lm["r_hoof"].z
                w_hoof  = 1.0 - smoothstep(z_hf - 0.02, z_hf + 0.06, p.z)
                w_hock  = smoothstep(z_hf - 0.02, z_hf + 0.06, p.z) * (1.0 - smoothstep(z_hk - 0.08, z_hk + 0.08, p.z))
                w_shin  = smoothstep(z_hk - 0.08, z_hk + 0.08, p.z) * (1.0 - smoothstep(z_st - 0.08, z_st + 0.08, p.z))
                w_thigh = smoothstep(z_st - 0.08, z_st + 0.08, p.z)
                add_w(wmap, f"RearThigh.{sfx}", leg_w * w_thigh)
                add_w(wmap, f"RearShin.{sfx}",  leg_w * w_shin)
                add_w(wmap, f"RearHock.{sfx}",  leg_w * w_hock)
                add_w(wmap, f"RearHoof.{sfx}",  leg_w * w_hoof)

        # ------------------------------------------------------------------
        # REGION E: BARREL TORSO (Hips -> Spine -> Chest along Y)
        # ------------------------------------------------------------------
        else:
            y_h = lm["hips"].y
            y_s = lm["spine1"].y
            y_c = lm["chest"].y
            w_chest = smoothstep(y_s + 0.08, y_c - 0.04, p.y)
            w_hips = smoothstep(y_s - 0.10, y_h + 0.04, p.y)
            w_spine = clamp(1.0 - w_chest - w_hips, 0.0, 1.0)
            add_w(wmap, "Hips", w_hips)
            add_w(wmap, "Spine", w_spine)
            add_w(wmap, "Chest", w_chest)

        # Normalize top-4 bone weights per vertex (glTF 2.0 requirement!)
        items = sorted(wmap.items(), key=lambda kv: kv[1], reverse=True)[:4]
        tot = sum(w for _, w in items)
        if tot < 1e-6:
            vgroups["Spine"].add([v.index], 1.0, "REPLACE")
        else:
            for bname, w in items:
                vgroups[bname].add([v.index], w / tot, "REPLACE")

    # Smooth joints cleanly (3 passes), then enforce 100% rigid Head on upper antlers!
    bpy.ops.object.select_all(action="DESELECT")
    deer_obj.select_set(True)
    bpy.context.view_layer.objects.active = deer_obj
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.32, repeat=3)
    bpy.ops.object.mode_set(mode="OBJECT")

    # Lock upper antlers strictly 100% to Head after smoothing so tips stay razor-crisp!
    for v in deer_obj.data.vertices:
        if v.co.z >= z_head_base + 0.14:
            for g in list(v.groups):
                deer_obj.vertex_groups[g.group].remove([v.index])
            vgroups["Head"].add([v.index], 1.0, "REPLACE")

    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    deer_obj.data.validate(verbose=False)
    print(f"[Deer] Applied anatomical quadruped capsule skinning + 3x smoothing ({len(deer_obj.data.vertices)} verts)")


def build_deer_animations(arm_obj):
    bpy.ops.object.select_all(action="DESELECT")
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="POSE")

    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "QUATERNION"

    # World-space-to-bone-local rotation solver (Lesson L009 — 100% immune to bone roll quirks!)
    rest_rot = {b.name: b.matrix_local.to_3x3().normalized() for b in arm_obj.data.bones}
    rest_rot_inv = {name: m.inverted() for name, m in rest_rot.items()}

    def reset_pose():
        for pb in arm_obj.pose.bones:
            pb.location = (0.0, 0.0, 0.0)
            pb.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))

    def set_world_rot(bname, pitch_x=0.0, roll_y=0.0, yaw_z=0.0):
        """
        Right-hand rule around WORLD axes (deer faces -Y, up is +Z, left is +X):
          pitch_x > 0 : drops front (-Y) DOWN, swings downward limbs BACKWARD (+Y)!
          pitch_x < 0 : lifts front (-Y) UP, swings downward limbs FORWARD (-Y)!
          yaw_z   > 0 : turns nose LEFT (+X)
          yaw_z   < 0 : turns nose RIGHT (-X)
        """
        pb = arm_obj.pose.bones.get(bname)
        if not pb:
            return
        r_world = Euler((pitch_x, roll_y, yaw_z), "XYZ").to_matrix()
        r_local = rest_rot_inv[bname] @ r_world @ rest_rot[bname]
        pb.rotation_quaternion = r_local.to_quaternion()

    def set_world_loc(bname, dx=0.0, dy=0.0, dz=0.0):
        pb = arm_obj.pose.bones.get(bname)
        if not pb:
            return
        pb.location = rest_rot_inv[bname] @ Vector((dx, dy, dz))

    def kf_all(frame):
        for pb in arm_obj.pose.bones:
            pb.keyframe_insert(data_path="rotation_quaternion", frame=frame)
            if pb.name == "Hips":
                pb.keyframe_insert(data_path="location", frame=frame)

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

    def apply_front_leg(sfx, shoulder_fwd, elbow_bend, knee_bend, hoof_pitch):
        # shoulder_fwd > 0 swings leg FORWARD (-Y) -> pitch_x = -shoulder_fwd
        # elbow_bend > 0 bends lower foreleg BACKWARD (+Y) -> pitch_x = +elbow_bend
        # knee_bend > 0 bends carpal/shin BACKWARD (+Y) -> pitch_x = +knee_bend
        set_world_rot(f"FrontUpper.{sfx}", -shoulder_fwd, 0.0, 0.0)
        set_world_rot(f"FrontMid.{sfx}",    elbow_bend,   0.0, 0.0)
        set_world_rot(f"FrontLower.{sfx}",  knee_bend,    0.0, 0.0)
        set_world_rot(f"FrontHoof.{sfx}",  -hoof_pitch,   0.0, 0.0)

    def apply_rear_leg(sfx, hip_fwd, stifle_bend, hock_bend, hoof_pitch):
        # hip_fwd > 0 swings thigh FORWARD (-Y) -> pitch_x = -hip_fwd
        # stifle_bend > 0 flexes shin BACKWARD (+Y) -> pitch_x = +stifle_bend
        # hock_bend > 0 flexes metatarsus FORWARD (-Y) -> pitch_x = -hock_bend
        set_world_rot(f"RearThigh.{sfx}", -hip_fwd,      0.0, 0.0)
        set_world_rot(f"RearShin.{sfx}",   stifle_bend,  0.0, 0.0)
        set_world_rot(f"RearHock.{sfx}",  -hock_bend,    0.0, 0.0)
        set_world_rot(f"RearHoof.{sfx}",   hoof_pitch,   0.0, 0.0)

    def apply_neck_head(neck_down, head_nod, yaw_total=0.0, roll_total=0.0):
        # neck_down > 0 dips neck DOWN toward grass (+pitch_x)!
        # Slight +0.16 rad straightening yaw counteracts Meshy's slight natural right-turn!
        ny = yaw_total + 0.14
        set_world_rot("Neck",  neck_down * 0.56, roll_total * 0.3, ny * 0.35)
        set_world_rot("Neck1", neck_down * 0.44, roll_total * 0.3, ny * 0.35)
        set_world_rot("Head",  head_nod,         roll_total * 0.4, ny * 0.30)

    # =========================================================================
    # 1. RUN (Majestic Bounding Deer Gallop / Alarm Leap + White Tail Flag!)
    # =========================================================================
    gal_f_sh = [(0.00,  0.88), (0.20,  0.25), (0.40, -0.58), (0.60, -0.42), (0.80,  0.52)]
    gal_f_el = [(0.00,  0.10), (0.22,  0.24), (0.45,  0.18), (0.68,  0.88), (0.86,  0.40)]
    gal_f_kn = [(0.00,  0.08), (0.24,  0.06), (0.48,  0.44), (0.70,  1.08), (0.88,  0.30)]

    gal_r_hp = [(0.00, -0.72), (0.20, -0.36), (0.42,  0.58), (0.62,  0.76), (0.82, -0.10)]
    gal_r_st = [(0.00, -0.14), (0.24,  0.52), (0.48,  0.65), (0.68,  0.24), (0.88, -0.12)]
    gal_r_hk = [(0.00, -0.18), (0.26,  0.42), (0.50,  0.52), (0.72,  0.12), (0.90, -0.16)]

    def run_pose(phase):
        t = phase * 2.0 * math.pi
        pFL = phase
        pFR = (phase + 0.12) % 1.0
        pRL = (phase + 0.04) % 1.0
        pRR = (phase + 0.16) % 1.0

        spine_ext = math.cos(t)  # +1 = soaring airborne extension, -1 = coiled gather!
        bounce_z = 0.16 * math.sin(t + 0.55) + 0.16
        body_pitch = -0.12 * math.sin(t) - 0.06 * spine_ext

        set_world_loc("Hips", 0.0, 0.0, bounce_z)
        set_world_rot("Hips",  body_pitch, 0.03 * math.sin(t), 0.0)
        # Smooth organic spine arch/extend (negative pitch lifts chest during airborne extension — ZERO hump!)
        set_world_rot("Spine", -0.10 * spine_ext, 0.0, 0.0)
        set_world_rot("Chest", -0.08 * spine_ext, 0.0, 0.0)

        # Proud streamlined leaping neck & antlers swept gracefully back for sprint!
        apply_neck_head(0.14 + 0.08 * spine_ext, -0.18 - 0.06 * spine_ext, 0.0, 0.0)
        # Alarmed white-tailed deer tail FLAGGED HIGH UP!
        set_world_rot("Tail",    1.22 + 0.15 * math.sin(t), 0.0, 0.14 * math.sin(2.0 * t))
        set_world_rot("TailTip", 0.35, 0.0, 0.12 * math.cos(2.0 * t))

        apply_front_leg("L", sample_periodic(gal_f_sh, pFL), sample_periodic(gal_f_el, pFL), sample_periodic(gal_f_kn, pFL), 0.12)
        apply_front_leg("R", sample_periodic(gal_f_sh, pFR), sample_periodic(gal_f_el, pFR), sample_periodic(gal_f_kn, pFR), 0.12)
        apply_rear_leg("L",  sample_periodic(gal_r_hp, pRL), sample_periodic(gal_r_st, pRL), sample_periodic(gal_r_hk, pRL), 0.08)
        apply_rear_leg("R",  sample_periodic(gal_r_hp, pRR), sample_periodic(gal_r_st, pRR), sample_periodic(gal_r_hk, pRR), 0.08)

    # =========================================================================
    # 2. WALK (Graceful 4-Beat Woodland Stag Stride)
    # =========================================================================
    wlk_f_sh = [(0.00,  0.34), (0.24,  0.08), (0.50, -0.32), (0.72, -0.06), (0.88,  0.30)]
    wlk_f_el = [(0.00,  0.06), (0.26,  0.14), (0.52,  0.12), (0.74,  0.52), (0.90,  0.18)]
    wlk_f_kn = [(0.00,  0.04), (0.28,  0.04), (0.55,  0.22), (0.76,  0.66), (0.92,  0.14)]

    wlk_r_hp = [(0.00,  0.30), (0.25,  0.05), (0.52, -0.28), (0.74,  0.06), (0.90,  0.28)]
    wlk_r_st = [(0.00,  0.06), (0.28,  0.12), (0.55, -0.05), (0.76,  0.42), (0.92,  0.12)]
    wlk_r_hk = [(0.00,  0.06), (0.30,  0.08), (0.56, -0.10), (0.76,  0.36), (0.92,  0.10)]

    def walk_pose(phase):
        t = phase * 2.0 * math.pi
        pFL = phase
        pRR = (phase + 0.25) % 1.0
        pFR = (phase + 0.50) % 1.0
        pRL = (phase + 0.75) % 1.0

        bob_z = -0.024 * math.cos(2.0 * t) + 0.015
        sway_x = 0.016 * math.sin(t)
        yaw_s = 0.05 * math.sin(t)

        set_world_loc("Hips", sway_x, 0.0, bob_z)
        set_world_rot("Hips", 0.025 * math.sin(2.0 * t), 0.03 * math.sin(t), yaw_s)
        set_world_rot("Spine", 0.0, 0.0, -yaw_s * 0.6)
        set_world_rot("Chest", 0.0, 0.0,  yaw_s * 0.5)

        apply_neck_head(0.06 + 0.05 * math.cos(2.0 * t), -0.04 - 0.03 * math.cos(2.0 * t), -yaw_s * 0.5, 0.0)
        set_world_rot("Tail", 0.14 + 0.08 * math.sin(2.0 * t), 0.0, 0.20 * math.sin(t))

        apply_front_leg("L", sample_periodic(wlk_f_sh, pFL), sample_periodic(wlk_f_el, pFL), sample_periodic(wlk_f_kn, pFL), 0.04)
        apply_front_leg("R", sample_periodic(wlk_f_sh, pFR), sample_periodic(wlk_f_el, pFR), sample_periodic(wlk_f_kn, pFR), 0.04)
        apply_rear_leg("L",  sample_periodic(wlk_r_hp, pRL), sample_periodic(wlk_r_st, pRL), sample_periodic(wlk_r_hk, pRL), 0.04)
        apply_rear_leg("R",  sample_periodic(wlk_r_hp, pRR), sample_periodic(wlk_r_st, pRR), sample_periodic(wlk_r_hk, pRR), 0.04)

    # =========================================================================
    # 3. GRAZE (Peaceful Woodland Grazing — Level Back, Grounded Hooves, Graceful Neck Dip!)
    # =========================================================================
    grz_neck = [(0.00, 0.96), (0.22, 1.24), (0.38, 1.08), (0.56, 1.26), (0.76, 1.06), (0.90, 0.98)]
    grz_head = [(0.00, 0.14), (0.22, 0.28), (0.38, 0.12), (0.56, 0.30), (0.76, 0.16), (0.90, 0.14)]

    def graze_pose(phase):
        t = phase * 2.0 * math.pi
        b = math.sin(t)
        nd = sample_periodic(grz_neck, phase)
        hn = sample_periodic(grz_head, phase)

        # Keep back natural & level so ALL FOUR hooves stay planted cleanly on Z=0.0 with ZERO hump!
        set_world_loc("Hips", 0.0, 0.0, 0.012 + 0.006 * b)
        set_world_rot("Hips",  0.015, 0.0, 0.0)
        set_world_rot("Spine", 0.020, 0.0, 0.0)
        set_world_rot("Chest", 0.045, 0.0, 0.0)

        apply_neck_head(nd, hn, 0.14 * math.sin(t * 0.5), 0.04 * math.sin(t))
        flick = 0.35 * (max(0.0, math.sin(t * 2.0 - 0.8)) ** 6)
        set_world_rot("Tail", 0.10 + flick * 0.3, 0.0, 0.38 * math.sin(t * 2.0) * (0.25 + flick))

        # Natural staggered grazing stance (hooves grounded cleanly!)
        apply_front_leg("L",  0.14, 0.04, 0.02, 0.0)
        apply_front_leg("R", -0.06, 0.02, 0.02, 0.0)
        apply_rear_leg("L",  -0.04, 0.04, 0.02, 0.0)
        apply_rear_leg("R",   0.04, 0.02, 0.02, 0.0)

    # =========================================================================
    # 4. IDLE (Proud Alert Woodland Stag — Breathing, Antlered Head Scan & Tail Flick — LAST!)
    # =========================================================================
    def idle_pose(phase):
        t = phase * 2.0 * math.pi
        b = math.sin(t)
        s2 = math.sin(t * 0.5)

        set_world_loc("Hips", 0.0, 0.0, 0.008 * b)
        set_world_rot("Hips",  0.012 * b, 0.0, 0.0)
        set_world_rot("Spine", -0.018 * b, 0.0, 0.0)
        set_world_rot("Chest", -0.014 * b, 0.0, 0.0)

        # Regal alert head scan left & right!
        apply_neck_head(-0.04 + 0.03 * b, 0.02 * b, 0.22 * s2, 0.04 * s2)
        flick = 0.28 * (max(0.0, math.sin(t * 2.0 - 1.1)) ** 6)
        set_world_rot("Tail", 0.08 + 0.14 * flick, 0.0, 0.22 * math.sin(t) + 0.35 * flick)

    run_action   = record_clip("Run",   20, run_pose)
    walk_action  = record_clip("Walk",  32, walk_pose)
    graze_action = record_clip("Graze", 48, graze_pose)
    idle_action  = record_clip("Idle",  48, idle_pose)

    bpy.ops.object.mode_set(mode="OBJECT")
    print("[Deer] Baked 4 NLA clips: Run, Walk, Graze, Idle (LAST)")
    return idle_action, graze_action, walk_action, run_action


def render_deer_previews(arm_obj, idle_action, graze_action, walk_action, run_action):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 768
    scene.render.resolution_y = 700
    scene.render.image_settings.file_format = "PNG"

    world = bpy.data.worlds.new("StudioWorld_TMP")
    scene.world = world
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (*srgb_to_linear((0.18, 0.21, 0.26)), 1.0)
    bg.inputs["Strength"].default_value = 0.65

    bpy.ops.mesh.primitive_plane_add(size=12.0, location=(0.0, 0.0, 0.0))
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
        add_area_light("KeyLight_TMP",  (-2.4, -2.6, 3.0), (0.0, 0.0, 1.05), 320.0, 1.4, (1.0, 0.96, 0.90)),
        add_area_light("FillLight_TMP", ( 2.8, -1.8, 1.8), (0.0, 0.0, 1.00), 110.0, 2.4, (0.82, 0.90, 1.0)),
        add_area_light("RimLight_TMP",  ( 0.8,  2.8, 2.8), (0.0, 0.0, 1.25), 260.0, 1.2, (0.92, 0.96, 1.0)),
    ]

    cam_data = bpy.data.cameras.new("StudioCam_TMP")
    cam_obj = bpy.data.objects.new("StudioCam_TMP", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    def render_shot(filename, action, frame, cam_loc, target_loc, lens=44.0):
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

    render_shot("meshy_deer_idle.png",  idle_action,   1, (-2.35, -2.75, 1.35), (0.0, -0.06, 0.98), lens=44.0)
    render_shot("meshy_deer_graze.png", graze_action, 12, ( 3.65, -1.15, 1.05), (0.0, -0.26, 0.78), lens=38.0)
    render_shot("meshy_deer_walk.png",  walk_action,   1, ( 3.45, -0.45, 1.15), (0.0,  0.00, 0.98), lens=44.0)
    render_shot("meshy_deer_run.png",   run_action,    1, ( 3.85, -0.95, 1.25), (0.0,  0.00, 1.08), lens=38.0)

    arm_obj.animation_data.action = None
    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = False
    scene.frame_set(1)

    for tmp in [floor, cam_obj] + lights:
        bpy.data.objects.remove(tmp, do_unlink=True)
    bpy.data.materials.remove(fmat, do_unlink=True)


def main():
    deer_obj = import_and_normalize_deer()
    lm = measure_deer_landmarks(deer_obj)
    arm_obj = build_deer_armature(lm)
    skin_deer_anatomically(deer_obj, arm_obj, lm)
    idle_action, graze_action, walk_action, run_action = build_deer_animations(arm_obj)
    render_deer_previews(arm_obj, idle_action, graze_action, walk_action, run_action)

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
    print(f"[Deer] Saved editable rig -> {OUT_BLEND}")
    print(f"[Deer] Exported skinned glTF -> {OUT_GLB} ({sz_mb:.2f} MB)")


if __name__ == "__main__":
    main()
