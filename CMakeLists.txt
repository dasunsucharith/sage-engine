cmake_minimum_required(VERSION 3.24)

project(SAGEEngine
    VERSION 1.5.0
    DESCRIPTION "SAGE — Scriptable AI Game Engine"
    LANGUAGES C CXX
)

# Use modern C++20 across all platforms
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and AI tools understand all C++ symbols
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Dependencies (fetched automatically via FetchContent — no manual DLL hunting!)
# ------------------------------------------------------------------------------
include(FetchContent)

# 1. SDL3 (Platform Window, Events, Input, OpenGL Context)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.20.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3)

# 2. Lua 5.4.9 (Embedded Scripting VM — compiled cleanly as a static C library)
FetchContent_Declare(
    lua
    URL https://github.com/lua/lua/archive/refs/tags/v5.4.9.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_ALL_C_SOURCES "${lua_SOURCE_DIR}/*.c")
list(FILTER LUA_ALL_C_SOURCES EXCLUDE REGEX "(lua|luac|onelua|ltests)\\.c$")

add_library(lua_static STATIC ${LUA_ALL_C_SOURCES})
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")

# 3. sol2 3.5.0 (Modern C++20 <-> Lua Binding Layer, header-only)
FetchContent_Declare(
    sol2
    URL https://github.com/ThePhD/sol2/archive/refs/tags/v3.5.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sol2)

# 4. nlohmann/json 3.12.0 (Plain-text manifest.json & scenes/*.json parser)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# 5. GLM 1.0.3 (3D Vector, Quaternion & Matrix Math, header-only)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glm)

# 6. cgltf 1.15 (Single-header glTF 2.0 / .glb 3D model parser)
FetchContent_Declare(
    cgltf
    URL https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.15.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cgltf)

# 7. stb (stb_image.h for embedded/external .glb textures)
FetchContent_Declare(
    stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(stb)

# 8. Jolt Physics 5.7.0 (Multithreaded AAA 3D Rigid Body Physics Engine)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JoltPhysics
    URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.7.0.tar.gz
    SOURCE_SUBDIR Build
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(JoltPhysics)
if(MSVC)
    target_compile_options(Jolt PRIVATE /Z7)
endif()

# 9. Dear ImGui 1.92.6 (Live In-Game Viewport HUD & Scene Inspector)
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.6.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_static STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_static
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_static
    PUBLIC
        SDL3::SDL3-static
)

# 10. miniaudio 0.11.26 (Cross-Platform 3D & Procedural Audio Engine, single header)
FetchContent_Declare(
    miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.26.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniaudio)

find_package(OpenGL REQUIRED)

# ------------------------------------------------------------------------------
# Core Engine Library
# ------------------------------------------------------------------------------
add_library(sage_core STATIC
    engine/src/engine.cpp
    engine/src/audio/audio_engine.cpp
    engine/src/editor/editor_ui.cpp
    engine/src/physics/physics_world.cpp
    engine/src/platform/input.cpp
    engine/src/platform/window.cpp
    engine/src/renderer/gl_functions.cpp
    engine/src/renderer/gl_renderer.cpp
    engine/src/scripting/lua_engine.cpp
    engine/src/world/scene_loader.cpp
    engine/src/world/world.cpp
)

target_include_directories(sage_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/src
)

target_include_directories(sage_core
    SYSTEM PRIVATE
        ${sol2_SOURCE_DIR}/include
        ${cgltf_SOURCE_DIR}
        ${stb_SOURCE_DIR}
        ${miniaudio_SOURCE_DIR}
)

target_compile_definitions(sage_core
    PRIVATE
        SOL_ALL_SAFETIES_ON=1
        GLM_FORCE_RADIANS
)

target_link_libraries(sage_core
    PRIVATE
        SDL3::SDL3-static
        OpenGL::GL
        lua_static
        nlohmann_json::nlohmann_json
        glm::glm
        Jolt
        imgui_static
)

if(MSVC)
    target_compile_options(sage_core PRIVATE /W4 /utf-8 /bigobj)
else()
    target_compile_options(sage_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------------------------
# Main Executable
# ------------------------------------------------------------------------------
add_executable(sage
    engine/src/main.cpp
)

target_link_libraries(sage
    PRIVATE
        sage_core
)

if(MSVC)
    target_compile_options(sage PRIVATE /W4 /utf-8)
else()
    target_compile_options(sage PRIVATE -Wall -Wextra -Wpedantic)
endif()
