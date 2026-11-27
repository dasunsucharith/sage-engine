"""
SAGE Engine — Blender 5.3 Animated & Interactive 3D Asset Expansion (v0.5)
Builds 4 reusable general-purpose 3D .glb assets (including native glTF 2.0 keyframe animations!):
  1. wind_turbine.glb  — Animated spinning 3-blade city wind turbine
  2. hover_drone.glb   — Animated hovering quad-rotor sci-fi drone with spinning blades
  3. traffic_cone.glb  — High-vis reflective stunt/road cone
  4. stunt_ball.glb    — Rocket-League-style metallic neon-ringed physics stunt ball
"""

import math
import os
import bpy
import bmesh
from mathutils import Vector

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models"))
os.makedirs(OUT_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 60


def make_material(name, rgba, metallic=0.1, roughness=0.5, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name=name)
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
    obj.data.materials.append(mat)


def create_box(name, loc, dims, mat, rot=(0, 0, 0), parent=None):
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
    if parent is not None:
        obj.parent = parent
    return obj


def create_cylinder(name, loc, r1, r2, depth, segs, mat, rot=(0, 0, 0), parent=None, smooth=True):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cone(
        bm,
        cap_ends=True,
        cap_tris=False,
        segments=segs,
        radius1=r1,
        radius2=r2,
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
        for p in obj.data.polygons:
            p.use_smooth = True
    if parent is not None:
        obj.parent = parent
    return obj


def create_uv_sphere(name, loc, radius, mat, segs=24, rings=16, parent=None):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=radius)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    obj.location = Vector(loc)
    assign_mat(obj, mat)
    link_obj(obj)
    for p in obj.data.polygons:
        p.use_smooth = True
    if parent is not None:
        obj.parent = parent
    return obj


def export_glb(filename, animated=False):
    path = os.path.join(OUT_DIR, filename)
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        export_yup=True,
        export_apply=not animated,
        export_materials="EXPORT",
        export_normals=True,
        export_texcoords=True,
        export_animations=animated,
        export_animation_mode="SCENE" if animated else "ACTIONS",
        export_force_sampling=animated,
        export_frame_range=animated,
    )
    print(f"[Blender 5.3] Exported {filename} ({os.path.getsize(path):,} bytes, animated={animated})")


# =============================================================================
# 1. Animated Wind Turbine (wind_turbine.glb) — Spinning 3-blade rotor!
# =============================================================================
def build_wind_turbine():
    clear_scene()
    bpy.context.scene.frame_end = 90

    white_steel = make_material("TurbineWhite", (0.90, 0.92, 0.95), metallic=0.30, roughness=0.32)
    dark_hub = make_material("TurbineHub", (0.14, 0.16, 0.20), metallic=0.65, roughness=0.35)
    beacon_red = make_material("AviationBeacon", (1.0, 0.15, 0.12), emission=(1.0, 0.12, 0.10), emission_strength=2.0)
    cyan_tip = make_material("BladeTipCyan", (0.12, 0.88, 1.0), emission=(0.12, 0.88, 1.0), emission_strength=1.2)

    # Tapered mast (15m tall, base at Z = 0)
    create_cylinder("Mast", (0.0, 0.0, 7.5), 0.65, 0.30, 15.0, 16, white_steel)
    create_box("Nacelle", (0.0, 0.25, 15.2), (0.95, 2.2, 0.95), white_steel)
    create_uv_sphere("BeaconLight", (0.0, 0.9, 15.85), 0.18, beacon_red, segs=12, rings=8)

    # Animated rotor pivot at front (-Y) of nacelle
    pivot = bpy.data.objects.new("RotorPivot", None)
    pivot.location = Vector((0.0, -0.95, 15.2))
    link_obj(pivot)

    create_cylinder("NoseCone", (0.0, -0.35, 0.0), 0.42, 0.08, 0.85, 14, dark_hub, rot=(90, 0, 0), parent=pivot)
    for i in range(3):
        ang = i * 120.0
        rad = math.radians(ang)
        # Blade root + tip extending radially in XZ plane
        cx, cz = math.sin(rad) * 3.1, math.cos(rad) * 3.1
        tx, tz = math.sin(rad) * 5.8, math.cos(rad) * 5.8
        create_box(f"Blade_{i}", (cx, 0.0, cz), (0.42, 0.10, 5.4), white_steel, rot=(0, ang, 0), parent=pivot)
        create_box(f"BladeTip_{i}", (tx, 0.0, tz), (0.34, 0.11, 0.85), cyan_tip, rot=(0, ang, 0), parent=pivot)

    # Keyframe smooth 360-degree rotation around Y axis over 90 frames (3 sec loop)
    for frame, deg in ((1, 0.0), (31, 120.0), (61, 240.0), (91, 360.0)):
        pivot.rotation_euler = (0.0, math.radians(deg), 0.0)
        pivot.keyframe_insert(data_path="rotation_euler", frame=frame)

    export_glb("wind_turbine.glb", animated=True)


