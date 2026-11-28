// Deterministic simulation (knowledge-base entries 020 and 029): the same script on the
// same fixed 60 Hz clock must produce bit-identical world state every run. This is what
// lets us prove refactors change nothing.
#include "test_helpers.h"

#include "sage/audio/audio_engine.h"
#include "sage/physics/physics_world.h"
#include "sage/platform/input.h"
#include "sage/scripting/lua_engine.h"
#include "sage/world/world.h"

#include <string>
#include <vector>

using namespace sage;
using sage_test::TempDir;

namespace {

constexpr const char* kScenarioScript = R"LUA(
local tick = 0
function onStart()
  world.spawnBox("hero", 5, 0.5, 5)
  world.setLookAt("hero", 10, 2, 10, 0.8)
  world.spawnBox("a", 0, 1, 0); world.spawnBox("b", 1, 1, 0); world.spawnBox("c", 2, 1, 0)
  world.destroy("a")
  world.spawnBox("crate", 20, 12, 20)
  physics.addDynamicBox("crate", 20, 12, 20, 0.5, 0.5, 0.5, 10.0, 0.3, 0.5)
end
function onTick(dt)
  tick = tick + 1
  if tick == 3 then world.triggerRagdoll("hero", 12, 6, -4, 2.0) end
  if tick == 20 then world.clearLookAt("hero") end
  if tick == 25 then world.destroy("b"); world.spawnBox("b", 9, 9, 9) end
  local x, y, z = world.getPosition("c")
  world.setPosition("c", x + 0.01, y, z)
end
)LUA";

struct Snapshot {
    std::string id;
    float x, y, z;
};

// Runs the scenario headless for `ticks` fixed steps, mirroring Engine::fixedTick.
std::vector<Snapshot> runScenario(int ticks) {
    TempDir dir;
    dir.write("resources/scenario/manifest.json", R"({ "name": "scenario", "scripts": ["main.lua"] })");
    dir.write("resources/scenario/main.lua", kScenarioScript);

    World world;
    PhysicsWorld physics;
    AudioEngine audio;
    InputState input;
    LuaEngine lua;
    REQUIRE(physics.initialize());
    audio.initialize(true);
    double simTime = 0.0;
    LuaHostCallbacks cb;
    cb.getTimeSeconds = [&simTime]() { return simTime; };
    REQUIRE(lua.initialize(dir.path() / "resources", world, physics, audio, input, std::move(cb)));

    constexpr double dt = 1.0 / 60.0;
    for (int i = 0; i < ticks; ++i) {
        world.captureTickSnapshot();
        lua.tick(dt);
        physics.step(static_cast<float>(dt), world, static_cast<float>(simTime));
        world.updateAnimations(static_cast<float>(dt));
        simTime += dt;
    }

    std::vector<Snapshot> out;
    for (const Entity& e : world.entities()) {
        out.push_back({e.id, e.position.x, e.position.y, e.position.z});
    }
    lua.shutdown();
    audio.shutdown();
    physics.shutdown();
    return out;
}

} // namespace

TEST_CASE("the same scenario produces bit-identical results every run") {
    const auto first = runScenario(120);
    const auto second = runScenario(120);
    REQUIRE(first.size() == second.size());
    REQUIRE(first.size() == 4); // hero, b (respawned), c, crate ("a" was destroyed)
    for (std::size_t i = 0; i < first.size(); ++i) {
        CAPTURE(first[i].id);
        CHECK(first[i].id == second[i].id);
        CHECK(first[i].x == second[i].x); // Exact equality on purpose
        CHECK(first[i].y == second[i].y);
        CHECK(first[i].z == second[i].z);
    }

    // Sanity: things actually moved (physics fell, the ragdoll flew, the script moved "c").
    for (const Snapshot& s : first) {
        if (s.id == "crate") CHECK(s.y < 12.0f);
        if (s.id == "hero") CHECK((s.x != 5.0f || s.z != 5.0f));
        if (s.id == "c") CHECK(s.x > 2.5f);
    }
}
