#pragma once

#include <filesystem>
#include <string>

namespace sage {

class PhysicsWorld;
class World;

// Loads human- and AI-readable plain-text JSON scenes (scenes/*.json) into World
// and automatically registers any entity "collider" definitions in PhysicsWorld.
class SceneLoader {
public:
    static bool loadFromFile(const std::filesystem::path& scenePath, World& world, PhysicsWorld* physics = nullptr);
};

} // namespace sage
