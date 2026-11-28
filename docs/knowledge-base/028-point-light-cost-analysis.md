# 028 — Point-Light Cost Analysis & Clustered Light Culling

## Plain-English Summary

We tried to make lighting cheaper for objects far from lamps without changing the picture. Per-object light lists and a shader tweak didn't help and were reverted. **Clustered light culling was built and kept**: it's exact and switched on by default (`--no-light-tiles` turns it off), but it gives no measurable speed-up in today's scenes, because most pixels near the player really are lit by several lamps. It's groundwork for raising the 16-light cap later, when lights can be spread across a whole map.

## How clustered culling works (engine code)
* **Clusters:** the view is split into 32-pixel screen tiles × 16 depth slices, spaced exponentially between the camera's near and far planes (`OpenGLRenderer::buildLightTiles` in `gl_renderer.cpp`).
* **Marking lights:** each frame, each of the up-to-16 lights sets its bit in every cluster its sphere can touch. Screen extent comes from projecting the 8 corners of the sphere's bounding box, expanded by a pixel; if a corner is behind the camera, the light covers every tile. Depth extent is `[z - r, z + r]` plus one slice of margin on each side.
* **Upload:** masks are stored in an `R32F` texture (a 16-bit mask is exact in a float), with the slices stacked vertically, bound on texture unit 8 so no other pass's textures are disturbed.
* **Shader:** the lit shader computes its pixel's tile and depth slice, reads the mask, and skips lights whose bit is clear (and the whole loop when the mask is 0).
* **Exactness:** light falloff is exactly zero outside a light's radius and the marking is conservative, so the image can't change. Verified with screenshot diffs at the noise floor, and pixels changing darker and lighter in equal numbers.

## What we measured
Stunt City, 1920×1080, Release, GPU time of `opaqueEntities` (temporary experiment switches, since removed):

| Experiment | Entities (ms) |
| :--- | :--- |
| Normal | ~1.02–1.19 |
| Point lights off entirely | 0.61 |
| All 16 lights looped, but lighting maths replaced by a stub | ~0.93 |
| Shadow sampling off | ~1.00 (shadows aren't the cost) |
| Cloud shadows off | ~0.98 (not the cost) |

So for objects the point-light work splits into:
* **~0.3 ms: looping over lights that are out of range.** Each pixel checks all 16 uploaded lights even though most are tens of metres away.
* **~0.2 ms: lighting maths for pixels a lamp really lights.** This is needed; cutting it would change the image.

## What we tried (both reverted)
1. **Per-object light lists (exact).** Each draw received only the lights whose radius reaches its bounding sphere: 3.15 lights per draw on average instead of 16, and 0.92 on Echo Island. **No GPU gain** (entities 1.17 → 1.24 ms, noise), because the pixel-heavy objects are big downtown towers standing among the lamps, so their lists stay long even though most of their pixels are far from any single lamp.
2. **Squared-distance early rejection in the shader (exact).** No gain (1.20–1.32 ms both ways): the driver's shader compiler already does it.

## What would work
**Tiled (or clustered) forward light culling:** split the screen into tiles (e.g. 16×16 px), find on the CPU which lights' screen-space bounds touch each tile, upload one bitmask per tile, and have the shader read its tile's mask and skip lights whose bit is clear. That's exact (a conservative tile assignment never drops a light) and removes most of the ~0.3 ms of out-of-range looping, because a tower's pixels then only see the lamps actually near them on screen.

**Expected upside:** at most ~0.3 ms at 1080p (about 7% of the frame), less at 720p. **Cost:** a per-frame tile-mask texture, new shader code (GLSL 3.30 has no `findLSB`, so it loops with bit tests) and careful conservative screen-space bounds.

## Lesson
Pick the granularity of culling to match where the pixels are. Object-level culling only helps when the expensive pixels belong to small objects far from lights. Here the expensive pixels are on large objects among the lights, which needs per-pixel (tile) granularity.

## Follow-up: tiled and clustered culling were built and measured

1. **2D tiled culling** (16×16 px tiles): exact, but each tile still listed **12.8 of 16 lights on average with 0% empty tiles** (Echo Island: 6.4 of 7). The lamps sit near the camera, so their spheres cover most of the screen, and a flat tile can't tell a distant tower from a nearby lamp. No gain.
2. **Clustered culling** (32 px tiles × 16 exponential depth slices, one-slice safety margin, masks in a stacked R32F texture): exact (screenshot diffs at the noise floor, pixels darker and lighter in equal numbers). Clusters averaged 4.5 lights and 77% were non-empty. Object time at 1080p: 1.25 ms (off) vs 1.15–1.23 ms (on), within noise.

**Decisive experiment:** forcing every cluster mask to empty dropped object time to 0.70–0.74 ms. So the skipping mechanism works, but in these scenes most pixels genuinely lie within reach of several lamps. That work is real lighting the image needs, not waste. The earlier "~0.3 ms of skippable looping" estimate was too optimistic, because "GGX stubbed" still left pixels inside light radii paying for distance and attenuation maths.

**Where clustered culling would pay off:** scenes with many lights spread across the map (beyond today's 16-light cap), where most pixels are far from most lights.
