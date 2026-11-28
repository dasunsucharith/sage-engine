# 027 — Opaque Pass: Same Picture, Less Work (Draw Order & Early-Z)

## Plain-English Summary

Painting a wall and then hanging a poster over it wastes the paint behind the poster. SAGE was doing that: it painted the ground first, with its expensive, detailed terrain shader, then drew buildings and roads on top, wasting all the work spent on ground that ended up hidden. Now nearby things are drawn first and the ground last, so the graphics card skips the hidden parts before painting them. The picture is identical; the GPU just does less work.

## Technical Deep Dive

### Diagnose first: pixel-bound or geometry-bound?
`--resolution` was added so the same scene can be benchmarked at several sizes. With finer GPU markers (`opaqueEntities`, `opaqueInstanced`, `opaqueTerrain`):

| Opaque pass (Stunt City) | 640×360 | 1280×720 | 1920×1080 |
| :--- | :--- | :--- | :--- |
| total | 0.38 ms | 0.93 ms | 2.03 ms |

The cost scales almost exactly with pixel count, so it's **pixel-shading-bound**. Fewer draw calls wouldn't help; shading fewer pixels would.

### The fix: draw order for early-Z
GPUs can reject a pixel *before* running its shader if something nearer was already drawn there (early depth test, "early-Z"). That only helps if near things come first. The new order:
1. **Solid entities, front-to-back:** a stable sort by squared distance to the camera, reusing scratch vectors (no per-frame allocation).
2. **Ground planes and water, in their original authored order:** layered roads resolve exactly as before.
3. **Instanced props:** their instances inherit the sorted order.
4. **Terrain, last of all:** the most expensive shader and the largest surface. Under roads and buildings it's now rejected before shading.

This can't change the image: the opaque pass uses no blending, and the lit shader has no `discard` or `gl_FragDepth` (either would disable early-Z, which was checked first).

### Results
**Visual:** screenshot diffs against `main` from 5 cameras were 0.05–0.18% of pixels, at or below the 0.19–0.22% that two runs of the *same* old build differ by (animated water and grass). In other words, the image is identical.

**Performance**, interleaved Release A/B, 3 runs each, 1280×720:

| Scene | Frame | GPU |
| :--- | :--- | :--- |
| Stunt City | 2.59 → 2.37 ms (−8.8%) | 1.77 → 1.51 ms (−14.8%) |
| Echo Island | 1.93 → 1.83 ms (−5.3%) | 1.23 → 1.13 ms (−8.2%) |

At 1920×1080 (per-pass, same build): opaque went from about 2.03 to 1.70 ms in Stunt City and from 1.27 to 0.97 ms on Echo Island.

### Tried and reverted
Fading out the terrain's micro-bump detail beyond 90–160 m skipped 12 noise evaluations per distant pixel, but saved only ~0.03–0.09 ms. It visibly smoothed the distant mountain rock and changed seabed shading seen through water, so it was reverted. Art changes need a bigger payoff than that.

## How SAGE Uses This
* Always benchmark a candidate optimization at two resolutions to know what kind of cost you're attacking.
* Keep expensive full-screen surfaces (terrain, large ground) late in the opaque pass.
* **Next candidates:** `opaqueEntities` (~1.1 ms at 1080p in Stunt City, mostly large buildings with many lights); a cheaper lighting path for distant objects; or a depth pre-pass if overdraw among entities stays high.
