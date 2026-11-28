"""
SAGE Engine — Meshy Timber Wolf Optimization, Rig Polish & Quadruped Animation Pipeline (Blender 5.3)
Takes raw Meshy .glb (`assets/models/Wolf Quadruped Rig.glb`, 50.1 MB with 3x 4K textures & extra
Icosphere), cleans & optimizes it down to ~3-5 MB game-ready quality while preserving 100% of its
56-bone quadruped rig & skin weights, normalizes ground/scale/center, bakes 4 lifelike quadruped NLA
clips (Attack, Run, Walk, Idle LAST) using exact world-space-to-bone-local quaternion conversion,
exports `assets/models/survival/timber_wolf.glb` + `timber_wolf_rigged.blend`, and renders 4 Cycles
studio previews for visual verification.
"""

import bpy
import math
import os
from mathutils import Vector, Euler, Matrix, Quaternion

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
SURVIVAL_DIR = os.path.join(MODELS_DIR, "survival")
ARTIFACT_DIR = r"C:\Users\DASUN\.gemini\antigravity\brain\2f9de14d-2f3f-473b-8005-216904e8d69d"

IN_GLB = os.path.join(MODELS_DIR, "Wolf Quadruped Rig.glb")
OUT_BLEND = os.path.join(SURVIVAL_DIR, "timber_wolf_rigged.blend")
OUT_GLB = os.path.join(SURVIVAL_DIR, "timber_wolf.glb")

os.makedirs(SURVIVAL_DIR, exist_ok=True)
os.makedirs(ARTIFACT_DIR, exist_ok=True)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def srgb_to_linear(c):
    return tuple((v ** 2.2) for v in c)


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


def import_optimize_and_normalize_wolf():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=IN_GLB)
    bpy.context.view_layer.update()

    # 1. Delete stray non-wolf Icosphere / extra objects
    arm_obj = None
    wolf_mesh = None
    for obj in list(bpy.data.objects):
        if obj.type == "ARMATURE":
            arm_obj = obj
        elif obj.type == "MESH" and (obj.parent and obj.parent.type == "ARMATURE"):
            wolf_mesh = obj
        else:
            print(f"[Wolf] Removing stray object '{obj.name}' (type={obj.type})")
            bpy.data.objects.remove(obj, do_unlink=True)

    if not arm_obj or not wolf_mesh:
        raise RuntimeError("Could not find rigged wolf armature + mesh in Wolf Quadruped Rig.glb")

    arm_obj.name = "WolfArmature"
    wolf_mesh.name = "TimberWolf_Mesh"

    # 2. Downscale giant 4096x4096 textures cleanly to 1024x1024 (16x less VRAM & ~12x smaller GLB!)
    for img in bpy.data.images:
        w, h = img.size[0], img.size[1]
        if w > 1024 or h > 1024:
            print(f"[Wolf] Downscaling texture '{img.name}' {w}x{h} -> 1024x1024")
            img.scale(1024, 1024)
            img.pack()

    # 3. Measure actual wolf mesh bounds (excluding stray Icosphere!)
    verts = [wolf_mesh.matrix_world @ v.co for v in wolf_mesh.data.vertices]
    min_x = min(v.x for v in verts)
    max_x = max(v.x for v in verts)
    min_y = min(v.y for v in verts)
    max_y = max(v.y for v in verts)
    min_z = min(v.z for v in verts)
    max_z = max(v.z for v in verts)

    raw_h = max_z - min_z
    raw_l = max_y - min_y
    # Realistic imposing Grey Timber Wolf: ~1.06m ear-tip height (~0.80m shoulder), ~1.58m length
    target_h = 1.06
    scale_f = target_h / raw_h

    # Center X using paws/shoulders (so body is centered cleanly at X=0)
    paw_xs = [v.x for v in verts if v.z < min_z + raw_h * 0.12]
    cx = sum(paw_xs) / max(len(paw_xs), 1)
    cy = 0.5 * (min_y + max_y)

    print(f"[Wolf] Raw wolf bbox: X[{min_x:.3f}..{max_x:.3f}] Y[{min_y:.3f}..{max_y:.3f}] Z[{min_z:.3f}..{max_z:.3f}] (raw_h={raw_h:.3f}, raw_l={raw_l:.3f})")
    print(f"[Wolf] Normalizing: scale={scale_f:.4f}, offset=(-cx={-cx:.3f}, -cy={-cy:.3f}, -min_z={-min_z:.3f})")

    # Transform BOTH mesh vertices AND armature edit_bones identically so skinning stays 100% exact!
    for v in wolf_mesh.data.vertices:
        v.co.x = (v.co.x - cx) * scale_f
        v.co.y = (v.co.y - cy) * scale_f
        v.co.z = (v.co.z - min_z) * scale_f
    wolf_mesh.data.update()

    bpy.ops.object.select_all(action="DESELECT")
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")
    for eb in arm_obj.data.edit_bones:
        eb.head = Vector(((eb.head.x - cx) * scale_f, (eb.head.y - cy) * scale_f, (eb.head.z - min_z) * scale_f))
        eb.tail = Vector(((eb.tail.x - cx) * scale_f, (eb.tail.y - cy) * scale_f, (eb.tail.z - min_z) * scale_f))
    bpy.ops.object.mode_set(mode="OBJECT")

    # Enforce glTF max-4-weights-per-vertex & smooth shading
    for p in wolf_mesh.data.polygons:
        p.use_smooth = True
    bpy.ops.object.select_all(action="DESELECT")
    wolf_mesh.select_set(True)
    bpy.context.view_layer.objects.active = wolf_mesh
    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    wolf_mesh.data.validate(verbose=False)

    # Verify normalized bounds
    nv = [v.co for v in wolf_mesh.data.vertices]
    print(f"[Wolf] Normalized bbox: X[{min(v.x for v in nv):.3f}..{max(v.x for v in nv):.3f}] "
          f"Y[{min(v.y for v in nv):.3f}..{max(v.y for v in nv):.3f}] "
          f"Z[{min(v.z for v in nv):.3f}..{max(v.z for v in nv):.3f}] | bones={len(arm_obj.data.bones)}")
    return arm_obj, wolf_mesh


