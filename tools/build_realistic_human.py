"""
SAGE Engine — Realistic Rigged & Animated 3D Human Builder (Blender 5.3)
Clean AAA Quad-Topology Subdivision-Surface Modeling + True Linear PBR Materials
+ 22-Bone Armature Skinning + 4 Named Skeletal Clips (Idle, Walk, Run, Swim).

Outputs:
  - assets/models/human_rigged.blend       (Editable Blender source with full rig & NLA)
  - assets/models/character_rigged.glb     (Skinned glTF 2.0 with Idle, Walk, Run, Swim)
  - 5 studio Cycles renders in artifact directory for visual verification:
      * human_front_34.png
      * human_closeup_head.png
      * human_head_profile.png
      * human_side_profile.png
      * human_run_pose.png
"""

import bpy
import bmesh
import math
import os
from mathutils import Vector, Euler

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
ARTIFACT_DIR = r"C:\Users\DASUN\.gemini\antigravity\brain\2f9de14d-2f3f-473b-8005-216904e8d69d"
OUT_BLEND = os.path.join(MODELS_DIR, "human_rigged.blend")
OUT_GLB = os.path.join(MODELS_DIR, "character_rigged.glb")
os.makedirs(MODELS_DIR, exist_ok=True)
os.makedirs(ARTIFACT_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def srgb_to_linear(c):
    return tuple((v ** 2.2) for v in c)


def make_mat(name, srgb_color, roughness=0.55, metallic=0.0, coat=0.0, sss=0.0,
             bump_scale=0.0, bump_strength=0.0, sheen=0.0, specular=0.35):
    """Creates a realistic PBR material converting sRGB palette cleanly into Blender linear space."""
    mat = bpy.data.materials.new(name=name)
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    lin = srgb_to_linear(srgb_color)
    bsdf.inputs["Base Color"].default_value = (*lin, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = specular
    if coat > 0.0 and "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = coat
    if sheen > 0.0 and "Sheen Weight" in bsdf.inputs:
        bsdf.inputs["Sheen Weight"].default_value = sheen
    if sss > 0.0 and "Subsurface Weight" in bsdf.inputs:
        bsdf.inputs["Subsurface Weight"].default_value = sss
        if "Subsurface Scale" in bsdf.inputs:
            bsdf.inputs["Subsurface Scale"].default_value = 0.008
    if bump_scale > 0.0 and bump_strength > 0.0:
        tex_coord = nt.nodes.new("ShaderNodeTexCoord")
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = bump_scale
        noise.inputs["Detail"].default_value = 6.0
        noise.inputs["Roughness"].default_value = 0.65
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = bump_strength
        bump.inputs["Distance"].default_value = 0.0025
        nt.links.new(tex_coord.outputs["Object"], noise.inputs["Vector"])
        nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def set_smooth(obj):
    if obj.type == "MESH" and obj.data:
        for p in obj.data.polygons:
            p.use_smooth = True


def apply_mod(obj, mod_type, **kwargs):
    mod = obj.modifiers.new(name=mod_type, type=mod_type)
    for k, v in kwargs.items():
        setattr(mod, k, v)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def gauss(x, mu, sigma):
    d = (x - mu) / sigma
    return math.exp(-0.5 * d * d)


def gauss2(x, mx, sx, y, my, sy):
    dx = (x - mx) / sx
    dy = (y - my) / sy
    return math.exp(-0.5 * (dx * dx + dy * dy))


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def smoothstep(edge0, edge1, x):
    t = clamp((x - edge0) / max(edge1 - edge0, 1e-6), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# =============================================================================
# 1. ANATOMICALLY SCULPTED HEAD + FACE + JAW + NECK (CRISP FRONT & SIDE PROFILE)
# =============================================================================
def build_anatomical_head_and_neck(mats):
    mesh = bpy.data.meshes.new("HeadNeck_Mesh")
    bm = bmesh.new()

    # Slightly lowered natural neck-to-head proportions (chin ~1.548, crown ~1.768)
    z_levels = [
        1.420, 1.444, 1.468, 1.490, 1.508, 1.522,  # Upper chest / trapezius / athletic neck
        1.531, 1.539, 1.545, 1.551, 1.557, 1.563,  # Throat -> submental -> chin bottom -> chin
        1.568, 1.573, 1.577, 1.581, 1.585, 1.589,  # Mentolabial crease -> lower lip -> mouth -> upper lip
        1.593, 1.597, 1.601, 1.606, 1.612, 1.618,  # Philtrum -> nostrils -> nose tip -> mid bridge
        1.624, 1.630, 1.636, 1.642, 1.648, 1.655,  # Eye sockets -> nasion -> brow ridge -> lower forehead
        1.665, 1.677, 1.690, 1.704, 1.718, 1.731,  # Forehead -> hairline -> parietal dome
        1.743, 1.753, 1.761, 1.767,                # Broad natural masculine crown
    ]
    n_ang = 68

    rings = []
    for z in z_levels:
        ring = []
        if z < 1.528:
            t_neck = smoothstep(1.420, 1.488, z)
            rx = 0.112 * (1.0 - t_neck) + 0.052 * t_neck
            rf = 0.088 * (1.0 - t_neck) + 0.050 * t_neck
            rb = 0.082 * (1.0 - t_neck) + 0.058 * t_neck
            cy = 0.018 * (1.0 - t_neck) + 0.012 * t_neck
        elif z < 1.552:
            t = smoothstep(1.528, 1.552, z)
            rx = 0.052 + 0.011 * (t ** 0.9)
            rf = 0.050 + 0.049 * (t ** 0.80)
            rb = 0.058 + 0.008 * t
            cy = 0.012 * (1.0 - t) + 0.004 * t
        elif z < 1.652:
            t = (z - 1.552) / 0.100
            rx = 0.063 + 0.016 * math.sin(clamp(t * 1.1, 0.0, 1.0) * math.pi * 0.5)
            rf = 0.099 - 0.003 * smoothstep(0.0, 0.35, t) + 0.003 * gauss(t, 0.92, 0.15)
            t_occ = smoothstep(1.552, 1.650, z)
            rb = 0.066 + 0.048 * (t_occ ** 0.78)
            cy = 0.004
        else:
            u = clamp((z - 1.652) / (1.770 - 1.652), 0.0, 0.996)
            dome = (1.0 - u ** 2.35) ** 0.45
            rx = 0.079 * dome
            rf = 0.098 * (dome ** 0.96)
            rb = 0.114 * (dome ** 0.94)
            cy = 0.004 + 0.008 * u

        for i in range(n_ang):
            phi = (2.0 * math.pi * i) / n_ang
            alpha = phi if phi <= math.pi else phi - 2.0 * math.pi
            abs_a = abs(alpha)
            sinp = math.sin(phi)
            cosp = math.cos(phi)

            w_front = smoothstep(-0.35, 0.35, cosp)
            ry_local = rb * (1.0 - w_front) + rf * w_front

            x = rx * sinp
            y = cy - ry_local * cosp
            z_off = 0.0

            # 1. Chiseled Jawline, Chin & Submental Shadow
            if 1.535 < z < 1.605:
                chin_zone = gauss(z, 1.554, 0.017)
                if cosp > 0.25:
                    x *= (1.0 - 0.25 * chin_zone * (cosp ** 1.4))
                chin_proj = gauss2(alpha, 0.0, 0.24, z, 1.555, 0.009)
                y -= 0.0085 * chin_proj
                jaw_ang = gauss2(abs_a, 1.32, 0.24, z, 1.574, 0.014)
                x += math.copysign(0.0075 * jaw_ang, x)
                under_jaw = gauss(z, 1.537, 0.008) * smoothstep(-0.2, 0.6, cosp)
                y += 0.012 * under_jaw

            # 2. Neck Sternocleidomastoid & Adam's Apple
            if 1.460 < z < 1.535:
                y -= 0.0038 * gauss2(alpha, 0.0, 0.18, z, 1.508, 0.008)
                y -= 0.005 * gauss2(abs_a - math.pi, 0.0, 0.45, z, 1.525, 0.018)

            # 3. Cheekbones & Subtle Cheek Hollow
            if 1.565 < z < 1.638:
                cheek = gauss2(abs_a, 0.64, 0.24, z, 1.612, 0.015)
                x += math.copysign(0.0072 * cheek, x)
                y -= 0.0070 * cheek
                hollow = gauss2(abs_a, 0.56, 0.22, z, 1.582, 0.013)
                x -= math.copysign(0.0048 * hollow, x)
                y += 0.0040 * hollow

            # 4. Deep Anatomical Eye Sockets (Recessed cleanly BEHIND brow!) & Strong Brow Ridge
            if 1.608 < z < 1.658:
                socket = gauss2(abs_a, 0.34, 0.155, z, 1.628, 0.0115)
                y += 0.0205 * socket  # Deep realistic socket so eyeballs NEVER bulge forward!
                brow_arch = gauss2(abs_a, 0.30, 0.25, z, 1.646, 0.0075)
                glabella = gauss2(alpha, 0.0, 0.14, z, 1.645, 0.007)
                y -= 0.0105 * brow_arch + 0.0060 * glabella
                nasion = gauss2(alpha, 0.0, 0.10, z, 1.634, 0.006)
                y += 0.0040 * nasion

            # 5. Crisp Realistic Sculpted Nose (Clear from BOTH Front 3/4 AND Side Profile!)
            if 1.586 < z < 1.642 and abs_a < 0.36:
                if 1.594 <= z <= 1.640:
                    bt = clamp((1.640 - z) / (1.640 - 1.599), 0.0, 1.0)
                    bw = 0.068 + 0.026 * bt  # Sharper, narrower nasal bridge so front 3/4 reads crisply!
                    env_z = smoothstep(1.641, 1.631, z) * smoothstep(1.591, 1.599, z)
                    proj = (0.009 + 0.021 * (bt ** 0.88)) * gauss(alpha, 0.0, bw) * env_z
                    y -= proj

                tip = gauss2(alpha, 0.0, 0.088, z, 1.600, 0.0052)
                y -= 0.0090 * tip
                z_off -= 0.0020 * tip

                ala = gauss2(abs_a, 0.128, 0.052, z, 1.5965, 0.0045)
                y -= 0.0110 * ala
                x += math.copysign(0.0048 * ala, x)

                under_nose = gauss2(alpha, 0.0, 0.17, z, 1.589, 0.0038)
                y += 0.0058 * under_nose
                philtrum = gauss2(alpha, 0.0, 0.052, z, 1.586, 0.0042)
                y += 0.0038 * philtrum

            # 6. Pure 3D Sculpted Natural Lips (NO blocky flat pink texture!)
            if 1.560 < z < 1.589 and abs_a < 0.42:
                lip_w = gauss(alpha, 0.0, 0.19)
                cupid = 1.0 - 0.25 * gauss(alpha, 0.0, 0.045)
                u_lip = gauss(z, 1.5800, 0.0032) * lip_w * cupid
                corner_curve = 0.0020 * (alpha / 0.22) ** 2 if abs_a < 0.28 else 0.0020
                m_seam = gauss(z - corner_curve, 1.5755, 0.0017) * gauss(alpha, 0.0, 0.215)
                l_lip = gauss(z, 1.5705, 0.0038) * gauss(alpha, 0.0, 0.165)
                chin_fold = gauss(z, 1.5635, 0.0038) * gauss(alpha, 0.0, 0.175)

                y -= 0.0095 * u_lip
                y += 0.0090 * m_seam   # Deep natural recessed mouth crease casts real soft shadow!
                y -= 0.0088 * l_lip
                y += 0.0062 * chin_fold

            # 7. Temple Planes
            if 1.638 < z < 1.700:
                temple = gauss2(abs_a, 1.05, 0.22, z, 1.665, 0.022)
                x -= math.copysign(0.0040 * temple, x)

            ring.append(bm.verts.new((x, y, z + z_off)))
        rings.append(ring)

    for r in range(len(rings) - 1):
        r0, r1 = rings[r], rings[r + 1]
        for i in range(n_ang):
            ni = (i + 1) % n_ang
            bm.faces.new((r0[i], r0[ni], r1[ni], r1[i]))

    crown_v = bm.verts.new((0.0, 0.012, 1.770))
    for i in range(n_ang):
        ni = (i + 1) % n_ang
        bm.faces.new((rings[-1][i], rings[-1][ni], crown_v))

    bot_v = bm.verts.new((0.0, 0.018, 1.418))
    for i in range(n_ang):
        ni = (i + 1) % n_ang
        bm.faces.new((rings[0][ni], rings[0][i], bot_v))

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("HeadNeck", mesh)
    bpy.context.collection.objects.link(obj)
    # Single seamless realistic skin shader (3D sculpted lip shadow looks 100x more natural than blocky polygon strips!)
    obj.data.materials.append(mats["skin"])

    apply_mod(obj, "SUBSURF", levels=1, render_levels=1)
    set_smooth(obj)
    return obj


# =============================================================================
# 2. RECESSED REALISTIC EYES (CRISP CIRCULAR IRIS+PUPIL!), EYELIDS, BROWS, EARS & CLEAN HAIR
# =============================================================================
def build_head_details(mats):
    parts = []

    for side in (-1, 1):
        ex = side * 0.0315
        ey = -0.0725   # Recessed PROPERLY inside the eye socket so cornea sits ~12mm BEHIND brow!
        ez = 1.6280
        er = 0.0112

        # --- Build polar-aligned eyeball directly in bmesh with pole facing -Y (PERFECT circular iris & pupil!) ---
        bm = bmesh.new()
        n_lon = 28
        # Latitude rings from front pupil center (theta=0, -Y) back to rear of eye (theta=pi, +Y)
        # Exact ring breaks for crisp circular pupil, iris, and limbal ring!
        thetas = [
            0.08, 0.16, 0.22,        # Pupil (0..0.22 rad)
            0.30, 0.38, 0.46,        # Iris (0.22..0.46 rad)
            0.53,                    # Dark limbal ring (0.46..0.53 rad)
            0.68, 0.90, 1.20, 1.57, 2.05, 2.55,  # Sclera
        ]
        front_pole = bm.verts.new((ex, ey - er, ez))
        back_pole = bm.verts.new((ex, ey + er, ez))
        e_rings = []
        for th in thetas:
            st, ct = math.sin(th), math.cos(th)
            ring = []
            for j in range(n_lon):
                a = (2.0 * math.pi * j) / n_lon
                ring.append(bm.verts.new((ex + er * st * math.cos(a),
                                          ey - er * ct,
                                          ez + er * st * math.sin(a))))
            e_rings.append(ring)

        # Front pupil cap faces (mat 2 = pupil)
        for j in range(n_lon):
            nj = (j + 1) % n_lon
            f = bm.faces.new((front_pole, e_rings[0][j], e_rings[0][nj]))
            f.material_index = 2

        # Latitude quad strips with EXACT ring material indices!
        for r in range(len(e_rings) - 1):
            th_mid = 0.5 * (thetas[r] + thetas[r + 1])
            if th_mid < 0.22:
                m_idx = 2  # Pupil
            elif th_mid < 0.46:
                m_idx = 1  # Rich brown-hazel iris
            elif th_mid < 0.53:
                m_idx = 3  # Dark limbal ring
            else:
                m_idx = 0  # Sclera
            for j in range(n_lon):
                nj = (j + 1) % n_lon
                f = bm.faces.new((e_rings[r][j], e_rings[r + 1][j], e_rings[r + 1][nj], e_rings[r][nj]))
                f.material_index = m_idx

        # Rear sclera cap
        for j in range(n_lon):
            nj = (j + 1) % n_lon
            f = bm.faces.new((e_rings[-1][nj], e_rings[-1][j], back_pole))
            f.material_index = 0

        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        em = bpy.data.meshes.new(f"Eye_{side}_Mesh")
        bm.to_mesh(em)
        bm.free()

        eye = bpy.data.objects.new(f"Eye_{'L' if side > 0 else 'R'}", em)
        bpy.context.collection.objects.link(eye)
        eye.data.materials.append(mats["sclera"])
        eye.data.materials.append(mats["iris"])
        eye.data.materials.append(mats["pupil"])
        eye.data.materials.append(mats["iris_ring"])
        set_smooth(eye)
        parts.append(eye)

        # --- Sleek Recessed Almond Eyelids + Thin Dark Upper Lash Line (Hugs eyeball cleanly with ZERO lower white crescent!) ---
        for lid_name, z_sign, thick, arch, z_shift, y_off, mat_key in (
            ("UpperLid",  1.0, 0.0036, 0.0050,  0.0018, -0.0004, "skin"),
            ("UpperLash", 1.0, 0.0013, 0.0040,  0.0002, -0.0013, "hair"),
            ("LowerLid", -1.0, 0.0042, 0.0028, -0.0026, -0.0003, "skin"),
        ):
            bm = bmesh.new()
            n_u = 18
            n_v = 10
            lid_rings = []
            for ui in range(n_u):
                t = ui / (n_u - 1)
                u = (t - 0.5) * 2.0
                lx = ex + side * (u * 0.0138)
                ly = ey - 0.0108 * math.cos(u * 0.88) + y_off
                lz = ez + z_shift + z_sign * (0.0008 + arch * (math.cos(u * math.pi * 0.5) ** 0.85))
                rad = thick * (0.28 + 0.72 * math.cos(u * math.pi * 0.46))
                ring = []
                for vi in range(n_v):
                    va = (2.0 * math.pi * vi) / n_v
                    ring.append(bm.verts.new((lx, ly + math.cos(va) * rad, lz + math.sin(va) * rad * 0.88)))
                lid_rings.append(ring)
            for ui in range(n_u - 1):
                for vi in range(n_v):
                    nv = (vi + 1) % n_v
                    bm.faces.new((lid_rings[ui][vi], lid_rings[ui][nv], lid_rings[ui + 1][nv], lid_rings[ui + 1][vi]))
            # Cap corners cleanly
            c_in = bm.verts.new((ex - side * 0.0142, ey - 0.006, ez))
            c_out = bm.verts.new((ex + side * 0.0142, ey - 0.006, ez))
            for vi in range(n_v):
                nv = (vi + 1) % n_v
                bm.faces.new((lid_rings[0][nv], lid_rings[0][vi], c_in))
                bm.faces.new((lid_rings[-1][vi], lid_rings[-1][nv], c_out))
            bm.normal_update()
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
            lm = bpy.data.meshes.new(f"{lid_name}_{side}_Mesh")
            bm.to_mesh(lm)
            bm.free()
            lobj = bpy.data.objects.new(f"{lid_name}_{'L' if side > 0 else 'R'}", lm)
            bpy.context.collection.objects.link(lobj)
            lobj.data.materials.append(mats[mat_key])
            set_smooth(lobj)
            parts.append(lobj)

        # --- Natural Sculpted Eyebrows resting cleanly along brow ridge ---
        bm = bmesh.new()
        brow_rings = []
        n_b = 14
        n_s = 8
        for bi in range(n_b):
            t = bi / (n_b - 1)
            bx = side * (0.012 + t * 0.038)
            by = -0.1000 + 0.022 * (t ** 1.55)
            bz = 1.6445 + 0.0040 * math.sin(t * math.pi * 0.82) - 0.0018 * t
            w_z = 0.0034 * (0.32 + 0.68 * math.sin((0.12 + 0.78 * t) * math.pi)) * (1.0 - 0.28 * t)
            w_y = 0.0020
            ring = []
            for si in range(n_s):
                sa = (2.0 * math.pi * si) / n_s
                ring.append(bm.verts.new((bx, by + math.cos(sa) * w_y, bz + math.sin(sa) * w_z)))
            brow_rings.append(ring)
        for bi in range(n_b - 1):
            for si in range(n_s):
                ns = (si + 1) % n_s
                bm.faces.new((brow_rings[bi][si], brow_rings[bi][ns], brow_rings[bi + 1][ns], brow_rings[bi + 1][si]))
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bmesh_d = bpy.data.meshes.new(f"Brow_{side}_Mesh")
        bm.to_mesh(bmesh_d)
        bm.free()
        bobj = bpy.data.objects.new(f"Eyebrow_{'L' if side > 0 else 'R'}", bmesh_d)
        bpy.context.collection.objects.link(bobj)
        bobj.data.materials.append(mats["hair"])
        set_smooth(bobj)
        parts.append(bobj)

        # --- Sculpted Smooth Organic Human Ears (Closed manifold rounded helix + lobe + concha!) ---
        bm = bmesh.new()
        ear_pts = [
            ( 0.016, -0.006, -0.003, 0.0036),
            ( 0.023,  0.003,  0.007, 0.0046),
            ( 0.017,  0.014,  0.011, 0.0048),
            ( 0.003,  0.017,  0.011, 0.0046),
            (-0.011,  0.013,  0.009, 0.0048),
            (-0.021,  0.004,  0.007, 0.0056),
            (-0.021, -0.005, -0.002, 0.0040),
        ]
        ear_rings = []
        n_es = 10
        for (dz, dy, rx_out, thick) in ear_pts:
            ex_c = side * (0.072 + rx_out)
            ey_c = 0.015 + dy
            ez_c = 1.618 + dz
            ring = []
            for si in range(n_es):
                sa = (2.0 * math.pi * si) / n_es
                ring.append(bm.verts.new((
                    ex_c + side * math.cos(sa) * thick * 0.58,
                    ey_c + math.cos(sa) * thick,
                    ez_c + math.sin(sa) * thick * 0.88,
                )))
            ear_rings.append(ring)
        for ri in range(len(ear_rings) - 1):
            for si in range(n_es):
                ns = (si + 1) % n_es
                bm.faces.new((ear_rings[ri][si], ear_rings[ri][ns], ear_rings[ri + 1][ns], ear_rings[ri + 1][si]))
        # Rounded end-caps tucked smoothly into side of skull
        c_top = bm.verts.new((side * 0.066, 0.006, 1.633))
        c_bot = bm.verts.new((side * 0.066, 0.008, 1.597))
        for si in range(n_es):
            ns = (si + 1) % n_es
            bm.faces.new((ear_rings[0][ns], ear_rings[0][si], c_top))
            bm.faces.new((ear_rings[-1][si], ear_rings[-1][ns], c_bot))
        # Inner concha backing shell
        back_c = bm.verts.new((side * 0.070, 0.016, 1.618))
        for ri in range(len(ear_rings) - 1):
            bm.faces.new((ear_rings[ri][0], ear_rings[ri + 1][0], back_c))
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        emesh = bpy.data.meshes.new(f"Ear_{side}_Mesh")
        bm.to_mesh(emesh)
        bm.free()
        eobj = bpy.data.objects.new(f"Ear_{'L' if side > 0 else 'R'}", emesh)
        bpy.context.collection.objects.link(eobj)
        eobj.data.materials.append(mats["skin"])
        apply_mod(eobj, "SUBSURF", levels=2, render_levels=2)
        set_smooth(eobj)
        parts.append(eobj)

    # --- Clean, Cohesive Modern Short-Fade Swept Hairstyle (ZERO poking dots/nubs!) ---
    bm = bmesh.new()
    hair_rings = []
    n_h = 56
    n_lat = 22
    for li in range(n_lat):
        v = li / (n_lat - 1)
        ring = []
        for i in range(n_h):
            phi = (2.0 * math.pi * i) / n_h
            alpha = phi if phi <= math.pi else phi - 2.0 * math.pi
            abs_a = abs(alpha)

            # Crisp natural masculine hairline: forehead (1.678), sideburn, ear arch, tapered nape (1.578)
            t_back = smoothstep(0.50, 2.30, abs_a)
            z_base = 1.678 - 0.098 * t_back
            z_base -= 0.022 * gauss(abs_a, 1.24, 0.14)
            z_base += 0.022 * gauss(abs_a, 1.68, 0.24)
            z_base += 0.008 * gauss(abs_a, 0.62, 0.18)

            # Natural styled volume: slightly higher swept front quiff, smooth natural crown!
            quiff_lift = 0.011 * gauss(alpha, -0.18, 0.65) * smoothstep(0.15, 0.65, v) * (1.0 - 0.55 * v)
            z_top = 1.778
            z = z_base + (z_top - z_base) * (1.0 - (1.0 - v) ** 1.68) + quiff_lift

            if z < 1.652:
                t_occ = smoothstep(1.552, 1.650, z)
                rx_base = 0.078
                rf_base = 0.098
                rb_base = 0.066 + 0.048 * (t_occ ** 0.78)
                cy_base = 0.004
            else:
                u = clamp((z - 1.652) / (1.784 - 1.652), 0.0, 0.992)
                dome = (1.0 - u ** 2.35) ** 0.45
                rx_base = 0.080 * dome
                rf_base = 0.100 * (dome ** 0.96)
                rb_base = 0.115 * (dome ** 0.94)
                cy_base = 0.004 + 0.008 * u

            # Seamless tapered fade on sides/back, sculpted soft textured wave volume on top!
            top_mask = smoothstep(0.22, 0.78, v)
            # Soft directional combed hair grooves (continuous, NEVER poking spikes!)
            grooves = (0.0024 * math.sin(alpha * 14.0 + v * 2.5) +
                       0.0014 * math.cos(alpha * 23.0 - v * 3.0)) * top_mask * (1.0 - v * 0.85)
            # Fuller styled side-part volume
            part_dip = -0.0032 * gauss(alpha, 0.52, 0.12) * top_mask * (1.0 - 0.5 * v)
            front_full = 0.0075 * gauss(alpha, -0.15, 0.70) * smoothstep(0.08, 0.55, v) * (1.0 - 0.5 * v)
            thick = 0.0036 + 0.0078 * top_mask + front_full + grooves + part_dip
            thick *= (0.32 + 0.68 * smoothstep(0.0, 0.14, v))

            cosp = math.cos(phi)
            sinp = math.sin(phi)
            w_front = smoothstep(-0.35, 0.35, cosp)
            ry_base = rb_base * (1.0 - w_front) + rf_base * w_front

            x = (rx_base + thick) * sinp
            y = cy_base - (ry_base + thick) * cosp

            ring.append(bm.verts.new((x, y, z)))
        hair_rings.append(ring)

    for li in range(n_lat - 1):
        for i in range(n_h):
            ni = (i + 1) % n_h
            bm.faces.new((hair_rings[li][i], hair_rings[li][ni], hair_rings[li + 1][ni], hair_rings[li + 1][i]))

    top_center = bm.verts.new((0.0, 0.012, 1.781))
    for i in range(n_h):
        ni = (i + 1) % n_h
        bm.faces.new((hair_rings[-1][i], hair_rings[-1][ni], top_center))

    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    hmesh = bpy.data.meshes.new("Hair_Mesh")
    bm.to_mesh(hmesh)
    bm.free()
    hobj = bpy.data.objects.new("Hair", hmesh)
    bpy.context.collection.objects.link(hobj)
    hobj.data.materials.append(mats["hair"])
    apply_mod(hobj, "SUBSURF", levels=1, render_levels=1)
    set_smooth(hobj)
    parts.append(hobj)

    return parts


# =============================================================================
# 3. REALISTIC LAYERED BOMBER/LEATHER JACKET + INNER WHITE CREW-NECK TEE
# =============================================================================
def build_seamless_jacket_and_shirt(mats):
    # Inner Crew-Neck White T-Shirt
    bm_t = bmesh.new()
    n_t = 20
    tee_spec = [
        (0.955, 0.148, 0.098, 0.104, 0.008),
        (1.060, 0.150, 0.102, 0.106, 0.008),
        (1.190, 0.168, 0.122, 0.120, 0.005),
        (1.315, 0.186, 0.132, 0.124, 0.005),
        (1.410, 0.170, 0.108, 0.108, 0.010),
        (1.454, 0.070, 0.062, 0.066, 0.012),
    ]
    tee_rings = []
    for (z, rx, ryf, ryb, cy) in tee_spec:
        ring = []
        for i in range(n_t):
            a = (2.0 * math.pi * i) / n_t
            ca, sa = math.cos(a), math.sin(a)
            ry = ryb if sa >= 0 else ryf
            z_m = z - (0.016 * (-sa) if (z > 1.44 and sa < -0.3) else 0.0)
            ring.append(bm_t.verts.new((rx * ca, cy + ry * sa, z_m)))
        tee_rings.append(ring)
    for r in range(len(tee_rings) - 1):
        for i in range(n_t):
            ni = (i + 1) % n_t
            bm_t.faces.new((tee_rings[r][i], tee_rings[r][ni], tee_rings[r + 1][ni], tee_rings[r + 1][i]))
    bm_t.normal_update()
    bmesh.ops.recalc_face_normals(bm_t, faces=bm_t.faces)
    tmesh = bpy.data.meshes.new("InnerTee_Mesh")
    bm_t.to_mesh(tmesh)
    bm_t.free()
    tee_obj = bpy.data.objects.new("InnerTee", tmesh)
    bpy.context.collection.objects.link(tee_obj)
    tee_obj.data.materials.append(mats["shirt"])
    apply_mod(tee_obj, "SUBSURF", levels=1, render_levels=1)
    set_smooth(tee_obj)

    # Open-Front Seamless Quad-Topology Black/Charcoal Leather Bomber Jacket
    mesh = bpy.data.meshes.new("Jacket_Mesh")
    bm = bmesh.new()

    n_seg = 16
    torso_spec = [
        (0.952, 0.165, 0.111, 0.117, 0.008),  # 0: Waist hem
        (1.005, 0.160, 0.109, 0.113, 0.008),  # 1: Waist
        (1.068, 0.165, 0.114, 0.117, 0.008),  # 2: Abdomen
        (1.138, 0.175, 0.124, 0.123, 0.006),  # 3: Ribcage
        (1.208, 0.188, 0.136, 0.131, 0.004),  # 4: Lower chest
        (1.275, 0.200, 0.145, 0.137, 0.004),  # 5: Armpit bottom
        (1.345, 0.212, 0.144, 0.135, 0.006),  # 6: Broad athletic shoulders / chest
        (1.406, 0.190, 0.120, 0.118, 0.010),  # 7: Sloped athletic deltoid top
        (1.452, 0.122, 0.094, 0.095, 0.014),  # 8: Trapezius / collar base
        (1.488, 0.075, 0.073, 0.074, 0.012),  # 9: Standing bomber collar
    ]

    t_rings = []
    for (z, rx, ryf, ryb, cy) in torso_spec:
        ring = []
        for i in range(n_seg):
            ang = (2.0 * math.pi * i) / n_seg
            ca, sa = math.cos(ang), math.sin(ang)
            ry = ryb if sa >= 0.0 else ryf
            x = rx * ca
            y = cy + ry * sa

            if 1.24 < z < 1.39 and sa < -0.2:
                pec = gauss(z, 1.325, 0.055) * gauss(abs(ca), 0.45, 0.35)
                y -= 0.016 * pec
            if 1.05 < z < 1.40 and sa > 0.7:
                y -= 0.008 * (sa ** 4)
            z_mod = z
            if z > 1.470 and sa < -0.35:
                z_mod -= 0.016 * (-sa)

            ring.append(bm.verts.new((x, y, z_mod)))
        t_rings.append(ring)

    skip_quads = set()
    for r in (5, 6):
        for seg in (15, 0, 7, 8):
            skip_quads.add((r, seg))
    for r in range(len(t_rings) - 1):
        skip_quads.add((r, 11))
        skip_quads.add((r, 12))

    for r in range(len(t_rings) - 1):
        r0, r1 = t_rings[r], t_rings[r + 1]
        for i in range(n_seg):
            if (r, i) in skip_quads:
                continue
            ni = (i + 1) % n_seg
            bm.faces.new((r0[i], r0[ni], r1[ni], r1[i]))

    # Crisp 3D folded jacket front lapels with metallic zipper trim along open edges!
    for edge_idx, sgn in ((11, -1), (13, 1)):
        inner_lapel = []
        for r in range(len(t_rings)):
            v = t_rings[r][edge_idx]
            inner_lapel.append(bm.verts.new((v.co.x - sgn * 0.006, v.co.y + 0.016, v.co.z)))
        for r in range(len(t_rings) - 1):
            if sgn == -1:
                f = bm.faces.new((t_rings[r][edge_idx], inner_lapel[r], inner_lapel[r + 1], t_rings[r + 1][edge_idx]))
            else:
                f = bm.faces.new((inner_lapel[r], t_rings[r][edge_idx], t_rings[r + 1][edge_idx], inner_lapel[r + 1]))
            f.material_index = 2  # Crisp dark trim/zipper edge

    # Seamless Sleeves
    for side in (1, -1):
        if side == 1:
            loop_verts = [
                t_rings[7][0], t_rings[7][1], t_rings[6][1], t_rings[5][1],
                t_rings[5][0], t_rings[5][15], t_rings[6][15], t_rings[7][15],
            ]
        else:
            loop_verts = [
                t_rings[7][8], t_rings[7][7], t_rings[6][7], t_rings[5][7],
                t_rings[5][8], t_rings[5][9], t_rings[6][9], t_rings[7][9],
            ]

        sleeve_rings_spec = [
            (0.232, 0.012, 1.348, 0.066, 0.065),
            (0.260, 0.014, 1.284, 0.056, 0.058),
            (0.283, 0.015, 1.212, 0.052, 0.053),
            (0.302, 0.014, 1.142, 0.048, 0.048),
            (0.318, 0.012, 1.092, 0.045, 0.045),
            (0.326, 0.008, 1.064, 0.044, 0.044),
            (0.334, 0.000, 1.030, 0.045, 0.044),
            (0.344, -0.014, 0.964, 0.041, 0.040),
            (0.354, -0.028, 0.900, 0.036, 0.035),
            (0.360, -0.038, 0.858, 0.034, 0.033),
        ]

        prev_loop = loop_verts
        for s_idx, (cx_abs, cy, cz, r_fb, r_ud) in enumerate(sleeve_rings_spec):
            p_prev = Vector((side * (0.198 if s_idx == 0 else sleeve_rings_spec[s_idx - 1][0]),
                             0.008 if s_idx == 0 else sleeve_rings_spec[s_idx - 1][1],
                             1.370 if s_idx == 0 else sleeve_rings_spec[s_idx - 1][2]))
            p_curr = Vector((side * cx_abs, cy, cz))
            if s_idx < len(sleeve_rings_spec) - 1:
                p_next = Vector((side * sleeve_rings_spec[s_idx + 1][0],
                                 sleeve_rings_spec[s_idx + 1][1],
                                 sleeve_rings_spec[s_idx + 1][2]))
                tang = (p_next - p_prev).normalized()
            else:
                tang = (p_curr - p_prev).normalized()

            world_back = Vector((0.0, 1.0, 0.0))
            u_back = (world_back - tang * world_back.dot(tang)).normalized()
            u_top = u_back.cross(tang).normalized() if side == 1 else tang.cross(u_back).normalized()
            if u_top.z < 0:
                u_top = -u_top

            new_loop = []
            for k in range(8):
                a = (2.0 * math.pi * k) / 8.0
                pt = p_curr + u_top * (math.cos(a) * r_ud) + u_back * (math.sin(a) * r_fb)
                new_loop.append(bm.verts.new(pt))

            for k in range(8):
                nk = (k + 1) % 8
                if side == 1:
                    bm.faces.new((prev_loop[k], new_loop[k], new_loop[nk], prev_loop[nk]))
                else:
                    bm.faces.new((prev_loop[nk], new_loop[nk], new_loop[k], prev_loop[k]))
            prev_loop = new_loop

        p_end = Vector((side * 0.361, -0.040, 0.862))
        cuff_in = [bm.verts.new(p_end + (v.co - p_end) * 0.72) for v in prev_loop]
        for k in range(8):
            nk = (k + 1) % 8
            if side == 1:
                bm.faces.new((prev_loop[k], cuff_in[k], cuff_in[nk], prev_loop[nk]))
            else:
                bm.faces.new((prev_loop[nk], cuff_in[nk], cuff_in[k], prev_loop[k]))

    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()

    jacket = bpy.data.objects.new("Jacket", mesh)
    bpy.context.collection.objects.link(jacket)
    jacket.data.materials.append(mats["jacket"])
    jacket.data.materials.append(mats["jacket_trim"])
    jacket.data.materials.append(mats["metal"])

    # Clean matte ribbed collar, waist hem & wrist cuffs (NO jagged diagonal triangles!)
    for poly in jacket.data.polygons:
        if poly.material_index == 2:
            continue  # keep metallic front zipper edge
        c = poly.center
        if c.z > 1.452 or c.z < 0.992 or (abs(c.x) > 0.345 and c.z < 0.892):
            poly.material_index = 1  # dark matte ribbed knit trim
        else:
            poly.material_index = 0  # matte black/charcoal leather

    apply_mod(jacket, "SUBSURF", levels=2, render_levels=2)
    set_smooth(jacket)
    return jacket, tee_obj


# =============================================================================
# 4. SEAMLESS QUAD-TOPOLOGY DARK INDIGO JEANS
# =============================================================================
def build_seamless_jeans(mats):
    mesh = bpy.data.meshes.new("Jeans_Mesh")
    bm = bmesh.new()

    n_pelv = 12
    pelv_spec = [
        (0.968, 0.157, 0.105, 0.115, 0.006),
        (0.922, 0.169, 0.109, 0.129, 0.008),
        (0.874, 0.175, 0.109, 0.133, 0.008),
        (0.824, 0.171, 0.105, 0.123, 0.006),
    ]

    p_rings = []
    for (z, rx, ryf, ryb, cy) in pelv_spec:
        ring = []
        for i in range(n_pelv):
            ang = (2.0 * math.pi * i) / n_pelv
            sx = math.sin(ang)
            cy_dir = -math.cos(ang)
            ry = ryb if cy_dir >= 0.0 else ryf
            x = rx * sx
            y = cy + ry * cy_dir
            if i == 6 and z < 0.94:
                y -= 0.012
            ring.append(bm.verts.new((x, y, z)))
        p_rings.append(ring)

    for r in range(len(p_rings) - 1):
        r0, r1 = p_rings[r], p_rings[r + 1]
        for i in range(n_pelv):
            ni = (i + 1) % n_pelv
            bm.faces.new((r0[ni], r0[i], r1[i], r1[ni]))

    top_c = bm.verts.new((0.0, 0.006, 0.974))
    for i in range(n_pelv):
        ni = (i + 1) % n_pelv
        bm.faces.new((p_rings[0][i], p_rings[0][ni], top_c))

    bot = p_rings[-1]
    c_mid = bm.verts.new((0.0, 0.010, 0.808))

    leg_spec = [
        (0.758, 0.095,  0.004, 0.083, 0.089),
        (0.678, 0.096,  0.000, 0.075, 0.081),
        (0.594, 0.096, -0.004, 0.065, 0.069),
        (0.535, 0.096, -0.006, 0.059, 0.061),
        (0.495, 0.096, -0.008, 0.056, 0.059),
        (0.455, 0.096, -0.002, 0.055, 0.057),
        (0.370, 0.096,  0.012, 0.057, 0.065),
        (0.275, 0.096,  0.010, 0.049, 0.055),
        (0.175, 0.096,  0.006, 0.044, 0.048),
        (0.090, 0.096,  0.005, 0.047, 0.051),
    ]

    for side in (1, -1):
        prev_loop = ([bot[0], bot[1], bot[2], bot[3], bot[4], bot[5], bot[6], c_mid]
                     if side == 1 else
                     [bot[0], bot[11], bot[10], bot[9], bot[8], bot[7], bot[6], c_mid])

        for (z, cx_abs, cy, rx, ry) in leg_spec:
            new_loop = []
            angles = [-2.25, -1.55, -0.78, 0.0, 0.78, 1.55, 2.25, math.pi]
            for a in angles:
                vx = side * (cx_abs + math.cos(a) * rx)
                vy = cy + math.sin(a) * ry
                new_loop.append(bm.verts.new((vx, vy, z)))

            for k in range(8):
                nk = (k + 1) % 8
                if side == 1:
                    bm.faces.new((prev_loop[k], prev_loop[nk], new_loop[nk], new_loop[k]))
                else:
                    bm.faces.new((prev_loop[nk], prev_loop[k], new_loop[k], new_loop[nk]))
            prev_loop = new_loop

        cuff_in = [bm.verts.new((side * 0.096 + (v.co.x - side * 0.096) * 0.72,
                                 0.005 + (v.co.y - 0.005) * 0.72,
                                 v.co.z + 0.012)) for v in prev_loop]
        for k in range(8):
            nk = (k + 1) % 8
            if side == 1:
                bm.faces.new((prev_loop[k], prev_loop[nk], cuff_in[nk], cuff_in[k]))
            else:
                bm.faces.new((prev_loop[nk], prev_loop[k], cuff_in[k], cuff_in[nk]))

    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()

    jeans = bpy.data.objects.new("Jeans", mesh)
    bpy.context.collection.objects.link(jeans)
    jeans.data.materials.append(mats["jeans"])
    apply_mod(jeans, "SUBSURF", levels=2, render_levels=2)
    set_smooth(jeans)
    return jeans


# =============================================================================
# 5. BELT, CLEAN SCULPTED HANDS, AND SNEAKERS
# =============================================================================
def build_belt(mats):
    parts = []
    bm = bmesh.new()
    n_seg = 28
    z_bot, z_top = 0.940, 0.972
    rx, ryf, ryb, cy = 0.164, 0.110, 0.119, 0.008
    r_bot, r_top = [], []
    for i in range(n_seg):
        a = (2.0 * math.pi * i) / n_seg
        ca, sa = math.cos(a), math.sin(a)
        ry = ryb if sa >= 0 else ryf
        r_bot.append(bm.verts.new((rx * ca, cy + ry * sa, z_bot)))
        r_top.append(bm.verts.new((rx * ca, cy + ry * sa, z_top)))
    for i in range(n_seg):
        ni = (i + 1) % n_seg
        bm.faces.new((r_bot[i], r_bot[ni], r_top[ni], r_top[i]))
    bm.normal_update()
    mesh = bpy.data.meshes.new("Belt_Mesh")
    bm.to_mesh(mesh)
    bm.free()
    belt = bpy.data.objects.new("Belt", mesh)
    bpy.context.collection.objects.link(belt)
    belt.data.materials.append(mats["belt"])
    apply_mod(belt, "SOLIDIFY", thickness=0.006, offset=1.0)
    apply_mod(belt, "SUBSURF", levels=1, render_levels=1)
    set_smooth(belt)
    parts.append(belt)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, -0.107, 0.956))
    buckle = bpy.context.active_object
    buckle.name = "BeltBuckle"
    buckle.scale = (0.044, 0.011, 0.036)
    bpy.ops.object.transform_apply(scale=True)
    apply_mod(buckle, "BEVEL", width=0.004, segments=2)
    buckle.data.materials.append(mats["metal"])
    set_smooth(buckle)
    parts.append(buckle)
    return parts


def add_capsule_to_bm(bm, pts_r, n_s=8):
    rings = []
    for idx, (pt, rad) in enumerate(pts_r):
        if idx == 0:
            tang = (pts_r[1][0] - pt).normalized()
        elif idx == len(pts_r) - 1:
            tang = (pt - pts_r[idx - 1][0]).normalized()
        else:
            tang = (pts_r[idx + 1][0] - pts_r[idx - 1][0]).normalized()
        up_ref = Vector((1.0, 0.0, 0.0)) if abs(tang.x) < 0.8 else Vector((0.0, 1.0, 0.0))
        u = tang.cross(up_ref).normalized()
        v = tang.cross(u).normalized()
        ring = []
        for k in range(n_s):
            a = (2.0 * math.pi * k) / n_s
            ring.append(bm.verts.new(pt + u * (math.cos(a) * rad) + v * (math.sin(a) * rad)))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for k in range(n_s):
            nk = (k + 1) % n_s
            bm.faces.new((rings[i][k], rings[i][nk], rings[i + 1][nk], rings[i + 1][k]))
    t0 = (pts_r[1][0] - pts_r[0][0]).normalized()
    t1 = (pts_r[-1][0] - pts_r[-2][0]).normalized()
    c0 = bm.verts.new(pts_r[0][0] - t0 * (pts_r[0][1] * 0.6))
    c1 = bm.verts.new(pts_r[-1][0] + t1 * (pts_r[-1][1] * 0.6))
    for k in range(n_s):
        nk = (k + 1) % n_s
        bm.faces.new((rings[0][nk], rings[0][k], c0))
        bm.faces.new((rings[-1][k], rings[-1][nk], c1))


def build_hands(mats):
    hands = []
    for side in (1, -1):
        bm = bmesh.new()
        wx, wy, wz = side * 0.360, -0.038, 0.864

        palm_spec = [
            (0.000,  0.000,  0.012, 0.025, 0.026),
            (0.002, -0.002, -0.008, 0.023, 0.029),
            (0.006, -0.005, -0.036, 0.019, 0.034),
            (0.009, -0.008, -0.072, 0.016, 0.038),
            (0.007, -0.010, -0.104, 0.013, 0.035),
        ]
        n_s = 12
        p_rings = []
        for (dx, dy, dz, r_thick, r_width) in palm_spec:
            ring = []
            for i in range(n_s):
                a = (2.0 * math.pi * i) / n_s
                ring.append(bm.verts.new((wx + side * dx + math.cos(a) * r_thick,
                                          wy + dy + math.sin(a) * r_width,
                                          wz + dz)))
            p_rings.append(ring)
        for r in range(len(p_rings) - 1):
            for i in range(n_s):
                ni = (i + 1) % n_s
                bm.faces.new((p_rings[r][i], p_rings[r][ni], p_rings[r + 1][ni], p_rings[r + 1][i]))
        wc = bm.verts.new((wx, wy, wz + 0.018))
        kc = bm.verts.new((wx + side * 0.007, wy - 0.010, wz - 0.109))
        for i in range(n_s):
            ni = (i + 1) % n_s
            bm.faces.new((p_rings[0][ni], p_rings[0][i], wc))
            bm.faces.new((p_rings[-1][i], p_rings[-1][ni], kc))

        finger_defs = [
            (-0.025, 0.068, 0.0080),
            (-0.008, 0.075, 0.0084),
            ( 0.010, 0.070, 0.0080),
            ( 0.025, 0.056, 0.0068),
        ]
        for (fy_off, flen, frad) in finger_defs:
            pts = []
            for seg in range(5):
                t = seg / 4.0
                fx = wx + side * (0.007 - 0.020 * (t ** 1.7))
                fy = wy - 0.010 + fy_off
                fz = wz - 0.100 - flen * t
                pts.append((Vector((fx, fy, fz)), frad * (1.0 - 0.25 * t)))
            add_capsule_to_bm(bm, pts, n_s=8)

        thumb_pts = [
            (Vector((wx + side * 0.002, wy - 0.026, wz - 0.044)), 0.0115),
            (Vector((wx + side * -0.005, wy - 0.046, wz - 0.066)), 0.0102),
            (Vector((wx + side * -0.013, wy - 0.062, wz - 0.088)), 0.0090),
            (Vector((wx + side * -0.018, wy - 0.070, wz - 0.105)), 0.0076),
        ]
        add_capsule_to_bm(bm, thumb_pts, n_s=8)

        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        hmesh = bpy.data.meshes.new(f"Hand_{side}_Mesh")
        bm.to_mesh(hmesh)
        bm.free()
        hobj = bpy.data.objects.new(f"Hand_{'L' if side > 0 else 'R'}", hmesh)
        bpy.context.collection.objects.link(hobj)
        hobj.data.materials.append(mats["skin"])
        apply_mod(hobj, "SUBSURF", levels=1, render_levels=1)
        set_smooth(hobj)
        hands.append((hobj, side))
    return hands


def build_sneakers(mats):
    shoes = []
    for side in (1, -1):
        cx = side * 0.097
        bm = bmesh.new()

        shoe_slices = [
            ( 0.076, 0.006, 0.095, 0.032),
            ( 0.048, 0.000, 0.116, 0.044),
            ( 0.005, 0.000, 0.118, 0.046),
            (-0.040, 0.000, 0.084, 0.048),
            (-0.092, 0.000, 0.056, 0.050),
            (-0.140, 0.002, 0.038, 0.045),
            (-0.174, 0.008, 0.024, 0.029),
        ]
        n_p = 14
        s_rings = []
        for (y, zb, zt, hw) in shoe_slices:
            ring = []
            zc = 0.5 * (zb + zt)
            hz = 0.5 * (zt - zb)
            for i in range(n_p):
                a = (2.0 * math.pi * i) / n_p
                ca, sa = math.cos(a), math.sin(a)
                vx = cx + ca * hw * (1.05 if sa < 0 else 0.94)
                vz = zb if sa < -0.35 else zc + sa * hz
                ring.append(bm.verts.new((vx, y, vz)))
            s_rings.append(ring)

        for r in range(len(s_rings) - 1):
            for i in range(n_p):
                ni = (i + 1) % n_p
                bm.faces.new((s_rings[r][i], s_rings[r][ni], s_rings[r + 1][ni], s_rings[r + 1][i]))

        hc = bm.verts.new((cx, 0.082, 0.048))
        tc = bm.verts.new((cx, -0.181, 0.015))
        for i in range(n_p):
            ni = (i + 1) % n_p
            bm.faces.new((s_rings[0][ni], s_rings[0][i], hc))
            bm.faces.new((s_rings[-1][i], s_rings[-1][ni], tc))

        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        smesh = bpy.data.meshes.new(f"Sneaker_{side}_Mesh")
        bm.to_mesh(smesh)
        bm.free()

        sobj = bpy.data.objects.new(f"Sneaker_{'L' if side > 0 else 'R'}", smesh)
        bpy.context.collection.objects.link(sobj)
        sobj.data.materials.append(mats["shoe_upper"])
        sobj.data.materials.append(mats["shoe_sole"])
        sobj.data.materials.append(mats["accent"])

        for poly in sobj.data.polygons:
            c = poly.center
            if c.z < 0.026:
                poly.material_index = 1
            elif 0.045 < c.z < 0.075 and -0.055 < c.y < 0.025 and abs(c.x - cx) > 0.036:
                poly.material_index = 2
            else:
                poly.material_index = 0

        apply_mod(sobj, "SUBSURF", levels=1, render_levels=1)
        min_z = min(v.co.z for v in sobj.data.vertices)
        for v in sobj.data.vertices:
            v.co.z = max(0.0, v.co.z - min_z)
        sobj.data.update()
        set_smooth(sobj)
        shoes.append((sobj, side))
    return shoes


# =============================================================================
# 6. 22-BONE HUMANOID ARMATURE & CLEAN SKINNING
# =============================================================================
def build_armature():
    arm_data = bpy.data.armatures.new("HumanArmature_Data")
    arm_obj = bpy.data.objects.new("HumanArmature", arm_data)
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

    add_bone("Hips",  (0.0, 0.008, 0.935), (0.0, 0.008, 1.045), None, False, (0.0, -1.0, 0.0))
    add_bone("Spine", (0.0, 0.008, 1.045), (0.0, 0.006, 1.215), "Hips", True, (0.0, -1.0, 0.0))
    add_bone("Chest", (0.0, 0.006, 1.215), (0.0, 0.010, 1.430), "Spine", True, (0.0, -1.0, 0.0))
    add_bone("Neck",  (0.0, 0.010, 1.430), (0.0, 0.008, 1.535), "Chest", True, (0.0, -1.0, 0.0))
    add_bone("Head",  (0.0, 0.008, 1.535), (0.0, 0.005, 1.750), "Neck", True, (0.0, -1.0, 0.0))

    for side, sfx in ((1, "L"), (-1, "R")):
        add_bone(f"Shoulder.{sfx}", (side * 0.065, 0.008, 1.380), (side * 0.214, 0.012, 1.360), "Chest", False, (0.0, -1.0, 0.0))
        add_bone(f"UpperArm.{sfx}", (side * 0.214, 0.012, 1.360), (side * 0.326, 0.008, 1.064), f"Shoulder.{sfx}", True, (0.0, -1.0, 0.0))
        add_bone(f"Forearm.{sfx}",  (side * 0.326, 0.008, 1.064), (side * 0.360, -0.038, 0.858), f"UpperArm.{sfx}", True, (0.0, -1.0, 0.0))
        add_bone(f"Hand.{sfx}",     (side * 0.360, -0.038, 0.858), (side * 0.365, -0.048, 0.720), f"Forearm.{sfx}", True, (0.0, -1.0, 0.0))

        add_bone(f"UpperLeg.{sfx}", (side * 0.096, 0.006, 0.895), (side * 0.096, -0.008, 0.495), "Hips", False, (0.0, -1.0, 0.0))
        add_bone(f"LowerLeg.{sfx}", (side * 0.096, -0.008, 0.495), (side * 0.096, 0.006, 0.090), f"UpperLeg.{sfx}", True, (0.0, -1.0, 0.0))
        add_bone(f"Foot.{sfx}",     (side * 0.096, 0.006, 0.090), (side * 0.096, -0.155, 0.020), f"LowerLeg.{sfx}", True, (0.0, 0.0, 1.0))

    bpy.ops.object.mode_set(mode="OBJECT")
    return arm_obj


def skin_mesh_rigid(obj, arm_obj, bone_name):
    obj.parent = arm_obj
    mod = obj.modifiers.new(name="Armature", type="ARMATURE")
    mod.object = arm_obj
    vg = obj.vertex_groups.new(name=bone_name)
    vg.add([v.index for v in obj.data.vertices], 1.0, "REPLACE")


def skin_mesh_auto(obj, arm_obj, fallback_bone="Chest"):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")

    arm_bones = {b.name: (arm_obj.matrix_world @ b.head_local, arm_obj.matrix_world @ b.tail_local)
                 for b in arm_obj.data.bones}
    vgroups = obj.vertex_groups
    unweighted = 0
    for v in obj.data.vertices:
        wsum = sum(g.weight for g in v.groups)
        if wsum < 1e-4:
            unweighted += 1
            p = obj.matrix_world @ v.co
            best_b = fallback_bone
            best_d = 1e9
            for bname, (h, t) in arm_bones.items():
                d = (p - (h + t) * 0.5).length
                if d < best_d:
                    best_d = d
                    best_b = bname
            vg = vgroups.get(best_b) or vgroups.new(name=best_b)
            vg.add([v.index], 1.0, "REPLACE")

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    obj.data.validate(verbose=False)
    print(f"[SAGE][Rig] Skinned {obj.name} ({len(obj.data.vertices)} verts, fixed {unweighted} unweighted)")


# =============================================================================
# 7. FIVE BIOMECHANICAL SKELETAL ANIMATION CLIPS (SWIM, JUMP, RUN, WALK, IDLE LAST)
# =============================================================================
def sample_periodic(keys, phase):
    """Smooth Catmull-Rom periodic interpolation over [(p0, v0), ...] with p in [0, 1)."""
    p = phase % 1.0
    n = len(keys)
    # Find segment i such that keys[i][0] <= p < keys[i+1][0] (wrapping around 1.0)
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
    # Catmull-Rom spline for natural biomechanical acceleration/deceleration!
    u2 = u * u
    u3 = u2 * u
    return 0.5 * (
        (2.0 * v0)
        + (-vm1 + v1) * u
        + (2.0 * vm1 - 5.0 * v0 + 4.0 * v1 - v2) * u2
        + (-vm1 + 3.0 * v0 - 3.0 * v1 + v2) * u3
    )


def build_animations(arm_obj):
    bpy.ops.object.select_all(action="DESELECT")
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="POSE")

    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "XYZ"

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

    ARM_DROP = 0.17

    # --- 1. SWIM (Smooth Freestyle Crawl + Flutter Kick) ---
    def swim_pose(phase):
        t = phase * 2.0 * math.pi
        s, c = math.sin(t), math.cos(t)
        set_loc("Hips", 0.0, 0.0, -0.04)
        set_rot("Hips", 1.32, 0.0, 0.16 * s)
        set_rot("Spine", -0.14, 0.0, -0.08 * s)
        set_rot("Chest", -0.16, 0.0, 0.14 * s)
        set_rot("Neck", -0.46, 0.0, 0.0)
        set_rot("Head", -0.34, 0.0, 0.0)
        set_rot("UpperArm.L", 1.55 + 1.15 * s, 0.0, -0.10)
        set_rot("Forearm.L",  0.42 + 0.42 * max(0.0, -c), 0.0, 0.0)
        set_rot("UpperArm.R", 1.55 - 1.15 * s, 0.0,  0.10)
        set_rot("Forearm.R",  0.42 + 0.42 * max(0.0, c), 0.0, 0.0)
        set_rot("UpperLeg.L", 0.28 * math.sin(2.0 * t), 0.0, 0.0)
        set_rot("LowerLeg.L", -0.20 - 0.22 * max(0.0, math.sin(2.0 * t)), 0.0, 0.0)
        set_rot("Foot.L", -0.45, 0.0, 0.0)
        set_rot("UpperLeg.R", -0.28 * math.sin(2.0 * t), 0.0, 0.0)
        set_rot("LowerLeg.R", -0.20 - 0.22 * max(0.0, -math.sin(2.0 * t)), 0.0, 0.0)
        set_rot("Foot.R", -0.45, 0.0, 0.0)

    # --- 2. JUMP (Athletic Airborne Stride Tuck + Arm Balance!) ---
    def jump_pose(phase):
        t = phase * 2.0 * math.pi
        sway = math.sin(t)
        set_loc("Hips", 0.0, 0.04 + 0.015 * sway, 0.0)
        set_rot("Hips", 0.12, -0.06, 0.0)
        set_rot("Spine", 0.08, 0.04, 0.0)
        set_rot("Chest", 0.06, 0.06, 0.0)
        set_rot("Neck", -0.10, -0.04, 0.0)
        set_rot("Head", -0.08, -0.04, 0.0)

        set_rot("UpperLeg.L", 0.62 + 0.06 * sway, 0.0, 0.0)
        set_rot("LowerLeg.L", -1.10 - 0.08 * sway, 0.0, 0.0)
        set_rot("Foot.L", -0.22, 0.0, 0.0)

        set_rot("UpperLeg.R", -0.18 - 0.05 * sway, 0.0, 0.0)
        set_rot("LowerLeg.R", -0.78 + 0.06 * sway, 0.0, 0.0)
        set_rot("Foot.R", -0.35, 0.0, 0.0)

        set_rot("UpperArm.L", 0.55 + 0.08 * sway, 0.0, 0.22)
        set_rot("Forearm.L",  0.88, 0.0, 0.0)
        set_rot("UpperArm.R", 0.42 - 0.08 * sway, 0.0, -0.22)
        set_rot("Forearm.R",  0.95, 0.0, 0.0)

    # --- 3. RUN (Biomechanical Athletic Sprint — Flight Phase, High Knee Drive, Heel Kick, 90° Pumping Arms!) ---
    run_thigh_keys = [
        (0.00,  0.64),  # Foot Strike
        (0.14,  0.18),  # Mid-Stance Compression
        (0.30, -0.58),  # Terminal Push-Off
        (0.45, -0.72),  # Flight Trailing Extension
        (0.62,  0.14),  # Recovery Forward Drive
        (0.78,  0.96),  # High Knee Peak!
        (0.90,  0.80),  # Strike Preparation
    ]
    run_knee_keys = [
        (0.00, -0.30),  # Cushioned Midfoot Strike
        (0.14, -0.56),  # Springy Stance Load
        (0.32, -0.14),  # Explosive Push-Off Extension
        (0.52, -1.72),  # Athletic Heel-to-Glute Fold!
        (0.72, -1.28),  # High-Knee Tuck
        (0.88, -0.46),  # Extend for Landing
    ]
    run_foot_keys = [
        (0.00,  0.16),  # Slight Dorsiflex Strike
        (0.15,  0.20),  # Ankle Load
        (0.34, -0.48),  # Powerful Toe-Off Plantarflexion!
        (0.58, -0.22),  # Swing Relax
        (0.82,  0.18),  # Prep Dorsiflex
    ]
    run_arm_keys = [
        (0.00, -0.76),  # Backswing when same-side leg strikes forward
        (0.16, -0.48),
        (0.34,  0.18),
        (0.50,  0.84),  # Strong Forward Punch!
        (0.68,  0.52),
        (0.84, -0.22),
    ]
    run_elbow_keys = [
        (0.00,  1.22),  # Slightly open on rear swing
        (0.25,  1.38),
        (0.50,  1.68),  # Crisp ~95° athletic pump up toward chest!
        (0.75,  1.42),
    ]

    def run_pose(phase):
        pL = phase
        pR = (phase + 0.5) % 1.0
        t = phase * 2.0 * math.pi
        s = math.sin(t)
        # Double-frequency flight bounce: dips at p=0.14 & 0.64 (stance load), peaks at p=0.40 & 0.90 (flight!)
        bounce = -0.044 * math.sin((phase * 2.0 + 0.22) * 2.0 * math.pi) - 0.015
        sway_x = 0.016 * math.sin((phase - 0.14) * 2.0 * math.pi)
        pelv_yaw = 0.17 * math.cos(t)  # Right/left hip leads when that leg reaches forward!
        pelv_roll = 0.065 * math.sin((phase - 0.14) * 2.0 * math.pi)

        set_loc("Hips", sway_x, bounce, 0.0)
        set_rot("Hips", 0.16, pelv_yaw, pelv_roll)
        set_rot("Spine", 0.12, -pelv_yaw * 0.85, -pelv_roll * 0.6)
        set_rot("Chest", 0.10, -pelv_yaw * 1.15, -pelv_roll * 0.4)
        set_rot("Neck", -0.18, pelv_yaw * 0.55, 0.0)
        set_rot("Head", -0.14, pelv_yaw * 0.45, 0.0)

        set_rot("Shoulder.L", 0.0,  0.08 * math.cos(t), 0.0)
        set_rot("Shoulder.R", 0.0,  0.08 * math.cos(t), 0.0)

        set_rot("UpperLeg.L", sample_periodic(run_thigh_keys, pL), 0.0, 0.0)
        set_rot("LowerLeg.L", sample_periodic(run_knee_keys, pL), 0.0, 0.0)
        set_rot("Foot.L",     sample_periodic(run_foot_keys, pL), 0.0, 0.0)

        set_rot("UpperLeg.R", sample_periodic(run_thigh_keys, pR), 0.0, 0.0)
        set_rot("LowerLeg.R", sample_periodic(run_knee_keys, pR), 0.0, 0.0)
        set_rot("Foot.R",     sample_periodic(run_foot_keys, pR), 0.0, 0.0)

        set_rot("UpperArm.L", sample_periodic(run_arm_keys, pL), 0.0, -ARM_DROP + 0.04)
        set_rot("Forearm.L",  sample_periodic(run_elbow_keys, pL), 0.0, 0.0)
        set_rot("UpperArm.R", sample_periodic(run_arm_keys, pR), 0.0,  ARM_DROP - 0.04)
        set_rot("Forearm.R",  sample_periodic(run_elbow_keys, pR), 0.0, 0.0)

    # --- 4. WALK (True 8-Phase Biomechanical Human Gait — Heel Strike, Cushion, Vault, Push-Off, Swing!) ---
    walk_thigh_keys = [
        (0.00,  0.44),  # Heel Strike
        (0.12,  0.34),  # Loading Response
        (0.28,  0.02),  # Mid-Stance Vault
        (0.48, -0.35),  # Terminal Stance Push-Off
        (0.62, -0.22),  # Pre-Swing Lift
        (0.78,  0.25),  # Mid-Swing Passing
        (0.90,  0.48),  # Terminal Swing Reach
    ]
    walk_knee_keys = [
        (0.00, -0.07),  # Soft Straight Heel Strike
        (0.12, -0.24),  # Realistic Impact Shock-Absorption Bend!
        (0.30, -0.08),  # Vault Straightens Over Stance Leg
        (0.50, -0.26),  # Heel Lift
        (0.66, -0.92),  # Clean Swing Knee Clearance!
        (0.80, -0.64),  # Mid-Swing Unfold
        (0.92, -0.10),  # Reach Extension
    ]
    walk_foot_keys = [
        (0.00,  0.22),  # Heel Strike — Toes UP!
        (0.12, -0.03),  # Plant Flat on Ground
        (0.32,  0.07),  # Tibia Rolls Forward Over Foot
        (0.52, -0.34),  # Push-Off — Toes Push Down!
        (0.68, -0.12),  # Clear Ground
        (0.86,  0.16),  # Prepare Dorsiflex
    ]
    walk_arm_keys = [
        (0.00, -0.34),  # Natural counter-swing back
        (0.18, -0.22),
        (0.36,  0.08),
        (0.52,  0.38),  # Natural forward swing
        (0.72,  0.20),
        (0.88, -0.14),
    ]
    walk_elbow_keys = [
        (0.00,  0.14),  # Relaxed extension on backswing
        (0.25,  0.24),
        (0.52,  0.48),  # Soft natural bend on forward swing
        (0.78,  0.28),
    ]

    def walk_pose(phase):
        pL = phase
        pR = (phase + 0.5) % 1.0
        t = phase * 2.0 * math.pi
        # Classic inverted-pendulum walk bob: highest at mid-stance (p=0.28 & 0.78), lowest at contact (p=0.06 & 0.56)!
        bounce = -0.026 * math.cos((phase * 2.0 - 0.08) * 2.0 * math.pi)
        sway_x = 0.020 * math.sin((phase - 0.04) * 2.0 * math.pi)  # Weight shifts smoothly onto stance leg!
        pelv_yaw = 0.11 * math.cos(t)
        pelv_roll = 0.048 * math.sin((phase - 0.04) * 2.0 * math.pi)  # Subtle hip drop on swing side!

        set_loc("Hips", sway_x, bounce, 0.0)
        set_rot("Hips", 0.04, pelv_yaw, pelv_roll)
        set_rot("Spine", 0.03, -pelv_yaw * 0.75, -pelv_roll * 0.7)
        set_rot("Chest", 0.02, -pelv_yaw * 1.05, -pelv_roll * 0.4)
        set_rot("Neck", -0.04, pelv_yaw * 0.45, 0.0)
        set_rot("Head", -0.03, pelv_yaw * 0.35, 0.0)

        set_rot("UpperLeg.L", sample_periodic(walk_thigh_keys, pL), 0.0, 0.0)
        set_rot("LowerLeg.L", sample_periodic(walk_knee_keys, pL), 0.0, 0.0)
        set_rot("Foot.L",     sample_periodic(walk_foot_keys, pL), 0.0, 0.0)

        set_rot("UpperLeg.R", sample_periodic(walk_thigh_keys, pR), 0.0, 0.0)
        set_rot("LowerLeg.R", sample_periodic(walk_knee_keys, pR), 0.0, 0.0)
        set_rot("Foot.R",     sample_periodic(walk_foot_keys, pR), 0.0, 0.0)

        set_rot("UpperArm.L", sample_periodic(walk_arm_keys, pL), 0.0, -ARM_DROP)
        set_rot("Forearm.L",  sample_periodic(walk_elbow_keys, pL), 0.0, 0.0)
        set_rot("UpperArm.R", sample_periodic(walk_arm_keys, pR), 0.0,  ARM_DROP)
        set_rot("Forearm.R",  sample_periodic(walk_elbow_keys, pR), 0.0, 0.0)

    # --- 5. IDLE (Relaxed Natural Breathing & Weight Shift — RECORDED LAST as Upright Bind Pose!) ---
    def idle_pose(phase):
        t = phase * 2.0 * math.pi
        b = math.sin(t)
        s2 = math.sin(t * 0.5)
        set_loc("Hips", 0.008 * s2, 0.004 * b, 0.0)
        set_rot("Hips", 0.0, 0.02 * s2, 0.015 * s2)
        set_rot("Spine", -0.014 * b, -0.01 * s2, -0.01 * s2)
        set_rot("Chest", -0.020 * b, -0.01 * s2, 0.0)
        set_rot("Head", 0.015 * b, 0.07 * s2, 0.015 * b)
        set_rot("UpperLeg.L", 0.02, 0.0, 0.0)
        set_rot("LowerLeg.L", -0.04, 0.0, 0.0)
        set_rot("UpperLeg.R", -0.02, 0.0, 0.0)
        set_rot("LowerLeg.R", -0.04, 0.0, 0.0)
        set_rot("UpperArm.L", 0.03 + 0.015 * b, 0.0, -ARM_DROP)
        set_rot("Forearm.L",  0.16 + 0.02 * b, 0.0, 0.0)
        set_rot("UpperArm.R", 0.03 - 0.015 * b, 0.0,  ARM_DROP)
        set_rot("Forearm.R",  0.16 + 0.02 * b, 0.0, 0.0)

    record_clip("Swim", 28, swim_pose)
    record_clip("Jump", 24, jump_pose)
    run_action = record_clip("Run", 24, run_pose)
    walk_action = record_clip("Walk", 32, walk_pose)
    idle_action = record_clip("Idle", 48, idle_pose)

    bpy.ops.object.mode_set(mode="OBJECT")
    return idle_action, walk_action, run_action


