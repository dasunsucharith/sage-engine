"""
Generates spec-compliant glTF 2.0 binary (.glb) starter 3D models for SAGE Engine.
Every generated .glb can be opened, edited, or replaced directly in Blender!
"""

import json
import math
import os
import struct


class GlbBuilder:
    def __init__(self):
        self.materials = []
        self.primitives = []  # list of (mat_idx, positions, normals, uvs, indices)

    def add_material(self, name, rgba, metallic=0.1, roughness=0.55, emissive=None):
        mat = {
            "name": name,
            "pbrMetallicRoughness": {
                "baseColorFactor": [float(c) for c in rgba],
                "metallicFactor": float(metallic),
                "roughnessFactor": float(roughness),
            },
        }
        if emissive is not None:
            mat["emissiveFactor"] = [float(c) for c in emissive]
        self.materials.append(mat)
        return len(self.materials) - 1

    def add_box(self, mat_idx, cx, cy, cz, sx, sy, sz):
        hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
        faces = [
            # (+X) right
            ((1, 0, 0), [(hx, -hy, hz), (hx, -hy, -hz), (hx, hy, -hz), (hx, hy, hz)]),
            # (-X) left
            ((-1, 0, 0), [(-hx, -hy, -hz), (-hx, -hy, hz), (-hx, hy, hz), (-hx, hy, -hz)]),
            # (+Y) top
            ((0, 1, 0), [(-hx, hy, hz), (hx, hy, hz), (hx, hy, -hz), (-hx, hy, -hz)]),
            # (-Y) bottom
            ((0, -1, 0), [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, -hy, hz), (-hx, -hy, hz)]),
            # (+Z) front
            ((0, 0, 1), [(-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]),
            # (-Z) back
            ((0, 0, -1), [(hx, -hy, -hz), (-hx, -hy, -hz), (-hx, hy, -hz), (hx, hy, -hz)]),
        ]
        positions, normals, uvs, indices = [], [], [], []
        for normal, corners in faces:
            base = len(positions)
            for (vx, vy, vz), uv in zip(corners, [(0, 0), (1, 0), (1, 1), (0, 1)]):
                positions.append((cx + vx, cy + vy, cz + vz))
                normals.append(normal)
                uvs.append(uv)
            indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
        self.primitives.append((mat_idx, positions, normals, uvs, indices))

    def add_wedge(self, mat_idx, cx, cy, cz, width, height, length):
        """Ramp rising from -Z (ground y=0) up to +Z (top y=height)."""
        hx, hz = width * 0.5, length * 0.5
        y0, y1 = cy, cy + height

        def add_quad(p0, p1, p2, p3):
            ux, uy, uz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
            vx, vy, vz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
            nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
            nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            n = (nx / nl, ny / nl, nz / nl)
            base = len(positions)
            for p, uv in zip([p0, p1, p2, p3], [(0, 0), (1, 0), (1, 1), (0, 1)]):
                positions.append(p)
                normals.append(n)
                uvs.append(uv)
            indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

        def add_tri(p0, p1, p2):
            ux, uy, uz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
            vx, vy, vz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
            nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
            nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            n = (nx / nl, ny / nl, nz / nl)
            base = len(positions)
            for p, uv in zip([p0, p1, p2], [(0, 0), (1, 0), (0.5, 1)]):
                positions.append(p)
                normals.append(n)
                uvs.append(uv)
            indices.extend([base, base + 1, base + 2])

        positions, normals, uvs, indices = [], [], [], []
        # Bottom corners
        bl_b = (cx - hx, y0, cz - hz)
        br_b = (cx + hx, y0, cz - hz)
        bl_f = (cx - hx, y0, cz + hz)
        br_f = (cx + hx, y0, cz + hz)
        # Top front edge (+Z)
        tl_f = (cx - hx, y1, cz + hz)
        tr_f = (cx + hx, y1, cz + hz)

        # Sloped top face (faces up & toward -Z)
        add_quad(bl_b, br_b, tr_f, tl_f)
        # Front vertical face (+Z)
        add_quad(bl_f, br_f, tr_f, tl_f)
        # Bottom face (-Y)
        add_quad(bl_f, bl_b, br_b, br_f)
        # Left triangle (-X)
        add_tri(bl_b, tl_f, bl_f)
        # Right triangle (+X)
        add_tri(br_b, br_f, tr_f)

        self.primitives.append((mat_idx, positions, normals, uvs, indices))

    def add_cylinder_x(self, mat_idx, cx, cy, cz, radius, width, segments=16):
        """Wheel cylinder aligned along the X axis."""
        hx = width * 0.5
        positions, normals, uvs, indices = [], [], [], []

        # Tread band
        for i in range(segments):
            a0 = (i / segments) * 2.0 * math.pi
            a1 = ((i + 1) / segments) * 2.0 * math.pi
            c0, s0 = math.cos(a0), math.sin(a0)
            c1, s1 = math.cos(a1), math.sin(a1)
            base = len(positions)
            pts = [
                (cx - hx, cy + s0 * radius, cz + c0 * radius, (0, s0, c0)),
                (cx + hx, cy + s0 * radius, cz + c0 * radius, (0, s0, c0)),
                (cx + hx, cy + s1 * radius, cz + c1 * radius, (0, s1, c1)),
                (cx - hx, cy + s1 * radius, cz + c1 * radius, (0, s1, c1)),
            ]
            for px, py, pz, n in pts:
                positions.append((px, py, pz))
                normals.append(n)
                uvs.append((0.5, 0.5))
            indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

        # Side caps (+X and -X)
        for sign in (-1.0, 1.0):
            x = cx + sign * hx
            center_idx = len(positions)
            positions.append((x, cy, cz))
            normals.append((sign, 0.0, 0.0))
            uvs.append((0.5, 0.5))
            ring_start = len(positions)
            for i in range(segments):
                a = (i / segments) * 2.0 * math.pi
                positions.append((x, cy + math.sin(a) * radius, cz + math.cos(a) * radius))
                normals.append((sign, 0.0, 0.0))
                uvs.append((0.5, 0.5))
            for i in range(segments):
                nxt = (i + 1) % segments
                if sign > 0:
                    indices.extend([center_idx, ring_start + nxt, ring_start + i])
                else:
                    indices.extend([center_idx, ring_start + i, ring_start + nxt])

        self.primitives.append((mat_idx, positions, normals, uvs, indices))

    def save_glb(self, path):
        # Merge primitives by material index for clean multi-material meshes
        by_mat = {}
        for mat_idx, pos, norm, uv, idx in self.primitives:
            if mat_idx not in by_mat:
                by_mat[mat_idx] = ([], [], [], [])
            m_pos, m_norm, m_uv, m_idx = by_mat[mat_idx]
            offset = len(m_pos)
            m_pos.extend(pos)
            m_norm.extend(norm)
            m_uv.extend(uv)
            m_idx.extend([i + offset for i in idx])

        bin_data = bytearray()
        buffer_views = []
        accessors = []
        gltf_prims = []

        def append_buffer(raw_bytes, target):
            while len(bin_data) % 4 != 0:
                bin_data.append(0)
            offset = len(bin_data)
            bin_data.extend(raw_bytes)
            bv = {
                "buffer": 0,
                "byteOffset": offset,
                "byteLength": len(raw_bytes),
                "target": target,
            }
            buffer_views.append(bv)
            return len(buffer_views) - 1

        for mat_idx, (pos, norm, uv, idx) in by_mat.items():
            pos_bytes = b"".join(struct.pack("<3f", *p) for p in pos)
            norm_bytes = b"".join(struct.pack("<3f", *n) for n in norm)
            uv_bytes = b"".join(struct.pack("<2f", *t) for t in uv)
            idx_bytes = b"".join(struct.pack("<H", i) for i in idx)

            bv_pos = append_buffer(pos_bytes, 34962)
            bv_norm = append_buffer(norm_bytes, 34962)
            bv_uv = append_buffer(uv_bytes, 34962)
            bv_idx = append_buffer(idx_bytes, 34963)

            min_p = [min(p[k] for p in pos) for k in range(3)]
            max_p = [max(p[k] for p in pos) for k in range(3)]

            acc_pos = len(accessors)
            accessors.append({
                "bufferView": bv_pos,
                "componentType": 5126,
                "count": len(pos),
                "type": "VEC3",
                "min": min_p,
                "max": max_p,
            })
            acc_norm = len(accessors)
            accessors.append({
                "bufferView": bv_norm,
                "componentType": 5126,
                "count": len(norm),
                "type": "VEC3",
            })
            acc_uv = len(accessors)
            accessors.append({
                "bufferView": bv_uv,
                "componentType": 5126,
                "count": len(uv),
                "type": "VEC2",
            })
            acc_idx = len(accessors)
            accessors.append({
                "bufferView": bv_idx,
                "componentType": 5123,
                "count": len(idx),
                "type": "SCALAR",
            })

            gltf_prims.append({
                "attributes": {
                    "POSITION": acc_pos,
                    "NORMAL": acc_norm,
                    "TEXCOORD_0": acc_uv,
                },
                "indices": acc_idx,
                "material": mat_idx,
            })

        while len(bin_data) % 4 != 0:
            bin_data.append(0)

        gltf = {
            "asset": {"version": "2.0", "generator": "SAGE Starter Model Generator"},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": os.path.splitext(os.path.basename(path))[0]}],
            "meshes": [{"name": "Mesh", "primitives": gltf_prims}],
            "materials": self.materials,
            "buffers": [{"byteLength": len(bin_data)}],
            "bufferViews": buffer_views,
            "accessors": accessors,
        }

        json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
        while len(json_bytes) % 4 != 0:
            json_bytes += b" "

        total_length = 12 + 8 + len(json_bytes) + 8 + len(bin_data)
        header = struct.pack("<3I", 0x46546C67, 2, total_length)
        json_chunk = struct.pack("<2I", len(json_bytes), 0x4E4F534A) + json_bytes
        bin_chunk = struct.pack("<2I", len(bin_data), 0x004E4942) + bytes(bin_data)

        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(header)
            f.write(json_chunk)
            f.write(bin_chunk)
        print(f"Generated {path} ({total_length} bytes)")


