# 029 — Entity Handles & Components

## Plain-English Summary

Every object in a SAGE world (a tree, the car, a deer) used to be one big record holding every possible feature: position, looks, animation, ragdoll, look-at and more, even for a rock that uses almost none of it. Objects could only be found by their text name, which means a string lookup every time.

Two changes fix that:
* **Handles:** every object gets a numeric ticket (slot + version). Finding an object by ticket is instant, and a ticket to a deleted object is recognised as stale instead of silently pointing at whatever reused its place.
* **Components:** optional features live in their own small stores, and an object only has the pieces it uses. Ragdoll and look-at are the first two moved out.

Nothing changes for game makers: Lua scripts and scene files still use names.

## Technical Deep Dive

### Generational handles (`EntityHandle` in `world.h`)
* `World` keeps entities in a packed array (`m_entities`) for fast iteration, plus a **slot table**: `slot → dense index` and `slot → generation`.
* An entity's slot never changes while it lives. When an entity is destroyed, the last entity is moved into its array position and only that entity's `slot → dense` entry is updated, so every handle stays valid.
* Destroying an entity bumps its slot's generation and frees the slot; `World::clear()` does the same for all slots. A reused slot gets a new generation, so `World::get(oldHandle)` returns `nullptr`.
* Name lookups go `name → slot → entity`. The names API is unchanged (`findEntity`, `setPosition(id, ...)`, …), and handle overloads were added for hot paths: `handleOf`, `get`, `isValid`, `setPosition(handle, ...)`, `setRotationQuat(handle, ...)`.

### Components (`ComponentStore<T>` in `component_store.h`)
A sparse set: `sparse[slot]` gives an index into tightly packed `dense` arrays. Insert, remove and lookup are O(1), and iterating a component type touches only entities that have it.
* **`LookAtComponent`** (16 bytes): added by `setLookAt`, removed by `clearLookAt`. That's equivalent to the old "enabled / weight 0" flags, which the renderer already treated as off.
* **`RagdollComponent`** (84 bytes): added by `triggerRagdoll` and kept afterwards, exactly like the old fields. Its recovered state is the starting point of the next ragdoll.
* **Entity record size:** 416 → 336 bytes (−19%).

### Keeping behaviour identical
* **Same order of side effects:** the ragdoll update still walks entities in array order and looks up each one's component, so ground-impact particles are spawned in exactly the same order (particle randomness depends on it).
* **Interpolation:** ragdoll blend and angle are snapshotted per component, then blended and restored. A newly added component starts with "previous" values of zero, matching the old per-entity snapshot.
* **Physics:** each body caches its entity's handle, resolved once from the name and again only if the entity was respawned. The per-tick write-back is an array lookup instead of a string hash per body.
* **Renderer:** `evaluateModelNodeMatrices` receives the look-at and ragdoll components (or `nullptr`), looked up via the world being rendered.

## Verification
* **Behaviour log:** a Lua test exercised spawn, destroy (with the array shuffle), respawn, `setPosition`, animation, look-at set and clear, a full ragdoll fall and get-up, and a falling physics box, logging every value every tick. Two pre-refactor runs agreed 112/112 ticks (the fixed simulation clock makes runs repeatable). After the refactor, **113/113 ticks matched the baseline exactly**, in two runs.
* **Handle unit test:** 18 checks covering stale handles after destroy, handles surviving the array shuffle, slot reuse with a new generation, components not inherited by a reused slot, and `clear()` invalidating everything.
* **Smoke tests:** all four scenes pass, and an interleaved benchmark against `main` showed no regression.

## How SAGE Uses This / Next steps
* **New per-entity features** (AI state, health, vehicle data, …) should be components, not new `Entity` fields.
* **Candidates to move next:** animation playback state (every model uses it today, so it needs care with interpolation and the renderer), and render data (shape, model, colour).
* **Possible Lua API:** expose handles (e.g. `world.getHandle(id)`) for scripts that update many entities every tick.
