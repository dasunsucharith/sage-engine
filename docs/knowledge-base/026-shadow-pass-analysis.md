# 026 — Shadow Pass Analysis: A Correctness Fix, Not a Speed-Up

## Plain-English Summary

We set out to make shadows faster. Measuring first showed shadows were already cheap (about 0.37 ms of GPU time per frame), so there wasn't much speed to win. Along the way the measurements exposed a real bug: **some shadows were missing**. Long shadows from towers and trees lying along the sun's direction were dropped, leaving patches of ground wrongly sunlit. That's now fixed.

## Technical Deep Dive

### What we measured
* **Per-cascade GPU time** (now permanent markers `shadowNear`, `shadowMid`, `shadowFar`), Stunt City: 0.15 + 0.16 + 0.05 ms. Removing terrain from the shadow maps changed nothing measurable.
* **Draw calls per cascade** (temporary counters): animated models produced 110–189 shadow draws from just 6–12 entities (about 27 sub-meshes each); static models about 70.
* **Culling accuracy** (temporary counters comparing two tests on the same frames): the old test missed up to ~27 casters per frame in the mid cascade, and only ~1–4 of the casters it drew were unnecessary.

### Fix 1: exact light-space culling (correctness)
Each cascade is an orthographic box aligned with the sun. With a low sun, its ground footprint stretches far along the sun direction. The old culling kept casters within an XZ **circle** (`radius + 26 m`) around the camera target, so casters further along the sun axis were skipped and their shadows vanished. Now each caster's rotation-safe bounding sphere (`|boundCenter| + boundRadius`, scaled) is transformed into light space and tested against the box (`|x|, |y| ≤ radius + r`, depth within `[near − r, far + r]`).

**Verified with screenshot diffs** from identical cameras, old versus new build: 16,946 pixels gained shadow versus 55 lost (city mid view), and 8,027 versus 340 (wide view, where the losses are moving vehicles).

### Fix 2: merged depth-only shadow meshes (small efficiency gain)
The shadow shader writes only depth, so materials are irrelevant. At model load, sub-mesh geometry is concatenated into:
* one mesh per **static** model (instanced and single draws), and
* one mesh per **node** for animated/skinned models (node transforms and skins still apply).

Static shadow draws dropped from ~73 to ~15 per cascade, for about 5 MB of extra GPU memory in Stunt City (0.7 MB on Echo Island, logged at startup). Animated models gained little, because most of their nodes hold one sub-mesh each.

### Measured performance impact
Interleaved Release A/B against `main`, 3 runs per scene: Stunt City frame mean 2.60 ms vs 2.77 ms, but the spread overlaps. Echo Island is about 1% slower (within noise), and correctly draws a few more casters. GPU time is unchanged. **Honest summary: no significant speed change; shadows are now correct.**

## How SAGE Uses This
* `--benchmark` and F1 now show shadow cost per cascade.
* The bigger GPU target is the **opaque pass** (~0.95 ms in Stunt City). That's the place to optimize next.

## Next steps
* **Opaque pass:** profile further (overdraw versus vertex cost; terrain and grass in the main pass) before choosing a fix.
* **Animated models:** for nodes that move together, a skinned-style "rigid skinning" approach could merge them into one draw.
