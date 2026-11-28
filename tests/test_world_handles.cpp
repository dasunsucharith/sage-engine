// Entity handles & components (knowledge-base entry 029).
#include "test_helpers.h"

#include "sage/world/component_store.h"
#include "sage/world/world.h"

using namespace sage;

TEST_CASE("entity handles: lookup, stale detection and slot reuse") {
    World w;
    w.spawnEntity("a", EntityShape::Box, {1, 0, 0});
    w.spawnEntity("b", EntityShape::Box, {2, 0, 0});
    w.spawnEntity("c", EntityShape::Box, {3, 0, 0});
    const EntityHandle ha = w.handleOf("a");
    const EntityHandle hb = w.handleOf("b");
    const EntityHandle hc = w.handleOf("c");

    CHECK(w.isValid(ha));
    CHECK(w.handleOf("missing").isNull());
    REQUIRE(w.get(hb) != nullptr);
    CHECK(w.get(hb)->id == "b");

    SUBCASE("destroy makes the handle stale; others survive the array shuffle") {
        w.destroy("a");
        CHECK_FALSE(w.isValid(ha));
        REQUIRE(w.get(hc) != nullptr);
        CHECK(w.get(hc)->id == "c");
        CHECK(w.setPosition(hc, 9, 9, 9));
        CHECK(w.getPosition("c").x == 9.0f);
    }

    SUBCASE("a reused slot gets a new generation and never revives old handles") {
        w.destroy("a");
        w.spawnEntity("d", EntityShape::Box, {});
        const EntityHandle hd = w.handleOf("d");
        CHECK(hd.slot == ha.slot);
        CHECK(hd.generation != ha.generation);
        CHECK_FALSE(w.isValid(ha));
        CHECK(w.get(hd)->id == "d");
    }

    SUBCASE("respawning an existing name keeps the same handle") {
        w.spawnEntity("b", EntityShape::Sphere, {7, 0, 0});
        CHECK(w.handleOf("b") == hb);
        CHECK(w.get(hb)->shape == EntityShape::Sphere);
    }

    SUBCASE("clear invalidates every handle") {
        w.clear();
        CHECK_FALSE(w.isValid(ha));
        CHECK_FALSE(w.isValid(hb));
        CHECK(w.entityCount() == 0);
        w.spawnEntity("e", EntityShape::Box, {});
        CHECK(w.isValid(w.handleOf("e")));
    }
}

TEST_CASE("components exist only while used and are not inherited by a reused slot") {
    World w;
    w.spawnEntity("hero", EntityShape::Box, {});
    const EntityHandle h = w.handleOf("hero");

    CHECK(w.lookAt(h) == nullptr);
    CHECK(w.ragdoll(h) == nullptr);

    w.setLookAt("hero", {1, 2, 3}, 0.7f);
    REQUIRE(w.lookAt(h) != nullptr);
    CHECK(w.lookAt(h)->target.y == 2.0f);
    w.clearLookAt("hero");
    CHECK(w.lookAt(h) == nullptr);

    w.triggerRagdoll("hero", {5, 5, 0}, 1.0f);
    REQUIRE(w.ragdoll(h) != nullptr);
    CHECK(w.ragdoll(h)->active);
    CHECK(w.activeRagdollCount() == 1);

    w.destroy("hero");
    CHECK(w.activeRagdollCount() == 0);
    w.spawnEntity("next", EntityShape::Box, {});
    const EntityHandle hn = w.handleOf("next");
    CHECK(hn.slot == h.slot);
    CHECK(w.ragdoll(hn) == nullptr);
    CHECK(w.lookAt(hn) == nullptr);
}

TEST_CASE("ComponentStore: add, get, remove keep the dense array packed") {
    ComponentStore<int> store;
    store.getOrAdd(5) = 50;
    store.getOrAdd(2) = 20;
    store.getOrAdd(9) = 90;
    CHECK(store.size() == 3);
    store.remove(5); // Last element (slot 9) moves into the freed position
    CHECK(store.size() == 2);
    CHECK(store.get(5) == nullptr);
    REQUIRE(store.get(9) != nullptr);
    CHECK(*store.get(9) == 90);
    CHECK(*store.get(2) == 20);
    store.remove(5); // Removing twice is a no-op
    CHECK(store.size() == 2);
}
