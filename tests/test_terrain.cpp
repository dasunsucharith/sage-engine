// Data-driven terrain from scene JSON (knowledge-base entry 022, docs/terrain.md).
#include "test_helpers.h"

#include "sage/world/scene_loader.h"
#include "sage/world/world.h"

using namespace sage;
using sage_test::near;
using sage_test::TempDir;

namespace {

// No noise, so every height below can be worked out by hand.
const char* kTerrainScene = R"JSON({
  "name": "terrain test",
  "terrain": {
    "type": "procedural",
    "halfSize": 300,
    "baseHeight": 1.0,
    "features": [
      { "type": "peak", "center": [0, 0], "radius": 50, "height": 20, "sharpness": 2 },
      { "type": "flatten", "center": [100, 0], "innerRadius": 5, "radius": 10, "height": 7 },
      { "type": "road", "width": 4, "blend": 2, "points": [[-100, -50, 2], [-100, 50, 6]] }
    ],
    "layers": [
      { "name": "high", "color": [0.2, 0.2, 0.2], "startHeight": 10 },
      { "name": "low", "color": [0.1, 0.3, 0.1] }
    ]
  },
  "entities": []
})JSON";

} // namespace

TEST_CASE("procedural terrain features produce the documented heights") {
    TempDir dir;
    World world;
    REQUIRE(SceneLoader::loadFromFile(dir.write("scenes/t.json", kTerrainScene), world));
    REQUIRE(world.terrain().mode == TerrainSettings::kModeDataDriven);

    CHECK(near(world.sampleTerrain(0, 0).height, 21.0f));    // base 1 + peak 20 at its centre
    CHECK(near(world.sampleTerrain(60, 60).height, 1.0f));   // outside every feature
    CHECK(near(world.sampleTerrain(100, 0).height, 7.0f));   // plateau levels to exactly 7
    CHECK(near(world.sampleTerrain(-100, 0).height, 4.0f));  // road midpoint between heights 2 and 6

    // Layers are sorted by startHeight; biomeId is the highest layer reached.
    CHECK(world.terrain().spec.layers.front().name == "low");
    CHECK(world.sampleTerrain(0, 0).biomeId == 1);
    CHECK(world.sampleTerrain(60, 60).biomeId == 0);
}

TEST_CASE("an invalid terrain block falls back to flat terrain instead of failing") {
    TempDir dir;
    World world;
    const auto scene = dir.write("scenes/bad.json", R"JSON({
      "terrain": { "type": "procedural", "noise": { "amplitude": "big" },
                   "features": [ { "type": "volcano", "center": [0, 0], "radius": 10 } ] },
      "entities": []
    })JSON");
    REQUIRE(SceneLoader::loadFromFile(scene, world)); // The scene itself still loads
    CHECK_FALSE(world.terrain().enabled);
    CHECK(near(world.sampleTerrain(0, 0).height, 0.0f));
}

TEST_CASE("legacy terrain mode names still select the built-in presets") {
    TempDir dir;
    World world;
    REQUIRE(SceneLoader::loadFromFile(
        dir.write("scenes/island.json", R"JSON({ "terrain": { "mode": "island" }, "entities": [] })JSON"), world));
    CHECK(world.terrain().mode == TerrainSettings::kModeEchoIslandPreset);

    REQUIRE(SceneLoader::loadFromFile(dir.write("scenes/none.json", R"JSON({ "entities": [] })JSON"), world));
    CHECK(world.terrain().mode == TerrainSettings::kModeStuntCityPreset); // No block = defaults
}

TEST_CASE("scene export writes the terrain block back so it survives a round trip") {
    TempDir dir;
    World world;
    REQUIRE(SceneLoader::loadFromFile(dir.write("scenes/t.json", kTerrainScene), world));
    const auto exported = dir.path() / "scenes" / "exported.json";
    REQUIRE(SceneLoader::saveToFile(exported, world, "export test"));

    World reloaded;
    REQUIRE(SceneLoader::loadFromFile(exported, reloaded));
    CHECK(reloaded.terrain().mode == TerrainSettings::kModeDataDriven);
    CHECK(near(reloaded.sampleTerrain(0, 0).height, 21.0f));
    CHECK(near(reloaded.sampleTerrain(-100, 0).height, 4.0f));
}
