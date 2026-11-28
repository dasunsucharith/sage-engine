# 023 — Per-Resource Saves, Crash-Safe Writes & Save Migration

## Plain-English Summary

SAGE runs several Lua resources (mods) at once, but they all shared one save drawer, `saves/<key>.dat`. If two mods both saved a `"progress"` key, one silently destroyed the other's data. Now every resource has its **own drawer**, `saves/<resource folder>/<key>.dat`, and scripts don't change at all: `sage.saveData("progress", ...)` just lands in the right place.

## Technical Deep Dive

**Per-resource `sage` table.** All resources share one `sol::state`, each in its own `sol::environment`. When a resource loads, the engine creates a fresh table for it:

```text
env.sage  = { saveData, loadData, deleteData }   -- bound to saves/<resource folder>/
metatable = { __index = <shared global sage> }   -- sage.log, sage.getTime, ... still work
```

A resource can't reach another resource's functions, and values a script stores on `sage` stay private to it.

**Crash-safe writes.** Data is written to `<key>.dat.tmp` and then renamed over `<key>.dat`. On Windows `std::filesystem::rename` replaces the target in one step. If the game crashes mid-write, the previous save survives instead of a half-written file.

**Lazy migration.** On `loadData(key)`, if `saves/<key>.dat` (the old layout) exists and this resource has no copy yet, it's copied into the resource folder, and the old file is renamed to `<key>.dat.migrated`. That keeps a backup, and no other resource can claim it later. If the old file is **newer** than the resource's copy (someone ran an older build that saved again), it's migrated again, so switching builds never loses progress.

**No silent magic.** Keys containing characters other than `A-Z a-z 0-9 _ -` still work, but with a warning that shows the stored name. Saves over 16 MB are refused with a message.

## How SAGE Uses This
* `stunt_sandbox` (`stunt_sandbox_best`) and `survival_game` (`wild_frontier_save`) need no code changes. Their existing saves migrate on first load. Verified: a best score of 1234 survived the move intact.
* `sage.deleteData(key)` is new, for "New Game" and reset buttons.
* The folder name is the resource's **folder** (unique on disk), not the manifest `name` (which two mods could share).

## Verified
In an isolated scratch project, two resources saving the same key kept separate values. Migration, the `.migrated` backup, re-migration of a newer old-location file, delete, key warnings and no leftover temp files all behaved as expected. Both shipped games pass their smoke tests and migrate their saves.
