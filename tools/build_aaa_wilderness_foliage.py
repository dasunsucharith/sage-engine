"""
SAGE Engine v1.6.0 — AAA Organic Wilderness Foliage & Rock Builder (Blender 5.3)
Replaces old low-poly Stunt City placeholder trees/rocks with organic, realistic, GPU-instancing-friendly
(1-2 merged submeshes each!) survival island environment assets with slope-embedding base skirts so
NOTHING ever floats on rolling 3D hillsides:
  - assets/models/survival/survival_pine.glb    (Tall rugged Douglas-fir / coastal spruce, 2 submeshes)
  - assets/models/survival/survival_palm.glb    (Naturally curved coastal palm w/ 12 arched feathered fronds, 2 submeshes)
  - assets/models/survival/survival_boulder.glb (Sculpted mossy stratified granite boulder w/ deep slope skirt, 1 submesh)
  - assets/models/survival/survival_bush.glb    (Lush leafy forest wild-berry shrub, 2 submeshes)
"""

import bpy
import bmesh
import math
import os
import random
from mathutils import Vector, Euler, Matrix

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SURVIVAL_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models", "survival"))
os.makedirs(SURVIVAL_DIR, exist_ok=True)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def srgb_to_linear(c):
    return tuple((v ** 2.2) for v in c)


def make_mat(name, srgb, roughness=0.85, metallic=0.0):
    mat = bpy.data.materials.new(name=name)
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*srgb_to_linear(srgb), 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


def export_glb(filepath):
    bpy.ops.export_scene.gltf(
        filepath=filepath,
        export_format="GLB",
        export_yup=True,
        export_apply=True,
        export_skins=False,
        export_animations=False,
    )
    sz_kb = os.path.getsize(filepath) / 1024.0
    print(f"[Foliage] Exported {filepath} ({sz_kb:.1f} KB)")