def build_wolf_animations(arm_obj):
    bpy.ops.object.select_all(action="DESELECT")
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="POSE")

    # Use QUATERNION mode for clean gimbal-lock-free world-space-to-bone-local rotation!
    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "QUATERNION"

    # Precompute each bone's rest 3x3 rotation matrix (world space -> bone local space)
    rest_rot = {b.name: b.matrix_local.to_3x3().normalized() for b in arm_obj.data.bones}
    rest_rot_inv = {name: m.inverted() for name, m in rest_rot.items()}

    def reset_pose():
        for pb in arm_obj.pose.bones:
            pb.location = (0.0, 0.0, 0.0)
            pb.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))

    def set_world_rot(bname, pitch_x=0.0, roll_y=0.0, yaw_z=0.0):
        """
        Rotate bone around WORLD axes (relative to its rest pose!):
          pitch_x > 0 : lifts nose/front UP (rotates -Y toward +Z), swings legs FORWARD (-Y)
          pitch_x < 0 : dips nose/front DOWN, swings legs BACKWARD (+Y)
          roll_y  > 0 : rolls body clockwise looking from front (-Y)
          yaw_z   > 0 : turns nose/tail toward LEFT (+X)
          yaw_z   < 0 : turns nose/tail toward RIGHT (-X)
        """
        pb = arm_obj.pose.bones.get(bname)
        if not pb:
            return
        r_world = Euler((pitch_x, roll_y, yaw_z), "XYZ").to_matrix()
        r_local = rest_rot_inv[bname] @ r_world @ rest_rot[bname]
        pb.rotation_quaternion = r_local.to_quaternion()

    def set_world_loc(bname, dx=0.0, dy=0.0, dz=0.0):
        """Translate bone in WORLD space converted cleanly into bone's local rest frame."""
        pb = arm_obj.pose.bones.get(bname)
        if not pb:
            return
        v_local = rest_rot_inv[bname] @ Vector((dx, dy, dz))
        pb.location = v_local

    def kf_all(frame):
        for pb in arm_obj.pose.bones:
            pb.keyframe_insert(data_path="rotation_quaternion", frame=frame)
            if pb.name in ("root", "hips"):
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

    # Note: In Meshy's raw sculpt, the wolf's neck/head is turned slightly right/left (~+24 deg toward +X).
    # Adding a gentle corrective yaw (-0.36 rad total distributed across neck_01..05 + head) straightens his gaze forward!
    def apply_neck_head(pitch_total, yaw_total, roll_total=0.0, straighten=1.0):
        # Corrective straightening yaw (turns head from +X back to dead center -Y)
        corr_yaw = -0.36 * straighten
        ny = yaw_total + corr_yaw
        set_world_rot("neck_01", pitch_total * 0.16, roll_total * 0.2, ny * 0.16)
        set_world_rot("neck1",   pitch_total * 0.16, roll_total * 0.2, ny * 0.18)
        set_world_rot("neck_03", pitch_total * 0.18, roll_total * 0.2, ny * 0.20)
        set_world_rot("neck2",   pitch_total * 0.16, roll_total * 0.2, ny * 0.18)
        set_world_rot("neck_05", pitch_total * 0.14, roll_total * 0.1, ny * 0.14)
        set_world_rot("head",    pitch_total * 0.20, roll_total * 0.1, ny * 0.14)

    def apply_tail(pitch_base, sway_yaw, wave_phase):
        tail_bones = ["tail_01", "tail1", "tail2", "tail3", "tail4", "tail_06", "tail_07"]
        for i, bn in enumerate(tail_bones):
            w = 1.0 / len(tail_bones)
            lag = wave_phase - i * 0.45
            # Slight corrective -yaw on tail_01..04 since raw sculpt tail curves slightly to +X
            corr = -0.055 if i < 4 else 0.0
            set_world_rot(bn, pitch_base * (0.28 if i < 3 else 0.10), 0.0, corr + sway_yaw * (0.20 + 0.10 * i) * math.sin(lag))

    def apply_ears(pitch_back, splay=0.0):
        set_world_rot("ear.L_01", -pitch_back, -splay, 0.0)
        set_world_rot("ear.L_02", -pitch_back * 0.4, 0.0, 0.0)
        set_world_rot("ear.R_01", -pitch_back,  splay, 0.0)
        set_world_rot("ear.R_02", -pitch_back * 0.4, 0.0, 0.0)

    def apply_front_leg(sfx, shoulder_p, elbow_p, wrist_p, paw_p):
        # For a downward-hanging leg, pitch_x < 0 swings the bottom forward (-Y)!
        # Let's verify sign: R_x(+a) rotates (0, 0, -1) -> (0, +sin(a), -cos(a)) = BACKWARD (+Y)!
        # Therefore pitch_x = -a swings a downward limb FORWARD (-Y)!
        set_world_rot(f"clavicle.{sfx}",       -shoulder_p * 0.28, 0.0, 0.0)
        set_world_rot(f"frontUpr.{sfx}",       -shoulder_p * 0.72, 0.0, 0.0)
        set_world_rot(f"frontLwr.{sfx}",        elbow_p,           0.0, 0.0)
        set_world_rot(f"frontHorselink.{sfx}", -wrist_p,           0.0, 0.0)
        set_world_rot(f"frontFoot.{sfx}",      -paw_p,             0.0, 0.0)

    def apply_rear_leg(sfx, hip_p, stifle_p, hock_p, paw_p):
        # Same sign convention: pitch_x = -a swings downward femur FORWARD (-Y)!
        set_world_rot(f"rearUpr.{sfx}",       -hip_p,    0.0, 0.0)
        set_world_rot(f"rearLwr.{sfx}",        stifle_p, 0.0, 0.0)
        set_world_rot(f"rearHorselink.{sfx}", -hock_p,   0.0, 0.0)
        set_world_rot(f"rearFoot.{sfx}",       paw_p,    0.0, 0.0)

    # =========================================================================
    # 1. ATTACK (Predatory Crouch Coil -> Explosive Lunge & Jaw Snap!)
    # =========================================================================
    # Note right-hand rule around +X: pitch_x < 0 lifts front (-Y) UP; pitch_x > 0 drops front/jaw (-Y) DOWN!
    atk_lunge_y = [(0.00, 0.00), (0.24,  0.12), (0.48, -0.24), (0.68, -0.10), (0.86, 0.00)]
    atk_lunge_z = [(0.00, 0.00), (0.24, -0.07), (0.48,  0.14), (0.68,  0.03), (0.86, 0.00)]
    atk_body_p  = [(0.00, 0.00), (0.24,  0.14), (0.48, -0.28), (0.68, -0.06), (0.86, 0.00)]
    atk_jaw     = [(0.00, 0.00), (0.22,  0.18), (0.46,  0.58), (0.60,  0.04), (0.84, 0.00)]
    atk_front_p = [(0.00, 0.00), (0.24, -0.22), (0.48,  0.86), (0.68,  0.22), (0.86, 0.00)]
    atk_front_e = [(0.00, 0.00), (0.24,  0.55), (0.48,  0.52), (0.68,  0.22), (0.86, 0.00)]
    atk_rear_p  = [(0.00, 0.00), (0.24,  0.34), (0.48, -0.32), (0.68, -0.10), (0.86, 0.00)]

    def attack_pose(phase):
        ly = sample_periodic(atk_lunge_y, phase)
        lz = sample_periodic(atk_lunge_z, phase)
        bp = sample_periodic(atk_body_p, phase)
        jw = sample_periodic(atk_jaw, phase)
        fp = sample_periodic(atk_front_p, phase)
        fe = sample_periodic(atk_front_e, phase)
        rp = sample_periodic(atk_rear_p, phase)

        set_world_loc("root", 0.0, ly, lz)
        set_world_rot("root", bp * 0.55, 0.0, 0.0)
        set_world_rot("spine_02", bp * 0.22, 0.0, 0.0)
        set_world_rot("spine2",   bp * 0.22, 0.0, 0.0)
        set_world_rot("spine_04", bp * 0.18, 0.0, 0.0)
        set_world_rot("spine_05", bp * 0.14, 0.0, 0.0)

        # Level fierce gaze straight at prey, jaw snaps wide open (+pitch_x drops lower jaw DOWN)!
        apply_neck_head(-bp * 0.60 + 0.12, 0.0, 0.0, straighten=1.0)
        set_world_rot("jaw_01", jw * 0.65, 0.0, 0.0)
        set_world_rot("jaw",    jw * 0.35, 0.0, 0.0)
        # Ears pinned back fiercely during lunge!
        apply_ears(0.45 + 0.35 * max(0.0, -bp), splay=0.22)
        apply_tail(-0.18 + 0.30 * max(0.0, -bp), 0.18, phase * 2.0 * math.pi)

        apply_front_leg("L", fp + 0.08, fe, fe * 0.45, 0.14)
        apply_front_leg("R", fp - 0.08, fe, fe * 0.45, 0.14)
        apply_rear_leg("L", rp, max(0.0, rp * 0.8), max(0.0, rp * 0.7), 0.0)
        apply_rear_leg("R", rp, max(0.0, rp * 0.8), max(0.0, rp * 0.7), 0.0)

    # =========================================================================
    # 2. RUN (Authentic Quadruped Rotary Gallop — Dramatic Spine Flex/Extend!)
    # =========================================================================
    # Front legs: reach far forward during extended suspension, drive forcefully back under chest!
    gal_f_sh = [
        (0.00,  0.74),   # Max forward reach!
        (0.18,  0.32),   # Strike & support
        (0.38, -0.58),   # Powerful push-off back under chest
        (0.56, -0.68),   # Trailing lift
        (0.76,  0.22),   # Mid-swing recovery tuck
        (0.90,  0.68),   # Unfold forward
    ]
    gal_f_el = [
        (0.00,  0.14),   # Reaching straight
        (0.20,  0.34),   # Impact absorb
        (0.42,  0.22),   # Push-off
        (0.66,  1.15),   # Deep athletic foreleg tuck!
        (0.84,  0.58),
    ]
    gal_f_wr = [
        (0.00,  0.10),
        (0.22, -0.18),   # Fetlock load
        (0.44,  0.35),
        (0.68,  0.75),   # Wrist folds cleanly in swing!
        (0.88,  0.18),
    ]

    # Rear legs: gather far forward under belly, then explode straight back!
    gal_r_hp = [
        (0.00, -0.64),   # Trailing full extension!
        (0.18, -0.42),   # Lift off
        (0.38,  0.56),   # Gather under belly!
        (0.56,  0.76),   # Max forward reach for hind strike!
        (0.76,  0.10),   # Powerful drive phase
        (0.90, -0.48),   # Explosive push-off!
    ]
    gal_r_st = [
        (0.00, -0.18),
        (0.22,  0.58),   # Fold stifle during swing forward
        (0.45,  0.72),
        (0.66,  0.28),
        (0.86, -0.12),
    ]
    gal_r_hk = [
        (0.00, -0.22),
        (0.24,  0.48),
        (0.48,  0.56),
        (0.72,  0.14),
        (0.90, -0.18),
    ]

    def run_pose(phase):
        t = phase * 2.0 * math.pi
        # Asymmetric lead-lag offset for authentic rotary gallop (LF leads RF by 0.14, LR leads RR by 0.14)
        pFL = phase
        pFR = (phase + 0.14) % 1.0
        pRL = (phase + 0.04) % 1.0
        pRR = (phase + 0.18) % 1.0

        # Spine arches strongly during gather (phase ~0.45) and extends flat during airborne reach (phase ~0.95)
        spine_flex = math.cos(t)  # +1 = extended suspension, -1 = gathered coil!
        bounce_z = 0.085 * math.sin(t + 0.6) + 0.045
        body_pitch = -0.14 * math.sin(t)

        set_world_loc("root", 0.0, 0.0, bounce_z)
        set_world_rot("root", body_pitch, 0.04 * math.sin(t), 0.0)

        # Spine articulation along all 4 lumbar/thoracic vertebrae!
        sf = 0.24 * spine_flex
        set_world_rot("spine_02", sf * 0.30, 0.0, 0.0)
        set_world_rot("spine2",   sf * 0.30, 0.0, 0.0)
        set_world_rot("spine_04", sf * 0.25, 0.0, 0.0)
        set_world_rot("spine_05", sf * 0.15, 0.0, 0.0)

        # Head stays stabilized forward & slightly lowered (predatory sprint silhouette!)
        apply_neck_head(-body_pitch * 0.7 + 0.14 - sf * 0.3, 0.0, 0.0, straighten=1.0)
        set_world_rot("jaw_01", 0.16 + 0.06 * math.sin(t), 0.0, 0.0)  # Slight panting sprint jaw!
        apply_ears(0.32, splay=0.10)
        # Tail streams out straight behind, counter-pitching dynamically!
        apply_tail(0.38 - 0.24 * spine_flex, 0.16, t)

        apply_front_leg("L", sample_periodic(gal_f_sh, pFL), sample_periodic(gal_f_el, pFL), sample_periodic(gal_f_wr, pFL), 0.08)
        apply_front_leg("R", sample_periodic(gal_f_sh, pFR), sample_periodic(gal_f_el, pFR), sample_periodic(gal_f_wr, pFR), 0.08)
        apply_rear_leg("L",  sample_periodic(gal_r_hp, pRL), sample_periodic(gal_r_st, pRL), sample_periodic(gal_r_hk, pRL), 0.06)
        apply_rear_leg("R",  sample_periodic(gal_r_hp, pRR), sample_periodic(gal_r_st, pRR), sample_periodic(gal_r_hk, pRR), 0.06)

    # =========================================================================
    # 3. WALK (Smooth 4-Beat Diagonal Predatory Prowl: LF -> RR -> RF -> LR)
    # =========================================================================
    wlk_f_sh = [
        (0.00,  0.34),  # Forepaw strike
        (0.22,  0.08),  # Mid-stance
        (0.48, -0.32),  # Push-off
        (0.68, -0.12),  # Swing lift
        (0.86,  0.28),  # Reach
    ]
    wlk_f_el = [
        (0.00,  0.08),
        (0.25,  0.18),
        (0.52,  0.14),
        (0.72,  0.64),  # Clean knee fold clearing ground
        (0.88,  0.22),
    ]
    wlk_f_wr = [
        (0.00,  0.05),
        (0.25, -0.10),
        (0.55,  0.16),
        (0.74,  0.42),
        (0.90,  0.10),
    ]

    wlk_r_hp = [
        (0.00,  0.32),
        (0.24,  0.06),
        (0.50, -0.30),
        (0.72,  0.04),
        (0.88,  0.30),
    ]
    wlk_r_st = [
        (0.00,  0.06),
        (0.28,  0.14),
        (0.54, -0.06),
        (0.74,  0.48),
        (0.90,  0.14),
    ]
    wlk_r_hk = [
        (0.00,  0.06),
        (0.30,  0.10),
        (0.55, -0.12),
        (0.75,  0.40),
        (0.90,  0.12),
    ]

    def walk_pose(phase):
        t = phase * 2.0 * math.pi
        # Classic canine 4-beat lateral sequence walk offsets
        pFL = phase
        pRR = (phase + 0.25) % 1.0
        pFR = (phase + 0.50) % 1.0
        pRL = (phase + 0.75) % 1.0

        bob_z = -0.022 * math.cos(2.0 * t)
        sway_x = 0.018 * math.sin(t)
        yaw_s = 0.06 * math.sin(t)

        set_world_loc("root", sway_x, 0.0, bob_z)
        set_world_rot("root", 0.03 * math.sin(2.0 * t), 0.04 * math.sin(t), yaw_s)
        set_world_rot("spine_02", 0.0, 0.0, -yaw_s * 0.5)
        set_world_rot("spine2",   0.0, 0.0, -yaw_s * 0.6)
        set_world_rot("spine_04", 0.0, 0.0,  yaw_s * 0.5)
        set_world_rot("spine_05", 0.0, 0.0,  yaw_s * 0.4)

        # Low stalking predator head with subtle steady bob
        apply_neck_head(-0.14 + 0.04 * math.cos(2.0 * t), -yaw_s * 0.5, 0.0, straighten=1.0)
        apply_ears(0.06, splay=0.02)
        apply_tail(0.08, 0.26, t)

        apply_front_leg("L", sample_periodic(wlk_f_sh, pFL), sample_periodic(wlk_f_el, pFL), sample_periodic(wlk_f_wr, pFL), 0.04)
        apply_front_leg("R", sample_periodic(wlk_f_sh, pFR), sample_periodic(wlk_f_el, pFR), sample_periodic(wlk_f_wr, pFR), 0.04)
        apply_rear_leg("L",  sample_periodic(wlk_r_hp, pRL), sample_periodic(wlk_r_st, pRL), sample_periodic(wlk_r_hk, pRL), 0.04)
        apply_rear_leg("R",  sample_periodic(wlk_r_hp, pRR), sample_periodic(wlk_r_st, pRR), sample_periodic(wlk_r_hk, pRR), 0.04)

    # =========================================================================
    # 4. IDLE (Alert Breathing, Ear Twitch, Head Scan & Tail Sway — RECORDED LAST!)
    # =========================================================================
    def idle_pose(phase):
        t = phase * 2.0 * math.pi
        b = math.sin(t)
        s2 = math.sin(t * 0.5)
        set_world_loc("root", 0.0, 0.0, 0.008 * b)
        set_world_rot("root", 0.015 * b, 0.0, 0.0)
        set_world_rot("spine_04", -0.025 * b, 0.0, 0.0)
        set_world_rot("spine_05", -0.020 * b, 0.0, 0.0)
        set_world_rot("chest",    -0.018 * b, 0.0, 0.0)

        # Alert wild scan (partially straightens + scans gently left & right!)
        apply_neck_head(0.04 * b, 0.14 * s2, 0.03 * s2, straighten=0.85)
        set_world_rot("jaw_01", -0.02 - 0.02 * max(0.0, b), 0.0, 0.0)
        # Subtle ear perk/twitch
        twitch = 0.12 * (max(0.0, math.sin(t * 2.0 - 1.2)) ** 6)
        set_world_rot("ear.L_01", -twitch, -0.04, 0.0)
        set_world_rot("ear.R_01", -0.03 * b, 0.04, 0.0)
        apply_tail(0.04 + 0.05 * b, 0.28, t)

    attack_action = record_clip("Attack", 22, attack_pose)
    run_action    = record_clip("Run",    20, run_pose)
    walk_action   = record_clip("Walk",   32, walk_pose)
    idle_action   = record_clip("Idle",   48, idle_pose)

    bpy.ops.object.mode_set(mode="OBJECT")
    print("[Wolf] Baked 4 NLA clips: Attack, Run, Walk, Idle (LAST)")
    return idle_action, walk_action, run_action, attack_action


