"""
SAGE Engine — Blender 5.3 Procedural 3D Asset Pipeline
Run with:
  "D:\Program Files\Blender Foundation\Blender 5.3\blender.exe" --background --python tools/blender_build_assets.py

Coordinate convention for glTF 2.0 (export_yup=True):
  Blender +Z (Up)    -> SAGE +Y (Up)
  Blender -Y (Front) -> SAGE +Z (Forward)
  Blender +X (Left)  -> SAGE +X (Left)
"""

import math
import os
import bpy
import bmesh
from mathutils import Matrix, Vector

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
os.makedirs(OUT_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_material(name, rgba, metallic=0.1, roughness=0.5, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (rgba[0], rgba[1], rgba[2], 1.0)
    bsdf.inputs["Metallic"].default_value = float(metallic)
    bsdf.inputs["Roughness"].default_value = float(roughness)
    if emission is not None and emission_strength > 0.0:
        bsdf.inputs["Emission Color"].default_value = (emission[0], emission[1], emission[2], 1.0)
        bsdf.inputs["Emission Strength"].default_value = float(emission_strength)
    return mat


def link_obj(obj):
    bpy.context.collection.objects.link(obj)
    return obj


def assign_mat(obj, mat):
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)


def add_bevel(obj, width=0.04, segments=2):
    mod = obj.modifiers.new(name="Bevel", type="BEVEL")
    mod.width = width
    mod.segments = segments
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(30.0)


def create_box(name, loc, dims, mat, rot=(0, 0, 0), bevel=0.0):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(dims), verts=bm.verts)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    obj.location = Vector(loc)
    obj.rotation_euler = tuple(math.radians(a) for a in rot)
    assign_mat(obj, mat)
    link_obj(obj)
    if bevel > 0.0:
        add_bevel(obj, width=bevel, segments=2)
    return obj


def create_cylinder(name, loc, radius1, radius2, depth, segments, mat, rot=(0, 0, 0), smooth=True):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cone(
        bm,
        cap_ends=True,
        cap_tris=False,
        segments=segments,
        radius1=radius1,
        radius2=radius2,
        depth=depth,
    )
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    obj.location = Vector(loc)
    obj.rotation_euler = tuple(math.radians(a) for a in rot)
    assign_mat(obj, mat)
    link_obj(obj)
    if smooth:
        for poly in obj.data.polygons:
            poly.use_smooth = True
    return obj