# =============================================================================
# 1. ORGANIC COASTAL DOUGLAS-FIR / PINE TREE (Gnarled Trunk + Drooping Multi-Bough Canopy!)
# =============================================================================
def build_survival_pine():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rng = random.Random(42)

    mat_bark = make_mat("PineBark", (0.24, 0.16, 0.10), roughness=0.92)
    mat_needles = make_mat("PineNeedles", (0.11, 0.24, 0.10), roughness=0.84)

    # --- Trunk (with deep -0.85m root flare skirt so it NEVER floats on hills!) ---
    bm_t = bmesh.new()
    n_seg = 9
    rings = []
    trunk_levels = [
        (-0.85, 0.34, 0.00, 0.00),
        (-0.15, 0.30, 0.01, -0.01),
        ( 0.55, 0.24, 0.02, -0.02),
        ( 2.20, 0.20, 0.05, -0.03),
        ( 4.60, 0.16, 0.06, -0.02),
        ( 7.20, 0.12, 0.03,  0.02),
        ( 9.60, 0.07, -0.01, 0.03),
        (11.40, 0.02, 0.00,  0.00),
    ]
    for z, rad, ox, oy in trunk_levels:
        ring = []
        for i in range(n_seg):
            a = (2.0 * math.pi * i) / n_seg
            flare = (1.0 + 0.22 * math.sin(a * 4.0 + z)) if z < 0.8 else (1.0 + 0.06 * math.sin(a * 3.0))
            ring.append(bm_t.verts.new((ox + math.cos(a) * rad * flare, oy + math.sin(a) * rad * flare, z)))
        rings.append(ring)
    for r in range(len(rings) - 1):
        for i in range(n_seg):
            ni = (i + 1) % n_seg
            bm_t.faces.new((rings[r][i], rings[r][ni], rings[r + 1][ni], rings[r + 1][i]))
    top_v = bm_t.verts.new((0.0, 0.0, 11.75))
    for i in range(n_seg):
        bm_t.faces.new((rings[-1][i], rings[-1][(i + 1) % n_seg], top_v))
    bmesh.ops.recalc_face_normals(bm_t, faces=bm_t.faces)
    m_t = bpy.data.meshes.new("PineTrunk_M")
    bm_t.to_mesh(m_t); bm_t.free()
    o_t = bpy.data.objects.new("PineTrunk", m_t)
    bpy.context.collection.objects.link(o_t)
    o_t.data.materials.append(mat_bark)
    for p in o_t.data.polygons: p.use_smooth = True

    # --- Organic Layered Conifer Boughs (11 Irregular Tiers of Drooping Branch Clusters!) ---
    bm_f = bmesh.new()

    def add_bough_cluster(base, dir_xy, length, width, droop, thick):
        """Creates an organic 3D tapered drooping pine bough frond (diamond cross-section with soft curve)."""
        fwd = Vector((dir_xy.x, dir_xy.y, 0.0)).normalized()
        side = Vector((-fwd.y, fwd.x, 0.0))
        up = Vector((0.0, 0.0, 1.0))

        # 4 cross-sections along bough from trunk out to tip
        specs = [
            (0.00, 0.14 * width, 0.22 * thick,  0.04),
            (0.32, 0.72 * width, 0.52 * thick,  0.02 - droop * 0.25),
            (0.68, 0.92 * width, 0.42 * thick, -droop * 0.65),
            (1.00, 0.16 * width, 0.10 * thick, -droop * 1.05),
        ]
        b_rings = []
        for t, w, th, dz in specs:
            c = base + fwd * (length * t) + up * dz
            v_l = bm_f.verts.new(c - side * w + up * (th * 0.15))
            v_t = bm_f.verts.new(c + up * th)
            v_r = bm_f.verts.new(c + side * w + up * (th * 0.15))
            v_b = bm_f.verts.new(c - up * (th * 0.55))
            b_rings.append((v_l, v_t, v_r, v_b))

        for r in range(len(b_rings) - 1):
            r0, r1 = b_rings[r], b_rings[r + 1]
            for k in range(4):
                nk = (k + 1) % 4
                bm_f.faces.new((r0[k], r0[nk], r1[nk], r1[k]))
        tip = bm_f.verts.new(base + fwd * (length * 1.12) + up * (-droop * 1.18))
        for k in range(4):
            bm_f.faces.new((b_rings[-1][k], b_rings[-1][(k + 1) % 4], tip))

    tiers = [
        (2.4,  9, 2.75, 0.95, 0.65, 0.48),
        (3.3,  8, 2.55, 0.90, 0.60, 0.46),
        (4.2,  8, 2.35, 0.86, 0.55, 0.44),
        (5.1,  8, 2.15, 0.80, 0.48, 0.42),
        (6.0,  7, 1.90, 0.74, 0.42, 0.40),
        (6.9,  7, 1.65, 0.68, 0.36, 0.38),
        (7.8,  6, 1.40, 0.62, 0.30, 0.35),
        (8.7,  6, 1.15, 0.54, 0.24, 0.32),
        (9.5,  5, 0.88, 0.46, 0.18, 0.28),
        (10.3, 5, 0.62, 0.36, 0.12, 0.24),
        (11.0, 4, 0.38, 0.24, 0.06, 0.20),
    ]
    for idx, (tz, count, blen, bw, droop, th) in enumerate(tiers):
        ang_off = idx * 0.68
        for k in range(count):
            a = ang_off + (2.0 * math.pi * k) / count + rng.uniform(-0.22, 0.22)
            l_var = blen * rng.uniform(0.84, 1.16)
            z_var = tz + rng.uniform(-0.18, 0.18)
            add_bough_cluster(
                Vector((0.0, 0.0, z_var)),
                Vector((math.cos(a), math.sin(a), 0.0)),
                l_var, bw * rng.uniform(0.85, 1.15), droop * rng.uniform(0.8, 1.2), th,
            )
    # Crown spire needle tuft
    add_bough_cluster(Vector((0.0, 0.0, 11.3)), Vector((1.0, 0.0, 0.0)), 0.18, 0.20, -0.55, 0.35)

    bmesh.ops.recalc_face_normals(bm_f, faces=bm_f.faces)
    m_f = bpy.data.meshes.new("PineCanopy_M")
    bm_f.to_mesh(m_f); bm_f.free()
    o_f = bpy.data.objects.new("PineCanopy", m_f)
    bpy.context.collection.objects.link(o_f)
    o_f.data.materials.append(mat_needles)
    for p in o_f.data.polygons: p.use_smooth = True

    export_glb(os.path.join(SURVIVAL_DIR, "survival_pine.glb"))