def build_sports_car(out_path):
    """Sports car facing +Z, resting with tires on y = 0."""
    b = GlbBuilder()
    paint = b.add_material("CarPaint", (0.94, 0.20, 0.16, 1.0), metallic=0.35, roughness=0.28)
    stripe = b.add_material("RacingStripe", (0.96, 0.96, 0.96, 1.0), metallic=0.1, roughness=0.35)
    glass = b.add_material("CabinGlass", (0.10, 0.15, 0.24, 1.0), metallic=0.5, roughness=0.15)
    carbon = b.add_material("CarbonTrim", (0.10, 0.11, 0.13, 1.0), metallic=0.2, roughness=0.55)
    tire = b.add_material("TireRubber", (0.07, 0.07, 0.08, 1.0), metallic=0.0, roughness=0.88)
    rim = b.add_material("AlloyRim", (0.78, 0.80, 0.85, 1.0), metallic=0.85, roughness=0.22)
    headlight = b.add_material("Headlight", (1.0, 0.97, 0.80, 1.0), emissive=(0.95, 0.90, 0.65))
    taillight = b.add_material("Taillight", (1.0, 0.12, 0.10, 1.0), emissive=(0.90, 0.10, 0.08))

    # Lower chassis & sculpted hood
    b.add_box(paint, 0.0, 0.56, 0.0, 2.02, 0.54, 4.35)
    b.add_box(paint, 0.0, 0.80, 1.05, 1.88, 0.16, 1.75)  # Hood bulge
    b.add_box(carbon, 0.0, 0.28, 0.0, 2.08, 0.14, 4.46)  # Side skirts / splitter
    # Racing stripe down center
    b.add_box(stripe, 0.0, 0.86, 0.95, 0.36, 0.07, 2.15)
    b.add_box(stripe, 0.0, 1.37, -0.35, 0.36, 0.04, 1.65)
    # Cabin + roof
    b.add_box(glass, 0.0, 1.08, -0.30, 1.62, 0.52, 1.85)
    b.add_box(paint, 0.0, 1.34, -0.35, 1.56, 0.08, 1.60)
    # GT Rear Spoiler Wing + struts
    b.add_box(carbon, -0.68, 1.00, -1.96, 0.09, 0.42, 0.14)
    b.add_box(carbon,  0.68, 1.00, -1.96, 0.09, 0.42, 0.14)
    b.add_box(carbon,  0.00, 1.24, -2.02, 2.14, 0.09, 0.42)
    # Headlights (+Z front) & Taillights (-Z rear)
    b.add_box(headlight, -0.68, 0.62,  2.18, 0.46, 0.18, 0.08)
    b.add_box(headlight,  0.68, 0.62,  2.18, 0.46, 0.18, 0.08)
    b.add_box(taillight, -0.66, 0.66, -2.18, 0.52, 0.18, 0.08)
    b.add_box(taillight,  0.66, 0.66, -2.18, 0.52, 0.18, 0.08)
    # 4 Wheels (radius 0.38 -> center at y = 0.38 so wheels rest on y = 0)
    for wx in (-1.02, 1.02):
        for wz in (1.35, -1.35):
            b.add_cylinder_x(tire, wx, 0.38, wz, 0.38, 0.30, segments=16)
            b.add_cylinder_x(rim, wx + (0.02 if wx > 0 else -0.02), 0.38, wz, 0.22, 0.31, segments=12)

    b.save_glb(out_path)