# =============================================================================
# 2. Animated Hover Drone (hover_drone.glb) — Bobbing body + 4 spinning rotors!
# =============================================================================
def build_hover_drone():
    clear_scene()
    bpy.context.scene.frame_end = 60

    body_mat = make_material("DroneBody", (0.16, 0.19, 0.25), metallic=0.65, roughness=0.28)
    accent_mat = make_material("DroneAccent", (0.96, 0.42, 0.12), metallic=0.35, roughness=0.32)
    lens_cyan = make_material("DroneEye", (0.10, 0.95, 1.0), emission=(0.10, 0.95, 1.0), emission_strength=2.2)
    blade_mat = make_material("RotorBlade", (0.75, 0.80, 0.86), metallic=0.80, roughness=0.25)

    root = bpy.data.objects.new("DroneRoot", None)
    root.location = Vector((0.0, 0.0, 0.0))
    link_obj(root)

    create_box("Chassis", (0.0, 0.0, 0.0), (1.1, 1.4, 0.42), body_mat, parent=root)
    create_box("TopShell", (0.0, 0.05, 0.26), (0.78, 0.95, 0.16), accent_mat, parent=root)
    create_uv_sphere("ScannerEye", (0.0, -0.72, -0.02), 0.20, lens_cyan, segs=14, rings=10, parent=root)

    # 4 diagonal arms + animated spinning rotors
    arm_coords = [(-0.95, -0.95), (0.95, -0.95), (-0.95, 0.95), (0.95, 0.95)]
    for idx, (ax, ay) in enumerate(arm_coords):
        ang = math.degrees(math.atan2(ay, ax))
        create_box(f"Arm_{idx}", (ax * 0.55, ay * 0.55, 0.06), (1.15, 0.14, 0.10), body_mat, rot=(0, 0, ang), parent=root)
        create_cylinder(f"Motor_{idx}", (ax, ay, 0.14), 0.14, 0.14, 0.20, 12, accent_mat, parent=root)

        rotor = bpy.data.objects.new(f"Rotor_{idx}", None)
        rotor.location = Vector((ax, ay, 0.26))
        rotor.parent = root
        link_obj(rotor)

        create_box(f"PropA_{idx}", (0, 0, 0), (0.86, 0.10, 0.03), blade_mat, parent=rotor)
        create_box(f"PropB_{idx}", (0, 0, 0), (0.10, 0.86, 0.03), blade_mat, parent=rotor)

        spin_dir = 1.0 if (idx % 2 == 0) else -1.0
        for f, deg in ((1, 0.0), (16, 360.0), (31, 720.0), (46, 1080.0), (61, 1440.0)):
            rotor.rotation_euler = (0.0, 0.0, math.radians(deg * spin_dir))
            rotor.keyframe_insert(data_path="rotation_euler", frame=f)

    # Gentle sinusoidal hover bob + slow scan yaw on DroneRoot
    for f, z_off, yaw in ((1, 0.0, 0.0), (16, 0.45, 6.0), (31, 0.0, 0.0), (46, -0.45, -6.0), (61, 0.0, 0.0)):
        root.location = Vector((0.0, 0.0, z_off))
        root.rotation_euler = (0.0, 0.0, math.radians(yaw))
        root.keyframe_insert(data_path="location", frame=f)
        root.keyframe_insert(data_path="rotation_euler", frame=f)

    export_glb("hover_drone.glb", animated=True)


# =============================================================================
# 3. High-Visibility Stunt Traffic Cone (traffic_cone.glb) — Centered at Z = 0
# =============================================================================
def build_traffic_cone():
    clear_scene()

    orange = make_material("ConeOrange", (1.0, 0.34, 0.05), metallic=0.05, roughness=0.42)
    stripe = make_material("ReflectiveWhite", (0.96, 0.97, 0.98), metallic=0.15, roughness=0.25,
                           emission=(0.75, 0.78, 0.82), emission_strength=0.35)
    base_black = make_material("ConeRubberBase", (0.08, 0.08, 0.09), metallic=0.0, roughness=0.85)

    # Centered so Jolt dynamic box/capsule origin matches mesh center!
    create_box("ConeBase", (0.0, 0.0, -0.42), (0.62, 0.62, 0.10), base_black)
    create_cylinder("ConeBody", (0.0, 0.0, 0.02), 0.24, 0.06, 0.82, 16, orange)
    create_cylinder("ConeStripe", (0.0, 0.0, 0.12), 0.185, 0.145, 0.18, 16, stripe)

    export_glb("traffic_cone.glb", animated=False)


# =============================================================================
# 4. Giant Neon Physics Stunt Ball (stunt_ball.glb) — Radius 1.25m, origin centered
# =============================================================================
def build_stunt_ball():
    clear_scene()

    shell = make_material("BallArmor", (0.20, 0.24, 0.30), metallic=0.78, roughness=0.24)
    neon_ring = make_material("BallNeonRing", (0.12, 0.94, 1.0), emission=(0.12, 0.94, 1.0), emission_strength=1.5)
    amber_cap = make_material("BallAmber", (1.0, 0.62, 0.10), emission=(1.0, 0.58, 0.08), emission_strength=1.2)

    create_uv_sphere("BallCore", (0.0, 0.0, 0.0), 1.22, shell, segs=24, rings=16)
    # Orthogonal glowing neon bands so 3D rolling spin is unmistakable!
    create_cylinder("BandZ", (0.0, 0.0, 0.0), 1.25, 1.25, 0.22, 24, neon_ring)
    create_cylinder("BandX", (0.0, 0.0, 0.0), 1.25, 1.25, 0.22, 24, amber_cap, rot=(0, 90, 0))
    create_cylinder("BandY", (0.0, 0.0, 0.0), 1.25, 1.25, 0.22, 24, neon_ring, rot=(90, 0, 0))

    export_glb("stunt_ball.glb", animated=False)


if __name__ == "__main__":
    build_wind_turbine()
    build_hover_drone()
    build_traffic_cone()
    build_stunt_ball()
    print("[Blender 5.3] All 4 v0.5 animated & interactive .glb models built successfully!")