# =============================================================================
# 2. ORGANIC CURVED COASTAL PALM TREE (Smooth Ringed Trunk + 12 Feathered Arched Fronds!)
# =============================================================================
def build_survival_palm():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rng = random.Random(77)

    mat_trunk = make_mat("PalmTrunk", (0.38, 0.29, 0.20), roughness=0.90)
    mat_frond = make_mat("PalmFronds", (0.14, 0.33, 0.10), roughness=0.72)

    # Smooth naturally arching coastal palm trunk with bark growth rings (ZERO stacked boxes!)
    bm_t = bmesh.new()
    n_seg = 10
    n_steps = 16
    rings = []
    top_pos = Vector((0.0, 0.0, 0.0))
    for s in range(n_steps + 1):
        t = s / float(n_steps)
        z = -0.80 + t * 8.60  # Embedded -0.80m skirt -> 7.8m tall!
        curve_x = math.sin(t * math.pi * 0.72) * 0.95
        curve_y = math.sin(t * math.pi * 0.50) * 0.32
        rad = 0.26 * (1.0 - t * 0.56) + (0.016 if (s % 2 == 0 and s > 1) else 0.0)
        if t < 0.14:
            rad *= (1.0 + 0.35 * (1.0 - t / 0.14))
        c = Vector((curve_x, curve_y, z))
        top_pos = c
        ring = []
        for i in range(n_seg):
            a = (2.0 * math.pi * i) / n_seg
            ring.append(bm_t.verts.new((c.x + math.cos(a) * rad, c.y + math.sin(a) * rad, c.z)))
        rings.append(ring)
    for r in range(len(rings) - 1):
        for i in range(n_seg):
            ni = (i + 1) % n_seg
            bm_t.faces.new((rings[r][i], rings[r][ni], rings[r + 1][ni], rings[r + 1][i]))
    cap = bm_t.verts.new(top_pos + Vector((0.0, 0.0, 0.18)))
    for i in range(n_seg):
        bm_t.faces.new((rings[-1][i], rings[-1][(i + 1) % n_seg], cap))
    bmesh.ops.recalc_face_normals(bm_t, faces=bm_t.faces)
    m_t = bpy.data.meshes.new("PalmTrunk_M")
    bm_t.to_mesh(m_t); bm_t.free()
    o_t = bpy.data.objects.new("PalmTrunk", m_t)
    bpy.context.collection.objects.link(o_t)
    o_t.data.materials.append(mat_trunk)
    for p in o_t.data.polygons: p.use_smooth = True

    # 12 Graceful Arching V-Folded Tropical Palm Fronds (2 tiers!)
    bm_f = bmesh.new()

    def add_frond(origin, yaw, pitch_up, length, max_w):
        fwd = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        side = Vector((-fwd.y, fwd.x, 0.0))
        up = Vector((0.0, 0.0, 1.0))

        n_u = 8
        strips = []
        for ui in range(n_u + 1):
            t = ui / float(n_u)
            # Natural parabolic palm frond arch: rises gracefully then droops toward tip!
            dist = t * length
            z_arch = math.sin(t * math.pi * 0.85) * pitch_up - (t ** 2.2) * (length * 0.44)
            w = max_w * math.sin(clamp(t, 0.04, 0.98) * math.pi) * (1.0 - 0.15 * t)
            # Slight V-fold along midrib for realistic 3D light catch!
            v_drop = w * 0.26
            c = origin + fwd * dist + up * z_arch
            vl = bm_f.verts.new(c - side * w - up * v_drop)
            vm = bm_f.verts.new(c)
            vr = bm_f.verts.new(c + side * w - up * v_drop)
            strips.append((vl, vm, vr))

        for ui in range(n_u):
            a0, b0, c0 = strips[ui]
            a1, b1, c1 = strips[ui + 1]
            # Upper & lower faces so fronds look full from below AND above!
            bm_f.faces.new((a0, b0, b1, a1))
            bm_f.faces.new((b0, c0, c1, b1))

    # Lower crown (7 wide arching fronds) + Upper crown (5 steeper fronds)
    for i in range(7):
        a = (2.0 * math.pi * i) / 7.0 + rng.uniform(-0.14, 0.14)
        add_frond(top_pos + Vector((0.0, 0.0, -0.05)), a, 0.85 + rng.uniform(-0.15, 0.15), 3.25 + rng.uniform(-0.25, 0.25), 0.52)
    for i in range(5):
        a = 0.45 + (2.0 * math.pi * i) / 5.0 + rng.uniform(-0.14, 0.14)
        add_frond(top_pos + Vector((0.0, 0.0, 0.08)), a, 1.45 + rng.uniform(-0.12, 0.15), 2.55 + rng.uniform(-0.20, 0.20), 0.44)

    bmesh.ops.recalc_face_normals(bm_f, faces=bm_f.faces)
    m_f = bpy.data.meshes.new("PalmFronds_M")
    bm_f.to_mesh(m_f); bm_f.free()
    o_f = bpy.data.objects.new("PalmFronds", m_f)
    bpy.context.collection.objects.link(o_f)
    o_f.data.materials.append(mat_frond)
    for p in o_f.data.polygons: p.use_smooth = True

    export_glb(os.path.join(SURVIVAL_DIR, "survival_palm.glb"))