def build_stunt_ramp(out_path):
    """3D Stunt jump ramp rising from -Z to +Z, base on y = 0."""
    b = GlbBuilder()
    concrete = b.add_material("RampConcrete", (0.28, 0.31, 0.36, 1.0), metallic=0.1, roughness=0.75)
    neon = b.add_material("NeonCyan", (0.12, 0.92, 1.0, 1.0), emissive=(0.10, 0.80, 0.95))
    hazard = b.add_material("HazardAmber", (1.0, 0.68, 0.10, 1.0), emissive=(0.55, 0.35, 0.05))

    b.add_wedge(concrete, 0.0, 0.0, 0.0, width=5.0, height=2.2, length=7.5)
    # Glowing side guard rails along top edges
    b.add_wedge(neon, -2.35, 0.04, 0.0, width=0.28, height=2.26, length=7.45)
    b.add_wedge(neon,  2.35, 0.04, 0.0, width=0.28, height=2.26, length=7.45)
    # Launch lip highlight
    b.add_box(hazard, 0.0, 2.22, 3.65, 4.95, 0.10, 0.24)
    b.save_glb(out_path)


def build_crate(out_path):
    """Smashable 3D stunt crate centered at origin (1.3m cube)."""
    b = GlbBuilder()
    wood = b.add_material("CrateWood", (0.88, 0.54, 0.18, 1.0), metallic=0.05, roughness=0.75)
    frame = b.add_material("CrateFrame", (0.42, 0.24, 0.09, 1.0), metallic=0.1, roughness=0.80)

    b.add_box(wood, 0.0, 0.0, 0.0, 1.18, 1.18, 1.18)
    # Corner frame bands
    for sy in (-0.54, 0.54):
        b.add_box(frame, 0.0, sy, 0.0, 1.28, 0.16, 1.28)
    for sx in (-0.54, 0.54):
        for sz in (-0.54, 0.54):
            b.add_box(frame, sx, 0.0, sz, 0.18, 1.26, 0.18)
    b.save_glb(out_path)


if __name__ == "__main__":
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "assets", "models"))
    build_sports_car(os.path.join(root, "sports_car.glb"))
    build_stunt_ramp(os.path.join(root, "stunt_ramp.glb"))
    build_crate(os.path.join(root, "crate.glb"))
