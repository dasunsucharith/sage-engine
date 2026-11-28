// Render interpolation (knowledge-base entry 021).
#include "test_helpers.h"

#include "sage/world/world.h"

using namespace sage;
using sage_test::near;

TEST_CASE("render interpolation blends between ticks and restores the real state") {
    World w;
    w.spawnEntity("a", EntityShape::Box, {0, 0, 0});
    w.spawnEntity("b", EntityShape::Box, {0, 0, 0});
    w.setPointLight("l", {0, 0, 0}, {1, 1, 1});
    w.camera().position = {0, 0, 0};

    // Nothing to blend before the first tick (freshly spawned).
    w.applyRenderInterpolation(0.5f, true);
    CHECK(near(w.getPosition("a").x, 0));
    w.restoreFromRenderInterpolation();

    w.captureTickSnapshot();
    w.setPosition("a", 10, 0, 0);
    w.findEntity("a")->rotation.y = 10;
    w.findEntity("a")->interpPrev.rotation.y = 350;
    w.teleport("b", 5, 0, 0);
    w.setPointLight("l", {4, 0, 0}, {1, 1, 1});
    w.camera().position = {2, 0, 0};

    w.applyRenderInterpolation(0.25f, true);
    CHECK(near(w.getPosition("a").x, 2.5f));
    const float yaw = w.findEntity("a")->rotation.y;
    CHECK((near(yaw, 355.0f) || near(yaw, -5.0f))); // Shortest arc 350 -> 10
    CHECK(near(w.getPosition("b").x, 5));            // teleport() is never blended
    CHECK(near(w.pointLights()[0].position.x, 1.0f));
    CHECK(near(w.camera().position.x, 0.5f));
    w.applyRenderInterpolation(0.25f, true); // A second apply must be ignored
    CHECK(near(w.getPosition("a").x, 2.5f));
    w.restoreFromRenderInterpolation();
    CHECK(near(w.getPosition("a").x, 10));
    CHECK(near(w.findEntity("a")->rotation.y, 10));
    CHECK(near(w.pointLights()[0].position.x, 4));
    CHECK(near(w.camera().position.x, 2));

    // Moves over 25 m in one tick count as teleports.
    w.captureTickSnapshot();
    w.setPosition("a", 500, 0, 0);
    w.applyRenderInterpolation(0.5f, false);
    CHECK(near(w.getPosition("a").x, 500));
    CHECK(near(w.camera().position.x, 2)); // Camera excluded when asked
    w.restoreFromRenderInterpolation();

    // Unchanged values stay bit-identical (static-prop caches rely on it).
    w.captureTickSnapshot();
    w.applyRenderInterpolation(0.37f, true);
    CHECK(w.getPosition("a").x == 500.0f);
    w.restoreFromRenderInterpolation();

    // An animation restart snaps instead of rewinding.
    Entity* e = w.findEntity("a");
    e->animTime = 3.0f;
    w.captureTickSnapshot();
    e->animTime = 0.0f;
    w.applyRenderInterpolation(0.5f, true);
    CHECK(near(w.findEntity("a")->animTime, 0.0f));
    w.restoreFromRenderInterpolation();
}

TEST_CASE("ragdoll state is interpolated per component and restored") {
    World w;
    w.spawnEntity("npc", EntityShape::Box, {0, 0, 0});
    w.triggerRagdoll("npc", {0, 6, 0}, 2.0f);
    w.captureTickSnapshot();
    w.updateAnimations(1.0f / 60.0f); // Advances the ragdoll (blend rises)
    const EntityHandle h = w.handleOf("npc");
    const float realBlend = w.ragdoll(h)->blend;
    w.applyRenderInterpolation(0.5f, true);
    CHECK(w.ragdoll(h)->blend == doctest::Approx(realBlend * 0.5f));
    w.restoreFromRenderInterpolation();
    CHECK(w.ragdoll(h)->blend == realBlend);
}
