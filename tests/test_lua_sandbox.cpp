// Lua sandbox, runaway-script watchdog and per-resource saves
// (knowledge-base entries 020 and 023).
#include "test_helpers.h"

#include "sage/audio/audio_engine.h"
#include "sage/physics/physics_world.h"
#include "sage/platform/input.h"
#include "sage/scripting/lua_engine.h"
#include "sage/world/world.h"

#include <chrono>
#include <fstream>
#include <sstream>

using namespace sage;
using sage_test::TempDir;

namespace {

// A headless Lua runtime over a temporary project folder (resources/ + saves/).
struct LuaHarness {
    TempDir dir;
    World world;
    PhysicsWorld physics;
    AudioEngine audio;
    InputState input;
    LuaEngine lua;
    double simTime = 0.0;

    void addResource(const std::string& name, const std::string& script) {
        dir.write("resources/" + name + "/manifest.json",
                  "{ \"name\": \"" + name + "\", \"version\": \"1.0.0\", \"scripts\": [\"main.lua\"] }");
        dir.write("resources/" + name + "/main.lua", script);
    }

    bool start() {
        REQUIRE(physics.initialize());
        audio.initialize(/*silentMode=*/true);
        LuaHostCallbacks cb;
        cb.getTimeSeconds = [this]() { return simTime; };
        cb.getRealTimeSeconds = [this]() { return simTime; };
        return lua.initialize(dir.path() / "resources", world, physics, audio, input, std::move(cb));
    }

    std::string readSave(const std::string& resource, const std::string& key) const {
        std::ifstream in(dir.path() / "saves" / resource / (key + ".dat"), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    ~LuaHarness() {
        lua.shutdown();
        audio.shutdown();
        physics.shutdown();
    }
};

} // namespace

TEST_CASE("Lua sandbox blocks file access and unverified bytecode") {
    LuaHarness h;
    h.addResource("probe", R"LUA(
        local r = {}
        r[#r + 1] = tostring(dofile == nil)
        r[#r + 1] = tostring(loadfile == nil)
        local f = load("return 1 + 1")
        r[#r + 1] = tostring(f ~= nil and f() == 2)                 -- text chunks still work
        r[#r + 1] = tostring(load(string.dump(function() end)) == nil) -- binary chunks rejected
        r[#r + 1] = tostring(sage.fileExists("/etc/hosts") or sage.fileExists("C:/Windows/win.ini"))
        r[#r + 1] = tostring(sage.fileExists("../outside.txt"))
        r[#r + 1] = tostring(io == nil and os == nil and require == nil)
        sage.saveData("result", table.concat(r, ","))
    )LUA");
    REQUIRE(h.start());
    CHECK(h.readSave("probe", "result") == "true,true,true,true,false,false,true");
}

TEST_CASE("an infinite loop in a script is stopped and only pauses that resource") {
    LuaHarness h;
    h.addResource("looper", R"LUA(
        function onTick(dt) while true do end end
    )LUA");
    REQUIRE(h.start());

    const auto t0 = std::chrono::steady_clock::now();
    h.lua.tick(1.0 / 60.0); // Watchdog must abort this after its 1 s budget
    const double firstTick = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    CHECK(firstTick < 5.0);

    const auto t1 = std::chrono::steady_clock::now();
    h.lua.tick(1.0 / 60.0); // The resource is paused now: returns immediately
    const double secondTick = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
    CHECK(secondTick < 0.5);
}

TEST_CASE("sage.getTime returns the host's simulation clock") {
    LuaHarness h;
    h.addResource("clock", R"LUA(
        function onTick(dt) sage.saveData("t", string.format("%.3f", sage.getTime())) end
    )LUA");
    REQUIRE(h.start());
    h.simTime = 12.5;
    h.lua.tick(1.0 / 60.0);
    CHECK(h.readSave("clock", "t") == "12.500");
}

TEST_CASE("each resource saves into its own folder; old saves are migrated once") {
    LuaHarness h;
    h.dir.write("saves/legacy.dat", "old score 42"); // Pre-1.7 shared layout
    h.addResource("alpha", R"LUA(
        sage.saveData("shared", "from alpha")
        sage.saveData("migrated", sage.loadData("legacy"))
        sage.saveData("temp", "x")
        sage.saveData("deleted", tostring(sage.deleteData("temp")) .. "/" .. sage.loadData("temp"))
    )LUA");
    h.addResource("beta", R"LUA(
        sage.saveData("shared", "from beta")
        sage.saveData("legacySeen", "[" .. sage.loadData("legacy") .. "]")
    )LUA");
    REQUIRE(h.start());

    CHECK(h.readSave("alpha", "shared") == "from alpha");
    CHECK(h.readSave("beta", "shared") == "from beta");
    CHECK(h.readSave("alpha", "migrated") == "old score 42");
    CHECK(h.readSave("beta", "legacySeen") == "[]"); // Already claimed by alpha
    CHECK(h.readSave("alpha", "deleted") == "true/");
    CHECK(std::filesystem::exists(h.dir.path() / "saves" / "legacy.dat.migrated"));
}