# =============================================================================
# 8. STUDIO CYCLES LIGHTING & 5-ANGLE PORTRAIT/FULL-BODY RENDERS
# =============================================================================
def setup_studio_and_render(arm_obj, idle_action, walk_action, run_action):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 28
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 768
    scene.render.resolution_y = 960
    scene.render.image_settings.file_format = "PNG"

    world = bpy.data.worlds.new("StudioWorld")
    scene.world = world
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (*srgb_to_linear((0.18, 0.21, 0.26)), 1.0)
    bg.inputs["Strength"].default_value = 0.55

    bpy.ops.mesh.primitive_plane_add(size=10.0, location=(0.0, 0.0, 0.0))
    floor = bpy.context.active_object
    floor.name = "StudioFloor_TMP"
    floor.data.materials.append(make_mat("FloorMat", (0.24, 0.26, 0.30), roughness=0.78))

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
        add_area_light("KeyLight_TMP",  (-1.9, -2.1, 2.35), (0.0, 0.0, 1.35), 210.0, 1.1, (1.0, 0.95, 0.89)),
        add_area_light("FillLight_TMP", ( 2.3, -1.6, 1.55), (0.0, 0.0, 1.25),  65.0, 2.2, (0.80, 0.89, 1.0)),
        add_area_light("RimLight_TMP",  ( 0.7,  2.1, 2.25), (0.0, 0.0, 1.48), 210.0, 1.0, (0.90, 0.95, 1.0)),
    ]

    cam_data = bpy.data.cameras.new("StudioCam")
    cam_obj = bpy.data.objects.new("StudioCam_TMP", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    def render_shot(filename, cam_loc, target_loc, lens=65.0):
        cam_data.lens = lens
        cam_obj.location = Vector(cam_loc)
        cam_obj.rotation_euler = (Vector(target_loc) - Vector(cam_loc)).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(ARTIFACT_DIR, filename)
        bpy.ops.render.render(write_still=True)

    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = True

    # Verify Biomechanical Walk (frame 1 = heel strike, frame 11 = mid-stance passing!) & Run (frame 19 = high-knee flight!)
    arm_obj.animation_data.action = walk_action
    scene.frame_set(1)
    bpy.context.view_layer.update()
    render_shot("human_walk_pose.png",    (-2.45, -1.35, 1.04), (0.0, 0.0, 0.89), lens=56.0)

    arm_obj.animation_data.action = run_action
    scene.frame_set(19)
    bpy.context.view_layer.update()
    render_shot("human_run_pose.png",     (-2.25, -1.65, 1.04), (0.0, 0.0, 0.89), lens=54.0)

    arm_obj.animation_data.action = None
    for tr in arm_obj.animation_data.nla_tracks:
        tr.mute = False
    scene.frame_set(1)

    for tmp in [floor, cam_obj] + lights:
        bpy.data.objects.remove(tmp, do_unlink=True)


# =============================================================================
# MAIN
# =============================================================================
def main():
    clear_scene()

    # True artist-calibrated sRGB palette (automatically converted to linear PBR!)
    mats = {
        "skin":        make_mat("HumanSkin",       (0.76, 0.56, 0.44), roughness=0.48, sss=0.22, bump_scale=200.0, bump_strength=0.04, specular=0.35),
        "hair":        make_mat("HumanHair",       (0.16, 0.11, 0.08), roughness=0.70, sheen=0.15, bump_scale=240.0, bump_strength=0.14, specular=0.22),
        "sclera":      make_mat("EyeSclera",       (0.92, 0.90, 0.88), roughness=0.08, coat=1.0, specular=0.5),
        "iris":        make_mat("EyeIris",         (0.36, 0.22, 0.12), roughness=0.05, coat=1.0, specular=0.5),
        "iris_ring":   make_mat("EyeLimbalRing",   (0.10, 0.07, 0.05), roughness=0.05, coat=1.0, specular=0.5),
        "pupil":       make_mat("EyePupil",        (0.02, 0.02, 0.02), roughness=0.03, coat=1.0, specular=0.5),
        "jacket":      make_mat("LeatherJacket",   (0.15, 0.16, 0.18), roughness=0.60, coat=0.05, bump_scale=180.0, bump_strength=0.12, specular=0.28),
        "jacket_trim": make_mat("JacketRibTrim",   (0.10, 0.11, 0.12), roughness=0.86, sheen=0.22, bump_scale=260.0, bump_strength=0.16, specular=0.15),
        "shirt":       make_mat("InnerTee",        (0.86, 0.87, 0.89), roughness=0.88, sheen=0.20, bump_scale=260.0, bump_strength=0.08, specular=0.15),
        "accent":      make_mat("SneakerAccentRed",(0.72, 0.14, 0.14), roughness=0.50, bump_scale=160.0, bump_strength=0.08),
        "belt":        make_mat("LeatherBelt",     (0.24, 0.14, 0.08), roughness=0.46, bump_scale=150.0, bump_strength=0.10),
        "metal":       make_mat("BrushedSteel",    (0.75, 0.77, 0.80), roughness=0.24, metallic=0.92),
        "jeans":       make_mat("DenimJeans",      (0.17, 0.25, 0.39), roughness=0.90, sheen=0.32, bump_scale=240.0, bump_strength=0.18, specular=0.12),
        "shoe_upper":  make_mat("SneakerUpper",    (0.18, 0.19, 0.21), roughness=0.70, bump_scale=200.0, bump_strength=0.12, specular=0.20),
        "shoe_sole":   make_mat("SneakerSole",     (0.90, 0.90, 0.92), roughness=0.58, specular=0.22),
    }

    head_neck = build_anatomical_head_and_neck(mats)
    head_details = build_head_details(mats)
    jacket, inner_tee = build_seamless_jacket_and_shirt(mats)
    jeans = build_seamless_jeans(mats)
    belt_parts = build_belt(mats)
    hands = build_hands(mats)
    shoes = build_sneakers(mats)

    arm_obj = build_armature()

    skin_mesh_auto(head_neck, arm_obj, fallback_bone="Head")
    skin_mesh_auto(jacket, arm_obj, fallback_bone="Chest")
    skin_mesh_auto(inner_tee, arm_obj, fallback_bone="Chest")
    skin_mesh_auto(jeans, arm_obj, fallback_bone="Hips")

    for part in head_details:
        skin_mesh_rigid(part, arm_obj, "Head")
    for part in belt_parts:
        skin_mesh_rigid(part, arm_obj, "Hips")
    for hobj, side in hands:
        skin_mesh_rigid(hobj, arm_obj, "Hand.L" if side > 0 else "Hand.R")
    for sobj, side in shoes:
        skin_mesh_rigid(sobj, arm_obj, "Foot.L" if side > 0 else "Foot.R")

    idle_action, walk_action, run_action = build_animations(arm_obj)
    setup_studio_and_render(arm_obj, idle_action, walk_action, run_action)

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
    print(f"[SAGE][Blender] Saved editable rig -> {OUT_BLEND}")
    print(f"[SAGE][Blender] Exported skinned glTF -> {OUT_GLB}")


if __name__ == "__main__":
    main()
