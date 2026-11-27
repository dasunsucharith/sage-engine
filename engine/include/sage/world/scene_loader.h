#pragma once

#include <filesystem>
#include <string>

namespace sage {

class World;

// Loads human- and AI-readable plain-text JSON scenes (scenes/*.json) into a World.
class SceneLoader {
public:
    static bool loadFromFile(const std::filesystem::path& scenePath, World& world);
};

} // namespace sage
