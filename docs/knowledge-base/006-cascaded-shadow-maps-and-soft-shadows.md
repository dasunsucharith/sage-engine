# 006 — Real-Time Cascaded Shadow Maps (CSM) & PCF Soft Shadows

**Milestone Proposal:** `v0.5 — RAGE-Scale Tech` (Daily Engine Research Entry)

---

## 1. Plain-English Summary

Right now in `v0.4`, SAGE Engine lights your **Lamborghini Aventador SVJ**, skyscrapers, palm trees, and tumbling Jolt crates with directional sunlight, hemisphere sky bounce, and horizon fog — **but nothing casts a real shadow onto the ground**. Because your brain uses shadows to judge height and distance, adding real-time shadows is the single biggest visual leap toward an AAA look:

* **Shadow Map:** Before drawing the frame from the player's camera, the GPU quickly renders the scene's depth from the **Sun's point of view** into an invisible texture. Later, when shading every pixel on the road, if that pixel is farther from the sun than what the sun saw, it sits in shadow!
* **Cascaded Shadow Maps (CSM):** One giant shadow map stretched across a whole city looks blocky right next to your car. Just like Rockstar's **RAGE** engine (see [Entry 001](001-rage-and-gtavi.md)), CSM splits the camera view into **3–4 distance rings** (e.g., `0–18m`, `18–55m`, `55–180m`): razor-sharp shadows under your Aventador SVJ and crates nearby, and smooth wide coverage for distant skyscrapers!

---

## 2. Technical Deep Dive

Production-grade OpenGL 3.3+ Cascaded Shadow Maps solve three classic real-time graphics bugs:

### A. Stopping Shadow Shimmer ("Texel-Grid Snapping")
If a cascade's orthographic light box rotates or slides by sub-pixel amounts as the camera orbits, shadow edges visibly crawl and shimmer.
1. Fit a **rotation-invariant bounding sphere** around each camera frustum slice (radius stays constant when you orbit the camera).
2. Transform the sphere center into light view-space and quantize (`floor`) its `(x, y)` coordinates to whole multiples of `worldUnitsPerTexel = (2.0f * cascadeRadius) / shadowResolution`.
3. Build `glm::ortho` symmetrically around that snapped center.

### B. Stopping Shadow Acne & Peter-Panning (Dual Bias)
* **Hardware Slope-Scaled Bias (`glPolygonOffset`):** Applied during the depth pass (`glEnable(GL_POLYGON_OFFSET_FILL)`) so angled surfaces don't self-shadow.
* **Shader Normal-Offset Bias (Frostbite Technique):** Shift the world-space lookup point slightly along the surface normal (`worldPos + N * normalBias * texelSize`) before projecting into light space. This eliminates striped "acne" on flat asphalt without detaching tires from their shadows ("peter-panning").

### C. Soft Edges & Seamless Cascades
* **Hardware PCF (`sampler2DArrayShadow`):** Configure the `GL_TEXTURE_2D_ARRAY` (`GL_DEPTH_COMPONENT32F`, e.g. `2048×2048 × 3` layers) with `GL_TEXTURE_COMPARE_MODE = GL_COMPARE_REF_TO_TEXTURE`, `GL_TEXTURE_COMPARE_FUNC = GL_LEQUAL`, and `GL_LINEAR` filtering. Each texture tap gives a free 2×2 hardware bilinear PCF filter; a compact 3×3 or Poisson-disk loop yields silky soft shadows at 60+ FPS on an RTX 5050.
* **Cascade Blend Band:** Within the outer 10% of each cascade split, sample both cascade `i` and `i + 1` and `mix()` linearly so transitions across distance rings are invisible.

---

## 3. How SAGE Uses This (Concrete `v0.5` Implementation Proposal)

When you're ready to build this upgrade, it touches only **3 files** cleanly behind SAGE's existing `Renderer` interface:

1. **[`engine/src/renderer/gl_functions.h`](../../engine/src/renderer/gl_functions.h) & [`gl_functions.cpp`](../../engine/src/renderer/gl_functions.cpp):**
   * Load the remaining OpenGL 3.3 Core framebuffer & 3D texture array functions: `glGenFramebuffers`, `glBindFramebuffer`, `glFramebufferTextureLayer`, `glCheckFramebufferStatus`, `glDeleteFramebuffers`, `glTexImage3D`, `glUniform1fv`, `glUniformMatrix4fv` (already loaded).
2. **[`engine/src/renderer/gl_renderer.cpp`](../../engine/src/renderer/gl_renderer.cpp):**
   * Add a lightweight **`ShadowDepthPass`** shader (vertex-only transform + empty fragment shader) writing to a `2048×2048 × 3` `GL_TEXTURE_2D_ARRAY` depth FBO.
   * Compute 3 Practical-Split cascades (`18m`, `55m`, `180m`) with bounding-sphere fitting + texel-grid snapping using `world.sky().sunDirection`.
   * Bind the shadow texture array on texture unit `1` during `Lit3DPass`, apply normal-offset bias + 3×3 hardware PCF, and multiply only `directDiffuse + specular` by `(1.0 - shadow)` while keeping `hemiAmbient` untouched so shadows have realistic cool blue sky fill!
3. **[`engine/src/editor/editor_ui.cpp`](../../engine/src/editor/editor_ui.cpp):**
   * Add a **Shadows** toggle, **Shadow Softness** slider, and **Visualize Cascades (Debug Tint)** checkbox inside the Dear ImGui `Sky & Lighting` panel so you can inspect the 3 distance rings live!

> **Bonus Future Pairing:** Our parallel research also logged the exact blueprint for **Jolt Physics `VehicleConstraint` + `WheeledVehicleController`** (true 4-wheel raycast/cylinder suspension, anti-roll bars, and lowered center-of-mass) for our next physics evolution entry!
