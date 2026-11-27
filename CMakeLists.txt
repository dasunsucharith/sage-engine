cmake_minimum_required(VERSION 3.24)

project(SAGEEngine
    VERSION 0.0.1
    DESCRIPTION "SAGE — Scriptable AI Game Engine"
    LANGUAGES CXX
)

# Use modern C++20 across all platforms
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and AI tools understand all C++ symbols
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core Engine Library
add_library(sage_core STATIC
    engine/src/engine.cpp
)

target_include_directories(sage_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

# Main Executable
add_executable(sage
    engine/src/main.cpp
)

target_link_libraries(sage
    PRIVATE
        sage_core
)
