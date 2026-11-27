"""
SAGE Engine v0.6 — Blender 5.3 Multi-Biome 3D Asset Expansion
Exports 4 new reusable biome .glb models straight into assets/models/:
  1. pine_tree.glb    — Tiered alpine evergreen tree (Pine Highlands biome)
  2. desert_rock.glb  — Stratified red-rock canyon mesa/boulder (Red Rock Desert biome)
  3. lighthouse.glb   — Animated coastal lighthouse with 360-deg rotating light beam (Palm Coast biome)
  4. warehouse.glb    — Industrial delivery depot / garage hangar (Downtown Metro biome)
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
    bpy.context.scene.frame_end = 90


def make_material(name, rgba, metallic=0.1, roughness=0.6, emission=None, emission_strength=0.0):
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
    obj.data.materials.append(mat)
    link_obj(obj)
    if parent is not None:
        obj.parent = parent
    return obj


def create_cone(name, loc, r1, r2, depth, segs, mat, rot=(0, 0, 0), parent=None, smooth=False):
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
    obj.data.materials.append(mat)
    link_obj(obj)
    if smooth:
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
# 1. Alpine Pine Tree (pine_tree.glb) — Base at Z = 0, ~9.5m tall
# =============================================================================
def build_pine_tree():
    clear_scene()
    bark = make_material("PineBark", (0.30, 0.19, 0.11), metallic=0.0, roughness=0.90)
    foliage_dark = make_material("PineDark", (0.08, 0.30, 0.16), metallic=0.0, roughness=0.72)
    foliage_mid = make_material("PineMid", (0.11, 0.38, 0.20), metallic=0.0, roughness=0.68)
    snow_cap = make_material("AlpineTip", (0.82, 0.88, 0.92), metallic=0.05, roughness=0.55)

    create_cone("Trunk", (0, 0, 1.8), 0.42, 0.24, 3.6, 10, bark)
    create_cone("Tier1", (0, 0, 3.5), 2.55, 0.65, 2.8, 9, foliage_dark)
    create_cone("Tier2", (0, 0, 5.4), 2.05, 0.50, 2.5, 9, foliage_mid, rot=(0, 0, 20))
    create_cone("Tier3", (0, 0, 7.1), 1.48, 0.30, 2.2, 9, foliage_dark, rot=(0, 0, 40))
    create_cone("Tier4", (0, 0, 8.5), 0.88, 0.04, 1.7, 9, foliage_mid, rot=(0, 0, 10))
    create_cone("CrownDust", (0, 0, 9.15), 0.40, 0.02, 0.65, 9, snow_cap)

    export_glb("pine_tree.glb", animated=False)


# =============================================================================
# 2. Red Rock Canyon Mesa / Boulder (desert_rock.glb) — Base at Z = 0
# =============================================================================
def build_desert_rock():
    clear_scene()
    terra_base = make_material("RedRockBase", (0.64, 0.28, 0.15), metallic=0.02, roughness=0.88)
    terra_mid = make_material("RedRockMid", (0.76, 0.38, 0.20), metallic=0.02, roughness=0.84)
    ochre_top = make_material("OchreCap", (0.84, 0.50, 0.26), metallic=0.02, roughness=0.80)

    create_cone("MesaBase", (0.0, 0.0, 1.5), 4.6, 3.8, 3.0, 7, terra_base)
    create_cone("MesaMid", (0.3, -0.2, 4.0), 3.7, 2.9, 2.4, 7, terra_mid, rot=(0, 0, 18))
    create_cone("MesaTop", (-0.2, 0.2, 5.9), 2.7, 2.1, 1.6, 7, ochre_top, rot=(0, 0, -14))
    create_cone("SideSpire", (3.2, 1.8, 1.8), 2.1, 1.1, 3.6, 6, terra_mid, rot=(4, -6, 35))

    export_glb("desert_rock.glb", animated=False)


# =============================================================================
# 3. Animated Coastal Lighthouse (lighthouse.glb) — Base at Z = 0, rotating beacon!
# =============================================================================
def build_lighthouse():
    clear_scene()
    bpy.context.scene.frame_end = 90

    stone_base = make_material("LighthouseBase", (0.30, 0.33, 0.38), metallic=0.05, roughness=0.82)
    white_coat = make_material("LighthouseWhite", (0.94, 0.94, 0.92), metallic=0.05, roughness=0.48)
    red_band = make_material("LighthouseRed", (0.86, 0.16, 0.14), metallic=0.08, roughness=0.45)
    dark_iron = make_material("LanternIron", (0.12, 0.14, 0.18), metallic=0.75, roughness=0.35)
    beacon_glow = make_material("BeaconLamp", (1.0, 0.92, 0.55), emission=(1.0, 0.90, 0.45), emission_strength=2.8)
    beam_glow = make_material("SearchlightBeam", (1.0, 0.94, 0.65), emission=(1.0, 0.92, 0.52), emission_strength=2.2)

    create_cone("Foundation", (0, 0, 1.2), 3.2, 2.8, 2.4, 14, stone_base, smooth=True)
    create_cone("Stripe1", (0, 0, 4.2), 2.55, 2.25, 3.6, 16, white_coat, smooth=True)
    create_cone("Stripe2", (0, 0, 7.6), 2.25, 1.96, 3.2, 16, red_band, smooth=True)
    create_cone("Stripe3", (0, 0, 11.0), 1.96, 1.68, 3.6, 16, white_coat, smooth=True)
    create_cone("Stripe4", (0, 0, 14.2), 1.68, 1.48, 2.8, 16, red_band, smooth=True)
    create_cone("GalleryDeck", (0, 0, 15.8), 2.15, 2.15, 0.45, 16, dark_iron, smooth=True)
    create_cone("RoofCap", (0, 0, 18.9), 1.75, 0.10, 1.8, 16, red_band, smooth=True)

    # Rotating lantern & twin golden searchlight cones!
    pivot = bpy.data.objects.new("BeaconPivot", None)
    pivot.location = Vector((0, 0, 17.1))
    link_obj(pivot)

    create_cone("LampCore", (0, 0, 0), 0.95, 0.95, 1.7, 14, beacon_glow, parent=pivot, smooth=True)
    create_cone("BeamFront", (0, -4.5, 0), 0.35, 1.45, 8.2, 12, beam_glow, rot=(-90, 0, 0), parent=pivot, smooth=True)
    create_cone("BeamBack",  (0,  4.5, 0), 0.35, 1.45, 8.2, 12, beam_glow, rot=( 90, 0, 0), parent=pivot, smooth=True)

    for frame, deg in ((1, 0.0), (31, 120.0), (61, 240.0), (91, 360.0)):
        pivot.rotation_euler = (0.0, 0.0, math.radians(deg))
        pivot.keyframe_insert(data_path="rotation_euler", frame=frame)

    export_glb("lighthouse.glb", animated=True)


# =============================================================================
# 4. Industrial Delivery Warehouse / Garage (warehouse.glb) — 14m x 10m, Z = 0
# =============================================================================
def build_warehouse():
    clear_scene()
    steel_wall = make_material("DepotSteel", (0.28, 0.32, 0.38), metallic=0.35, roughness=0.55)
    roof_dark = make_material("DepotRoof", (0.16, 0.18, 0.22), metallic=0.45, roughness=0.50)
    bay_door = make_material("BayDoor", (0.45, 0.48, 0.52), metallic=0.60, roughness=0.40)
    neon_sign = make_material("DepotNeon", (1.0, 0.62, 0.14), emission=(1.0, 0.58, 0.12), emission_strength=1.8)
    window_glow = make_material("OfficeGlow", (0.98, 0.90, 0.65), emission=(0.95, 0.85, 0.55), emission_strength=1.1)

    create_box("MainHall", (0, 0, 3.2), (14.0, 10.0, 6.4), steel_wall)
    create_box("RoofSlab", (0, 0, 6.65), (14.8, 10.8, 0.6), roof_dark)
    create_box("RoofAC_1", (-3.5, 1.5, 7.4), (2.4, 2.0, 1.0), bay_door)
    create_box("RoofAC_2", ( 3.2, -1.5, 7.3), (1.8, 1.8, 0.8), bay_door)
    # 3 loading bay doors facing -Y (SAGE +Z)
    for bx in (-4.2, 0.0, 4.2):
        create_box(f"BayDoor_{bx}", (bx, -5.05, 2.0), (3.1, 0.18, 3.8), bay_door)
    # Glowing upper office windows & neon sign bar
    create_box("OfficeStrip", (0, -5.06, 5.1), (11.6, 0.12, 0.85), window_glow)
    create_box("SignBar", (0, -5.12, 6.1), (12.4, 0.16, 0.32), neon_sign)

    export_glb("warehouse.glb", animated=False)


if __name__ == "__main__":
    build_pine_tree()
    build_desert_rock()
    build_lighthouse()
    build_warehouse()
    print("[Blender 5.3] All 4 v0.6 biome .glb models built successfully!")
