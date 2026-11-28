"""Generates assets/terrain/demo_canyon.png, a 16-bit grayscale heightmap used by
scenes/heightmap_demo.json. Re-run to regenerate:  python tools/generate_demo_heightmap.py

Heightmap convention (see docs/terrain.md): black = terrain.heightmap.minHeight,
white = maxHeight, the image is a top-down map with north (+Z) at the top.
"""
import math
from pathlib import Path

from PIL import Image

SIZE = 257
OUT = Path(__file__).resolve().parent.parent / "assets" / "terrain" / "demo_canyon.png"


def hash2(ix: int, iz: int) -> float:
    h = math.sin(ix * 127.1 + iz * 311.7) * 43758.5453123
    return h - math.floor(h)


def value_noise(x: float, z: float) -> float:
    ix, iz = math.floor(x), math.floor(z)
    fx, fz = x - ix, z - iz
    ux, uz = fx * fx * (3 - 2 * fx), fz * fz * (3 - 2 * fz)
    a, b = hash2(ix, iz), hash2(ix + 1, iz)
    c, d = hash2(ix, iz + 1), hash2(ix + 1, iz + 1)
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz


def fbm(x: float, z: float, octaves: int = 5) -> float:
    total, amp, freq, norm = 0.0, 0.5, 1.0, 0.0
    for _ in range(octaves):
        total += amp * value_noise(x * freq, z * freq)
        norm += amp
        amp *= 0.5
        freq *= 2.03
    return total / norm


def height01(u: float, v: float) -> float:
    # u, v in 0..1. Ridged mountains on both sides of a meandering river valley.
    n = fbm(u * 6.0 + 11.0, v * 6.0 + 4.0)
    ridge = 1.0 - abs(n * 2.0 - 1.0)
    mountains = 0.25 + 0.75 * ridge * ridge
    river_x = 0.5 + 0.16 * math.sin(v * math.pi * 3.0) + 0.05 * math.sin(v * math.pi * 7.0 + 1.3)
    d = abs(u - river_x)
    valley = min(1.0, max(0.0, (d - 0.025) / 0.18))
    valley = valley * valley * (3 - 2 * valley)
    return 0.04 + mountains * valley * 0.96


def main() -> None:
    img = Image.new("I;16", (SIZE, SIZE))
    pixels = img.load()
    for row in range(SIZE):
        v = 1.0 - row / (SIZE - 1)  # row 0 = north (+Z) edge
        for col in range(SIZE):
            u = col / (SIZE - 1)
            pixels[col, row] = int(max(0.0, min(1.0, height01(u, v))) * 65535)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print(f"Wrote {OUT} ({SIZE}x{SIZE}, 16-bit)")


if __name__ == "__main__":
    main()
