"""
Exports D:\Blender\Models\lamborghini_aventador_svj.blend cleanly to
assets/models/aventador_svj.glb for SAGE Engine (without modifying the .blend).

- Strips the 08_Studio_Cameras collection (floor, lights, cameras)
- Converts Blender procedural nodes (Verde Mantis paint, Carbon Fiber, Grille, Clear Glass)
  into clean glTF 2.0 PBR Principled BSDF parameters so colors & shine match in real-time GL
- Preserves exact real-world meters, wheel ground contact (Z=0), and -Y front -> +Z glTF forward
"""

import os
import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "assets", "models", "aventador_svj.glb"))


def set_principled(mat, base_color, metallic=None, roughness=None, emission=None, emission_strength=None):
    if not mat or not mat.node_tree:
        return
    nt = mat.node_tree
    bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
    out = next((n for n in nt.nodes if n.type == "OUTPUT_MATERIAL"), None)
    if bsdf is None:
        nt.nodes.clear()
        bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    # Disconnect any procedural nodes feeding Base Color / Roughness / Metallic so glTF reads exact values
    for sock_name in ("Base Color", "Roughness", "Metallic"):
        sock = bsdf.inputs.get(sock_name)
        if sock and sock.is_linked:
            for link in list(sock.links):
                nt.links.remove(link)

    bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
    if metallic is not None:
        bsdf.inputs["Metallic"].default_value = float(metallic)
    if roughness is not None:
        bsdf.inputs["Roughness"].default_value = float(roughness)
    if emission is not None and emission_strength is not None:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = float(emission_strength)


def main():
    # 1. Remove studio floor, cameras, and studio lights
    studio_col = bpy.data.collections.get("08_Studio_Cameras")
    if studio_col:
        for obj in list(studio_col.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(studio_col)

    for obj in list(bpy.data.objects):
        if obj.type in ("CAMERA", "LIGHT") or "Studio" in obj.name:
            bpy.data.objects.remove(obj, do_unlink=True)

    # 2. Convert procedural materials into real-time glTF 2.0 PBR materials
    # Signature Lamborghini Verde Mantis vibrant metallic lime-green!
    set_principled(bpy.data.materials.get("Mat_Verde_Mantis"), (0.14, 0.78, 0.03), metallic=0.52, roughness=0.18)
    set_principled(bpy.data.materials.get("Mat_Carbon_Fiber"), (0.045, 0.05, 0.055), metallic=0.28, roughness=0.36)
    set_principled(bpy.data.materials.get("Mat_Mesh_Grille"), (0.025, 0.028, 0.032), metallic=0.40, roughness=0.55)
    set_principled(bpy.data.materials.get("Mat_Glass_Clear"), (0.05, 0.09, 0.14), metallic=0.65, roughness=0.08)
    set_principled(bpy.data.materials.get("Mat_Glass_Tinted"), (0.04, 0.06, 0.11), metallic=0.60, roughness=0.10)
    set_principled(bpy.data.materials.get("Mat_LED_White"), (1.0, 0.98, 0.90), metallic=0.0, roughness=0.15,
                   emission=(1.0, 0.96, 0.84), emission_strength=1.6)
    set_principled(bpy.data.materials.get("Mat_LED_Red"), (1.0, 0.08, 0.08), metallic=0.0, roughness=0.15,
                   emission=(1.0, 0.06, 0.06), emission_strength=1.8)

    # Ensure decals with missing external files fall back cleanly instead of exporting broken pink textures
    for mat in bpy.data.materials:
        if not mat.node_tree:
            continue
        for node in list(mat.node_tree.nodes):
            if node.type == "TEX_IMAGE":
                img = node.image
                if not img or (not img.packed_file and not os.path.exists(bpy.path.abspath(img.filepath))):
                    print(f"[Export] Missing image on {mat.name}, converting to clean accent color.")
                    color = (0.85, 0.72, 0.20) if "Crest" in mat.name else (0.92, 0.92, 0.94)
                    set_principled(mat, color, metallic=0.75, roughness=0.25)
                    break

    # 3. Export cleanly to GLB (+Y up, -Y Blender front -> +Z SAGE forward)
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=OUT_PATH,
        export_format="GLB",
        export_yup=True,
        export_apply=True,
        export_materials="EXPORT",
        export_normals=True,
        export_texcoords=True,
    )
    print(f"[Blender 5.3] Exported Aventador SVJ -> {OUT_PATH} ({os.path.getsize(OUT_PATH):,} bytes)")


if __name__ == "__main__":
    main()
