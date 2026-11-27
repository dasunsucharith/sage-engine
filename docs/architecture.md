# SAGE Engine Architecture

SAGE separates **fast native systems (C++20)** from **AI- and human-friendly game creation (JSON + Lua + glTF)**.

```mermaid
flowchart TB
    subgraph GameLayer["Game & AI Layer (No C++ Needed)"]
        AI["Developer / AI Agent"]
        LUA["resources/*.lua (FiveM-Style Scripts)"]
        JSON["scenes/*.json (Plain-Text Worlds)"]
        GLB["assets/models/*.glb (Blender / AI 3D)"]
    end

    subgraph EngineCore["SAGE C++20 Engine Core"]
        SCRIPT["Lua Scripting VM (sol2 + Hot-Reload)"]
        ECS["World & Scene Manager (ECS)"]
        RENDER["Graphics Renderer (OpenGL -> Vulkan/DX12)"]
        PHYS["Physics & Audio (Jolt + miniaudio)"]
        STREAM["Async World Streaming & Job System"]
        PLAT["Platform Layer (SDL3: Window, Input, OS)"]
    end

    AI --> LUA
    AI --> JSON
    AI --> GLB
    LUA --> SCRIPT
    JSON --> ECS
    GLB --> RENDER
    SCRIPT <--> ECS
    ECS --> RENDER
    ECS --> PHYS
    STREAM --> ECS
    PLAT --> EngineCore
```

## Design Philosophy
1. **Start simple, scale like RAGE:** Begin with a clean sandbox renderer and upgrade toward Deferred PBR, Cascaded Shadow Maps, and open-world chunk streaming.
2. **Hot-reload everything:** Editing `.lua` scripts or `.json` scenes reloads live without rebuilding C++.
3. **Self-documenting for AI:** Every engine capability is documented in `docs/lua_api.md` and `AGENTS.md`.