# =============================================================================
# 3. SCULPTED MOSSY GRANITE COASTAL BOULDER (Organic Fractured Rock + Deep Skirt!)
# =============================================================================
def build_survival_boulder():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mat_rock = make_mat("CoastalGranite", (0.34, 0.33, 0.31), roughness=0.90)

    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1.0, location=(0.0, 0.0, 0.0))
    obj = bpy.context.active_object
    obj.name = "GraniteBoulder"

    # Sculpt organic faceted coastal granite planes + flattened base + deep slope skirt (-0.55m)!
    for v in obj.data.vertices:
        p = v.co.copy()
        # Broad asymmetric boulder shape
        p.x *= 1.32
        p.y *= 1.08
        p.z *= 0.82
        # Multi-frequency stratified granite displacement (deterministic, organic rock planes!)
        n1 = math.sin(p.x * 2.1 + p.y * 1.7) * math.cos(p.z * 2.4 - p.x * 1.1)
        n2 = math.sin(p.x * 4.8 - p.z * 3.9 + 1.2) * math.cos(p.y * 5.2 + 0.7)
        strata = math.sin(p.z * 6.5 + n1 * 1.8) * 0.055
        disp = 1.0 + n1 * 0.16 + n2 * 0.065 + strata
        p *= disp
        # Shift so rock sits firmly embedded (-0.52m skirt below Z=0, ~1.25m above ground!)
        p.z += 0.36
        if p.z < -0.52:
            p.z = -0.52
        v.co = p

    obj.data.update()
    obj.data.materials.append(mat_rock)
    for p in obj.data.polygons:
        p.use_smooth = False  # Crisp chiseled granite rock facets look awesome under GGX PBR & SSAO!

    export_glb(os.path.join(SURVIVAL_DIR, "survival_boulder.glb"))


# =============================================================================
# 4. LUSH WILD BERRY SHRUB (Replaces Giant Red Sphere Berry Nodes!)
# =============================================================================
def build_survival_bush():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rng = random.Random(123)

    mat_leaf = make_mat("BushLeaves", (0.12, 0.27, 0.09), roughness=0.82)
    mat_berry = make_mat("WildBerries", (0.68, 0.08, 0.14), roughness=0.38)

    bm_l = bmesh.new()
    # 7 overlapping organic sculpted foliage puffs forming a natural low woodland shrub
    centers = [
        ( 0.00,  0.00, 0.44, 0.56),
        ( 0.38,  0.18, 0.34, 0.42),
        (-0.36,  0.22, 0.36, 0.44),
        ( 0.22, -0.36, 0.32, 0.40),
        (-0.28, -0.32, 0.34, 0.42),
        ( 0.05,  0.34, 0.48, 0.38),
        (-0.08, -0.05, 0.64, 0.36),
    ]
    for cx, cy, cz, rad in centers:
        ret = bmesh.ops.create_icosphere(bm_l, subdivisions=2, radius=rad)
        for v in ret["verts"]:
            p = v.co
            n = 1.0 + 0.18 * math.sin(p.x * 8.0 + cx * 5.0) * math.cos(p.y * 8.0 + cz * 4.0)
            v.co = Vector((cx + p.x * n, cy + p.y * n, max(-0.18, cz + p.z * 0.78 * n)))
    bmesh.ops.recalc_face_normals(bm_l, faces=bm_l.faces)
    m_l = bpy.data.meshes.new("BushLeaves_M")
    bm_l.to_mesh(m_l); bm_l.free()
    o_l = bpy.data.objects.new("BushLeaves", m_l)
    bpy.context.collection.objects.link(o_l)
    o_l.data.materials.append(mat_leaf)
    for p in o_l.data.polygons: p.use_smooth = True

    # Clustered ruby-red wild berries nestled across the outer foliage!
    bm_b = bmesh.new()
    for _ in range(28):
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = rng.uniform(0.32, 0.68)
        z = rng.uniform(0.28, 0.76)
        bx, by = math.cos(a) * r, math.sin(a) * r
        ret = bmesh.ops.create_icosphere(bm_b, subdivisions=1, radius=0.048)
        for v in ret["verts"]:
            v.co += Vector((bx, by, z))
    bmesh.ops.recalc_face_normals(bm_b, faces=bm_b.faces)
    m_b = bpy.data.meshes.new("BushBerries_M")
    bm_b.to_mesh(m_b); bm_b.free()
    o_b = bpy.data.objects.new("BushBerries", m_b)
    bpy.context.collection.objects.link(o_b)
    o_b.data.materials.append(mat_berry)
    for p in o_b.data.polygons: p.use_smooth = True

    export_glb(os.path.join(SURVIVAL_DIR, "survival_bush.glb"))


if __name__ == "__main__":
    build_survival_pine()
    build_survival_palm()
    build_survival_boulder()
    build_survival_bush()