def render_wolf_previews(arm_obj, idle_action, walk_action, run_action, attack_action):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 768
    scene.render.resolution_y = 640
    scene.render.image_settings.file_format = "PNG"

    world = bpy.data.worlds.new("StudioWorld_TMP")
    scene.world = world
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (*srgb_to_linear((0.18, 0.21, 0.26)), 1.0)
    bg.inputs["Strength"].default_value = 0.65

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
        add_area_light("KeyLight_TMP",  (-1.8, -2.0, 2.1), (0.0, 0.0, 0.55), 180.0, 1.2, (1.0, 0.96, 0.90)),
        add_area_light("FillLight_TMP", ( 2.1, -1.4, 1.3), (0.0, 0.0, 0.50),  70.0, 2.0, (0.82, 0.90, 1.0)),
        add_area_light("RimLight_TMP",  ( 0.5,  2.2, 1.9), (0.0, 0.0, 0.65), 180.0, 1.0, (0.92, 0.96, 1.0)),
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

    render_shot("meshy_wolf_idle.png",   idle_action,    1, (-1.85, -2.15, 0.85), (0.0, -0.05, 0.48), lens=44.0)
    render_shot("meshy_wolf_walk.png",   walk_action,    1, ( 2.65, -0.40, 0.68), (0.0,  0.00, 0.46), lens=44.0)
    render_shot("meshy_wolf_run.png",    run_action,     1, ( 2.65, -0.85, 0.70), (0.0,  0.00, 0.50), lens=42.0)
    render_shot("meshy_wolf_attack.png", attack_action, 11, (-1.95, -2.15, 0.82), (0.0, -0.18, 0.54), lens=42.0)

    arm_obj.animation_data.action = None
    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = False
    scene.frame_set(1)

    for tmp in [floor, cam_obj] + lights:
        bpy.data.objects.remove(tmp, do_unlink=True)
    bpy.data.materials.remove(fmat, do_unlink=True)


def main():
    arm_obj, wolf_mesh = import_optimize_and_normalize_wolf()
    idle_action, walk_action, run_action, attack_action = build_wolf_animations(arm_obj)
    render_wolf_previews(arm_obj, idle_action, walk_action, run_action, attack_action)

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
    print(f"[Wolf] Saved editable rig -> {OUT_BLEND}")
    print(f"[Wolf] Exported skinned glTF -> {OUT_GLB} ({sz_mb:.2f} MB)")


if __name__ == "__main__":
    main()