def create_tapered_prism(name, y_front, y_back, z_bottom, z_top,
                         w_front_bot, w_back_bot, w_front_top, w_back_top,
                         y_top_front_shift, y_top_back_shift, mat, bevel=0.05):
    """Creates a sleek sculpted wedge/canopy along Blender -Y (front) to +Y (back)."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()

    verts = [
        # Bottom ring (0..3)
        bm.verts.new((-w_front_bot * 0.5, y_front, z_bottom)),
        bm.verts.new(( w_front_bot * 0.5, y_front, z_bottom)),
        bm.verts.new(( w_back_bot  * 0.5, y_back,  z_bottom)),
        bm.verts.new((-w_back_bot  * 0.5, y_back,  z_bottom)),
        # Top ring (4..7)
        bm.verts.new((-w_front_top * 0.5, y_front + y_top_front_shift, z_top)),
        bm.verts.new(( w_front_top * 0.5, y_front + y_top_front_shift, z_top)),
        bm.verts.new(( w_back_top  * 0.5, y_back  + y_top_back_shift,  z_top)),
        bm.verts.new((-w_back_top  * 0.5, y_back  + y_top_back_shift,  z_top)),
    ]
    faces = [
        (0, 3, 2, 1),  # bottom
        (4, 5, 6, 7),  # top
        (0, 1, 5, 4),  # front (-Y)
        (2, 3, 7, 6),  # back (+Y)
        (1, 2, 6, 5),  # +X side
        (3, 0, 4, 7),  # -X side
    ]
    for f in faces:
        bm.faces.new([verts[i] for i in f])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    assign_mat(obj, mat)
    link_obj(obj)
    if bevel > 0.0:
        add_bevel(obj, width=bevel, segments=2)
    return obj


def export_glb(filename):
    path = os.path.join(OUT_DIR, filename)
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        export_yup=True,
        export_apply=True,
        export_materials="EXPORT",
        export_normals=True,
        export_texcoords=True,
    )
    size = os.path.getsize(path)
    print(f"[Blender 5.3] Exported {filename} ({size:,} bytes)")


# =============================================================================
# 1. Sculpted Supercar (sports_car.glb) — Nose toward -Y (SAGE +Z), wheels at Z=0
# =============================================================================
def build_sports_car():
    clear_scene()

    paint = make_material("SupercarPaint", (0.92, 0.14, 0.16), metallic=0.45, roughness=0.22)
    stripe = make_material("RacingStripe", (0.96, 0.96, 0.98), metallic=0.15, roughness=0.30)
    glass = make_material("TintedGlass", (0.06, 0.10, 0.18), metallic=0.65, roughness=0.10)
    carbon = make_material("CarbonFiber", (0.08, 0.09, 0.11), metallic=0.25, roughness=0.48)
    chrome = make_material("ChromeExhaust", (0.85, 0.88, 0.92), metallic=0.95, roughness=0.15)
    tire = make_material("TireRubber", (0.05, 0.05, 0.06), metallic=0.0, roughness=0.88)
    rim = make_material("AlloyRim", (0.82, 0.84, 0.88), metallic=0.90, roughness=0.20)
    caliper = make_material("BrakeCaliper", (0.95, 0.18, 0.12), metallic=0.3, roughness=0.35)
    headlight = make_material("LEDHeadlight", (1.0, 0.98, 0.85), emission=(1.0, 0.95, 0.78), emission_strength=1.4)
    taillight = make_material("LEDTaillight", (1.0, 0.10, 0.10), emission=(1.0, 0.08, 0.08), emission_strength=1.5)

    # Carbon front splitter, side skirts & rear diffuser
    create_box("Undertray", (0.0, 0.0, 0.24), (2.10, 4.48, 0.12), carbon, bevel=0.03)
    create_box("FrontSplitter", (0.0, -2.26, 0.20), (2.02, 0.26, 0.06), carbon, bevel=0.02)
    create_box("RearDiffuser", (0.0, 2.22, 0.26), (1.76, 0.24, 0.16), carbon, bevel=0.02)

    # Sculpted wedge main body (low nose at -Y, muscular rear haunches at +Y)
    create_tapered_prism(
        "MainBody",
        y_front=-2.22, y_back=2.18,
        z_bottom=0.28, z_top=0.86,
        w_front_bot=1.94, w_back_bot=2.08,
        w_front_top=1.76, w_back_top=1.96,
        y_top_front_shift=0.35, y_top_back_shift=-0.08,
        mat=paint, bevel=0.07,
    )

    # Muscular front & rear fender flares over wheel arches
    for sx in (-0.92, 0.92):
        create_box(f"FrontFender_{sx}", (sx, -1.32, 0.62), (0.34, 1.15, 0.44), paint, bevel=0.07)
        create_box(f"RearFender_{sx}",  (sx,  1.32, 0.68), (0.40, 1.35, 0.52), paint, bevel=0.08)
        # Side intake scoop
        create_box(f"SideIntake_{sx}", (sx * 1.02, 0.55, 0.56), (0.14, 0.65, 0.28), carbon, bevel=0.02)

    # Aerodynamic fastback cabin canopy
    create_tapered_prism(
        "CabinCanopy",
        y_front=-0.85, y_back=1.45,
        z_bottom=0.82, z_top=1.34,
        w_front_bot=1.56, w_back_bot=1.62,
        w_front_top=1.22, w_back_top=1.18,
        y_top_front_shift=0.68, y_top_back_shift=-0.55,
        mat=glass, bevel=0.06,
    )
    # Painted roof panel & scoop
    create_box("RoofCap", (0.0, 0.28, 1.34), (1.16, 1.05, 0.06), paint, bevel=0.02)
    create_box("RoofScoop", (0.0, 0.45, 1.40), (0.34, 0.52, 0.09), carbon, bevel=0.02)

    # Center twin racing stripes
    for sx in (-0.14, 0.14):
        create_box(f"StripeHood_{sx}", (sx, -1.32, 0.87), (0.11, 1.15, 0.03), stripe)
        create_box(f"StripeRoof_{sx}", (sx,  0.28, 1.375), (0.11, 1.00, 0.02), stripe)

    # Hood vent & front grille
    create_box("HoodVent", (0.0, -1.48, 0.86), (0.64, 0.42, 0.04), carbon, bevel=0.01)
    create_box("FrontGrille", (0.0, -2.20, 0.42), (1.35, 0.08, 0.22), carbon)

    # Side mirrors
    for sx in (-0.95, 0.95):
        create_box(f"Mirror_{sx}", (sx, -0.35, 0.94), (0.24, 0.16, 0.10), paint, bevel=0.02)

    # Sculpted GT Rear Wing Spoiler
    for sx in (-0.64, 0.64):
        create_box(f"WingStrut_{sx}", (sx, 1.88, 1.06), (0.07, 0.24, 0.46), carbon, rot=(-12, 0, 0))
    create_box("RearWingBlade", (0.0, 2.02, 1.30), (2.14, 0.38, 0.07), carbon, rot=(6, 0, 0), bevel=0.02)
    for sx in (-1.07, 1.07):
        create_box(f"WingEndplate_{sx}", (sx, 2.02, 1.28), (0.05, 0.46, 0.26), paint, bevel=0.01)

    # Twin Chrome Exhausts (+Y rear)
    for sx in (-0.36, 0.36):
        create_cylinder(f"Exhaust_{sx}", (sx, 2.24, 0.30), 0.075, 0.075, 0.24, 14, chrome, rot=(90, 0, 0))

    # Aggressive LED Headlights (-Y) & Full-Width Rear Lightbar (+Y)
    for sx in (-0.66, 0.66):
        create_box(f"Headlight_{sx}", (sx, -2.14, 0.58), (0.46, 0.12, 0.12), headlight, rot=(0, 0, -12 if sx > 0 else 12))
    create_box("TailLightBar", (0.0, 2.18, 0.68), (1.72, 0.08, 0.11), taillight)

    # 4 Wheels with 5-spoke rims & red brake calipers (radius 0.39 -> Z = 0.39)
    for wx in (-1.02, 1.02):
        for wy in (-1.35, 1.35):
            side = 1.0 if wx > 0 else -1.0
            create_cylinder(f"Tire_{wx}_{wy}", (wx, wy, 0.39), 0.39, 0.39, 0.30, 22, tire, rot=(0, 90, 0))
            create_cylinder(f"RimHub_{wx}_{wy}", (wx + side * 0.04, wy, 0.39), 0.25, 0.25, 0.26, 18, rim, rot=(0, 90, 0))
            create_box(f"Caliper_{wx}_{wy}", (wx + side * 0.06, wy + 0.11, 0.42), (0.14, 0.14, 0.18), caliper)
            for s in range(5):
                ang = s * (360.0 / 5.0)
                create_box(
                    f"Spoke_{wx}_{wy}_{s}",
                    (wx + side * 0.15, wy, 0.39),
                    (0.04, 0.44, 0.055),
                    rim,
                    rot=(ang, 0, 0),
                )

    export_glb("sports_car.glb")


# =============================================================================
# 2. Industrial Neon Stunt Ramp (stunt_ramp.glb) — Rises from +Y up to -Y
# =============================================================================
def build_stunt_ramp():
    clear_scene()

    concrete = make_material("RampConcrete", (0.26, 0.29, 0.34), metallic=0.1, roughness=0.78)
    steel = make_material("RampSteel", (0.14, 0.16, 0.20), metallic=0.75, roughness=0.35)
    neon = make_material("NeonCyan", (0.10, 0.92, 1.0), emission=(0.12, 0.90, 1.0), emission_strength=1.4)
    amber = make_material("LaunchAmber", (1.0, 0.68, 0.10), emission=(1.0, 0.62, 0.08), emission_strength=1.2)

    # Main sloped deck rising from y = +3.75 (z=0) to y = -3.75 (z=2.2)
    def add_wedge_obj(name, x, w, y_low, y_high, z_low, z_high, mat, bevel=0.0):
        mesh = bpy.data.meshes.new(name)
        bm = bmesh.new()
        hx = w * 0.5
        v = [
            bm.verts.new((x - hx, y_low,  z_low)),
            bm.verts.new((x + hx, y_low,  z_low)),
            bm.verts.new((x + hx, y_high, z_low)),
            bm.verts.new((x - hx, y_high, z_low)),
            bm.verts.new((x - hx, y_high, z_high)),
            bm.verts.new((x + hx, y_high, z_high)),
        ]
        bm.faces.new((v[0], v[1], v[5], v[4]))  # sloped top
        bm.faces.new((v[3], v[2], v[5], v[4]))  # vertical launch wall (-Y)
        bm.faces.new((v[0], v[3], v[2], v[1]))  # bottom
        bm.faces.new((v[0], v[4], v[3]))        # left side
        bm.faces.new((v[1], v[2], v[5]))        # right side
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(name, mesh)
        assign_mat(obj, mat)
        link_obj(obj)
        if bevel > 0.0:
            add_bevel(obj, width=bevel, segments=2)
        return obj

    add_wedge_obj("RampDeck", 0.0, 5.0, 3.75, -3.75, 0.0, 2.2, concrete, bevel=0.04)

    # Raised steel + neon side rails
    for sx in (-2.42, 2.42):
        add_wedge_obj(f"SideRailSteel_{sx}", sx, 0.30, 3.75, -3.75, 0.0, 2.42, steel, bevel=0.02)
        add_wedge_obj(f"SideRailNeon_{sx}",  sx, 0.14, 3.60, -3.65, 0.10, 2.52, neon)

    # Glowing amber launch lip + guide chevrons up the slope
    create_box("LaunchLip", (0.0, -3.62, 2.22), (4.70, 0.32, 0.10), amber, bevel=0.02)
    slope_angle = math.degrees(math.atan2(2.2, 7.5))
    for i, t in enumerate((0.25, 0.52, 0.78)):
        y = 3.75 - t * 7.5
        z = t * 2.2 + 0.03
        create_box(f"RampStripe_{i}", (0.0, y, z), (2.4, 0.40, 0.05), neon, rot=(-slope_angle, 0, 0))

    export_glb("stunt_ramp.glb")


# =============================================================================
# 3. Detailed Shipping Crate (crate.glb) — Centered at origin (1.3m)
# =============================================================================
def build_crate():
    clear_scene()

    wood_light = make_material("WoodPlanks", (0.86, 0.54, 0.20), metallic=0.05, roughness=0.78)
    wood_frame = make_material("WoodFrame", (0.48, 0.28, 0.11), metallic=0.08, roughness=0.82)
    steel_corner = make_material("SteelBracket", (0.20, 0.22, 0.26), metallic=0.80, roughness=0.38)

    # Recessed inner wooden core
    create_box("CrateCore", (0, 0, 0), (1.10, 1.10, 1.10), wood_light, bevel=0.02)

    # 12 outer frame beams
    for s1 in (-0.56, 0.56):
        for s2 in (-0.56, 0.56):
            create_box(f"EdgeX_{s1}_{s2}", (0, s1, s2), (1.28, 0.16, 0.16), wood_frame, bevel=0.015)
            create_box(f"EdgeY_{s1}_{s2}", (s1, 0, s2), (0.16, 1.28, 0.16), wood_frame, bevel=0.015)
            create_box(f"EdgeZ_{s1}_{s2}", (s1, s2, 0), (0.16, 0.16, 1.28), wood_frame, bevel=0.015)

    # Diagonal cross-braces on all 4 vertical sides
    create_box("BraceFront", (0, -0.57, 0), (1.26, 0.08, 0.14), wood_frame, rot=(0, 45, 0))
    create_box("BraceBack",  (0,  0.57, 0), (1.26, 0.08, 0.14), wood_frame, rot=(0, -45, 0))
    create_box("BraceLeft",  (-0.57, 0, 0), (0.08, 1.26, 0.14), wood_frame, rot=(45, 0, 0))
    create_box("BraceRight", ( 0.57, 0, 0), (0.08, 1.26, 0.14), wood_frame, rot=(-45, 0, 0))

    # 8 metallic corner brackets
    for sx in (-0.59, 0.59):
        for sy in (-0.59, 0.59):
            for sz in (-0.59, 0.59):
                create_box(f"Corner_{sx}_{sy}_{sz}", (sx, sy, sz), (0.20, 0.20, 0.20), steel_corner, bevel=0.015)

    export_glb("crate.glb")


# =============================================================================
# 4. Vice-City Style Curved Palm Tree (palm_tree.glb) — Base at Z = 0
# =============================================================================
def build_palm_tree():
    clear_scene()

    trunk_a = make_material("PalmBarkA", (0.42, 0.29, 0.18), metallic=0.0, roughness=0.88)
    trunk_b = make_material("PalmBarkB", (0.34, 0.23, 0.14), metallic=0.0, roughness=0.90)
    frond_mat = make_material("PalmFrond", (0.14, 0.52, 0.20), metallic=0.02, roughness=0.58)
    coconut = make_material("Coconut", (0.28, 0.16, 0.08), metallic=0.05, roughness=0.72)

    # Gracefully curved segmented trunk (7m tall)
    segments = 9
    top_pos = Vector((0.0, 0.0, 0.0))
    for i in range(segments):
        t0 = i / segments
        t1 = (i + 1) / segments
        tm = (t0 + t1) * 0.5
        x_curve = math.sin(tm * 1.4) * 0.85
        z_mid = tm * 7.2
        lean_deg = -math.cos(tm * 1.4) * 14.0
        r_bot = 0.34 * (1.0 - t0 * 0.52)
        r_top = 0.34 * (1.0 - t1 * 0.52)
        mat = trunk_a if (i % 2 == 0) else trunk_b
        create_cylinder(
            f"TrunkSeg_{i}",
            (x_curve, 0.0, z_mid),
            r_bot, r_top, 0.90, 10, mat,
            rot=(0.0, lean_deg, 0.0),
            smooth=False,
        )
        top_pos = Vector((math.sin(t1 * 1.4) * 0.85, 0.0, t1 * 7.2))

    # Coconut cluster
    for c in range(4):
        ang = c * (math.pi * 0.5)
        create_cylinder(
            f"Coconut_{c}",
            (top_pos.x + math.cos(ang) * 0.22, top_pos.y + math.sin(ang) * 0.22, top_pos.z - 0.18),
            0.14, 0.14, 0.26, 8, coconut,
        )

    # 9 arched palm fronds radiating around crown
    num_fronds = 9
    for i in range(num_fronds):
        az = (i / num_fronds) * 2.0 * math.pi
        ca, sa = math.cos(az), math.sin(az)
        # Build each frond out of 3 progressively drooping leaf segments
        for seg, (dist, dz, droop, length, width) in enumerate([
            (0.90,  0.30, -14.0, 1.85, 0.58),
            (2.25,  0.15,  18.0, 1.55, 0.46),
            (3.25, -0.45,  46.0, 1.15, 0.24),
        ]):
            fx = top_pos.x + ca * dist
            fy = top_pos.y + sa * dist
            fz = top_pos.z + dz
            obj = create_box(
                f"Frond_{i}_{seg}",
                (fx, fy, fz),
                (length, width, 0.06),
                frond_mat,
                rot=(0.0, droop, math.degrees(az)),
                bevel=0.015,
            )
            obj.rotation_mode = "ZYX"
            obj.rotation_euler = (0.0, math.radians(droop), az)

    export_glb("palm_tree.glb")


# =============================================================================
# 5. Modern Boulevard Street Lamp (street_lamp.glb) — Base at Z = 0
# =============================================================================
def build_street_lamp():
    clear_scene()

    pole_mat = make_material("LampPole", (0.16, 0.18, 0.22), metallic=0.78, roughness=0.35)
    bulb_mat = make_material("LampGlow", (1.0, 0.90, 0.65), emission=(1.0, 0.86, 0.55), emission_strength=2.0)

    # Base pedestal + tapered vertical pole (6.4m)
    create_cylinder("LampBase", (0.0, 0.0, 0.35), 0.24, 0.18, 0.70, 10, pole_mat)
    create_cylinder("LampPole", (0.0, 0.0, 3.50), 0.11, 0.075, 5.80, 10, pole_mat)
    # Overhang arm extending toward -Y (SAGE +Z)
    create_box("LampArm", (0.0, -0.85, 6.32), (0.11, 1.85, 0.11), pole_mat, rot=(8, 0, 0), bevel=0.02)
    # Fixture housing & glowing LED diffuser
    create_box("LampHead", (0.0, -1.72, 6.42), (0.42, 0.78, 0.12), pole_mat, bevel=0.02)
    create_box("LampBulb", (0.0, -1.72, 6.34), (0.34, 0.66, 0.06), bulb_mat)

    export_glb("street_lamp.glb")


# =============================================================================
# 6. Stylized Glass & Concrete City Tower (city_tower.glb) — 11.5m footprint, Z=0
# =============================================================================
def build_city_tower():
    clear_scene()

    concrete = make_material("TowerConcrete", (0.28, 0.31, 0.38), metallic=0.12, roughness=0.68)
    trim = make_material("TowerTrim", (0.15, 0.17, 0.22), metallic=0.45, roughness=0.42)
    window_lit = make_material("LitWindow", (0.98, 0.86, 0.60), metallic=0.2, roughness=0.15,
                               emission=(0.95, 0.78, 0.45), emission_strength=0.85)
    window_dark = make_material("GlassFacade", (0.14, 0.22, 0.34), metallic=0.72, roughness=0.12)
    neon_crown = make_material("TowerNeon", (0.18, 0.88, 1.0), emission=(0.18, 0.88, 1.0), emission_strength=1.6)

    # Ground lobby podium
    create_box("LobbyPodium", (0, 0, 1.6), (11.6, 11.6, 3.2), trim, bevel=0.12)
    # Main tower shaft (3.2m to 16.5m)
    create_box("TowerShaft", (0, 0, 9.8), (10.6, 10.6, 13.4), concrete, bevel=0.16)
    # Corner structural columns
    for sx in (-5.2, 5.2):
        for sy in (-5.2, 5.2):
            create_box(f"Column_{sx}_{sy}", (sx, sy, 9.8), (1.1, 1.1, 13.8), trim, bevel=0.08)

    # Horizontal glass window bands around all 4 facades (some softly lit!)
    for floor_idx, z in enumerate((4.8, 7.2, 9.6, 12.0, 14.4)):
        mat = window_lit if (floor_idx % 2 == 1) else window_dark
        create_box(f"WinBandX_{floor_idx}", (0, 0, z), (10.75, 8.2, 1.35), mat)
        create_box(f"WinBandY_{floor_idx}", (0, 0, z), (8.2, 10.75, 1.35), mat)

    # Rooftop parapet, penthouse & glowing neon crown
    create_box("RoofDeck", (0, 0, 16.8), (11.0, 11.0, 0.6), trim, bevel=0.08)
    create_box("Penthouse", (0, 0, 18.2), (6.4, 6.4, 2.4), concrete, bevel=0.10)
    create_box("NeonCrown", (0, 0, 19.55), (6.7, 6.7, 0.36), neon_crown)
    create_cylinder("Antenna", (0, 0, 21.8), 0.14, 0.04, 4.2, 8, trim)

    export_glb("city_tower.glb")


if __name__ == "__main__":
    build_sports_car()
    build_stunt_ramp()
    build_crate()
    build_palm_tree()
    build_street_lamp()
    build_city_tower()
    print("[Blender 5.3] All 6 SAGE Engine .glb models built successfully!")
