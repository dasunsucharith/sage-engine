#pragma once

// Engine version & build configuration, injected by CMake from project(VERSION ...)
// so there is exactly ONE place to bump the version (CMakeLists.txt).

#ifndef SAGE_VERSION_STRING
#define SAGE_VERSION_STRING "0.0.0-dev"
#endif

#ifndef SAGE_BUILD_CONFIG
#define SAGE_BUILD_CONFIG ""
#endif

namespace sage {

inline constexpr const char* kEngineVersion = SAGE_VERSION_STRING;

// "Debug", "Release", "RelWithDebInfo", ... or "unspecified" when CMAKE_BUILD_TYPE was empty.
inline constexpr const char* buildConfigName() {
    return SAGE_BUILD_CONFIG[0] != '\0' ? SAGE_BUILD_CONFIG : "unspecified";
}

} // namespace sage
