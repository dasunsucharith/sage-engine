# 001 — Inside RAGE & GTA VI (and Designing an AI-Native Engine)

**Sources:**
* [Inside the Engine Behind GTA VI — Speclizer](https://youtu.be/GsbiNCwHmqk)
* [GTA V — Graphics Study by Adrian Courrèges](https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/)

---

## Part 1: Every Lesson from RAGE & GTA VI

### 1. RenderWare & The Streaming Problem (`0:00`)
* *GTA III, Vice City,* and *San Andreas* ran on a third-party engine called **RenderWare**. Because the PS2 only had 32 MB of RAM, an entire city could never fit in memory at once.
* **The breakthrough:** Seamless **open-world streaming** — loading only the city blocks near the player from disk while discarding blocks behind them, and replacing distant blocks with low-detail **LODs** (Levels of Detail).
* **For SAGE:** Design asset loading around chunked worlds and LODs as we scale up.

### 2. Why Rockstar Built Its Own Engine (`1:54`)
* When EA acquired Criterion (RenderWare's creator) in 2004, Rockstar refused to depend on a competitor's engine. They evolved Angel Studios' tech into **RAGE (Rockstar Advanced Game Engine)**.
* **For SAGE:** Owning a clean, open-source **C++20** core gives total freedom, zero licensing lock-in, and full control over how AI agents interact with the engine.

### 3. RAGE's First Game: The Table Tennis Rule (`2:49`)
* Rockstar did **not** build *GTA IV* first on RAGE. They shipped **Rockstar Games Presents Table Tennis (2006)** — 1 room, 1 table, 1 ball — to prove lighting, physics, and animation in a tiny sandbox first.
* **For SAGE:** We build a small, rock-solid **Starter Sandbox** (v0.1–v0.4) before tackling massive open worlds.

### 4. How Missions & Gameplay Are Made (`3:24`)
* RAGE separates the **C++ Engine** (fast low-level tech) from **Scripts** (mission state machines, spawning NPCs/cars, triggers, cutscenes).
* **For SAGE:** Embed **Lua (`sol2`)** with **FiveM-style `resources/` folders** and live hot-reloading so developers (and AI) never need to touch C++ to make games.

### 5. Streaming a Whole Open World (`5:41`)
* Every frame (33.3 ms at 30 FPS or 16.6 ms at 60 FPS), background threads predict player movement, stream meshes/textures/collisions from SSD into RAM/VRAM, and unload distant chunks.
* **For SAGE:** Use plain JSON chunk manifests in dev mode (AI-readable) and pack binary archives later for release speed.

### 6. How a Single Frame Gets Drawn (`7:38`)
RAGE uses a **Deferred Rendering Pipeline**:
1. **Cascaded Shadow Maps (CSM):** Render depth from the Sun's perspective in multiple distance rings (sharp shadows nearby, lighter shadows far away).
2. **Geometry Pass → G-Buffer:** Draw opaque 3D objects once into screen-sized textures:
   * **Albedo** (flat base color)
   * **Normals** (3D surface direction encoded in RGB)
   * **Material** (roughness, metallic/specular)
   * **Depth (Reversed-Z)** (near = `1.0`, far = `0.0` to prevent distant Z-fighting flicker)
   * **Stencil Buffer** (masking sky, cars, skin, foliage)
3. **Lighting Pass:** Combine the G-Buffer with sunlight, hundreds of dynamic lights, reflections (real-time cubemaps / SSR / RTGI), and fog — computing lighting **only for visible pixels**.
4. **Transparency & Post-Processing:** Draw glass/water/particles, then apply bloom, HDR tone-mapping, and anti-aliasing.

### 7. Sound Propagation (`9:55`)
* Audio responds to geometry — echoing in tunnels, muffling behind walls, and shifting pitch (Doppler) as vehicles speed past.
* **For SAGE:** Start with **miniaudio** (3D positional audio), then add raycast wall occlusion.

### 8. Euphoria & Physics (`10:39`)
* RAGE pairs rigid-body physics with **Euphoria** procedural active ragdolls (simulated muscles reacting dynamically to hits, slopes, and gravity).
* **For SAGE:** Integrate **Jolt Physics** (multithreaded, AAA-proven, great vehicle support) and later experiment with active ragdoll joint motors.

### 9. GTA VI / RAGE 9 (`12:38`)
* Adds **Ray-Traced Global Illumination (RTGI)**, simulated hair/cloth, dynamic buoyant water, and memory-driven NPC/witness AI.
* **For SAGE:** Hide our GPU renderer behind a clean C++ interface so we start fast (OpenGL / SDL3 GPU) and upgrade smoothly to **Vulkan / DX12**.

### 10. Community & Open Source Power (`13:43`)
* Community engineers and modders (OpenIV, FiveM/Cfx.re, and the famous GTA Online JSON loading-time fix) proved how much faster tech evolves when developers can inspect and extend it.
* **For SAGE:** Stay open-source (MIT), well-documented, and friendly to both human contributors and AI agents.

---

## Part 2: What Makes SAGE "AI-Native"

1. **Plain-Text Scenes (`scenes/*.json`):** AI can spawn, move, and light entire worlds without opaque binary editor files.
2. **FiveM-Style Lua (`resources/<name>/main.lua`):** Easy for AI to generate, easy for humans to read, hot-reloads live.
3. **Drop-In `.glb` / `.gltf` Models (`assets/models/`):** Works straight out of Blender or AI 3D generators.
4. **Built-In AI Manual (`AGENTS.md` + `docs/lua_api.md`):** Any AI assistant immediately knows how SAGE works on the first prompt.
5. **Automated CMake Build:** One command builds the whole engine from source.
