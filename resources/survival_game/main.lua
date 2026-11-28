-- =============================================================================
-- WILD FRONTIER: ECHO ISLAND — Open-World Survival Showcase (SAGE Engine v1.5.0)
--   * True Organic 3D Island Terrain (Zero 4-Block Quadrants — Coves, Lake & 70m Peak!)
--   * Seamless 3rd-Person <-> 1st-Person Camera (Press V!) with Animated FPS Tool/Torch
--   * Real Royalty-Free (CC0/MIT) Wilderness Ambience & 3D Spatial Foley SFX (miniaudio)
--   * 76,000 Interactive Wind-Swaying Grass Blades + Tiered 3-CSM + SSR + God-Rays
--   * Vitals, Day/Night, Storms, Gathering, Cooking, Base Building, Crafting & Rescue
--   * Look-At IK + Euphoria Active-Physics Ragdolls + Zero-Code Meshy .glb Auto-Swap
-- =============================================================================
-- CONTROLS:
--   W / A / S / D          : Move (A/D Strafe in 1st-Person, Turn in 3rd-Person)
--   Right-Mouse Drag       : Look Around (360° Free-Look in 1st-Person / Orbit in 3rd)
--   V                      : Toggle 1st-Person <-> 3rd-Person Camera!
--   C                      : Cycle 3rd-Person Camera Distance
--   Left Shift / Space     : Sprint (uses Stamina) / Jump
--   E                      : Gather / Loot / Drink Water / Cook Meat / Call Rescue
--   F or Left-Click        : Strike / Chop / Hunt (Quick Swing)
--   Q                      : Heavy Knockback Slam (Euphoria Ragdoll Launch!)
--   L                      : Toggle Handheld Fire Torch (Warmth + Night PBR Light)
--   K                      : Open / Close Crafting & Inventory Panel
--   1 / 2 / 3 / 4          : Quick Eat Berry / Eat Steak / Place Campfire / Use Bandage
--   T / G                  : Fast-Skip Time of Day / Cycle Weather (Clear/Drizzle/Storm)
--   F6                     : Quick-Save | Esc : Pause | F1 : Editor | F5 : Hot-Reload
-- =============================================================================

local function resolveAsset(meshyPath, fallbackPath, label)
    if sage.fileExists(meshyPath) then
        sage.log("[Meshy Auto-Swap] Loaded custom 3D asset (" .. label .. "): " .. meshyPath)
        return meshyPath, true
    end
    return fallbackPath, false
end

local ASSETS = {}
local MESHY_LOADED = {}

local SFX = {
    stepGrass1 = "assets/audio/survival/sfx_step_grass_1.wav",
    stepGrass2 = "assets/audio/survival/sfx_step_grass_2.wav",
    stepRock   = "assets/audio/survival/sfx_step_rock_1.wav",
    stepWood   = "assets/audio/survival/sfx_step_wood_1.wav",
    stepSand   = "assets/audio/survival/sfx_step_sand_1.wav",
    splash     = "assets/audio/survival/sfx_water_splash.wav",
    woodChop1  = "assets/audio/survival/sfx_wood_chop_1.wav",
    woodChop2  = "assets/audio/survival/sfx_wood_chop_2.wav",
    rockMine1  = "assets/audio/survival/sfx_rock_mine_1.wav",
    rockMine2  = "assets/audio/survival/sfx_rock_mine_2.wav",
    swing      = "assets/audio/survival/sfx_melee_swing.wav",
    hit        = "assets/audio/survival/sfx_hit_impact.wav",
    heavySlam  = "assets/audio/survival/sfx_heavy_slam.wav",
    heal       = "assets/audio/survival/sfx_cloth_heal.wav",
    craftOk    = "assets/audio/survival/sfx_craft_ok.wav",
    buildPlace = "assets/audio/survival/sfx_build_place.wav",
    wolfHowl   = "assets/audio/survival/sfx_wolf_howl.mp3",
    owl        = "assets/audio/survival/sfx_owl.mp3",
    thunder    = "assets/audio/survival/sfx_thunder.mp3",
}

local AMB = {
    forestWind   = "assets/audio/survival/amb_forest_wind.mp3",
    birds        = "assets/audio/survival/amb_birds.mp3",
    nightCricket = "assets/audio/survival/amb_night_crickets.mp3",
    rainHeavy    = "assets/audio/survival/amb_rain_heavy.mp3",
    oceanWaves   = "assets/audio/survival/amb_ocean_waves.mp3",
    campfire     = "assets/audio/survival/amb_campfire.mp3",
    mountainWind = "assets/audio/survival/amb_mountain_wind.mp3",
    seagulls     = "assets/audio/survival/amb_seagulls.mp3",
}

local function playSfxSafe(path, vol, pitch)
    local p = (pitch or 1.0) * (0.94 + math.random() * 0.12)
    if not audio.playSfx(path, vol or 0.85, p) then
        audio.playImpact(0.45)
    end
end

local function initAssets()
    ASSETS.survivor,  MESHY_LOADED.survivor  = resolveAsset("assets/models/survival/survivor.glb",      "assets/models/character_rigged.glb", "Survivor")
    ASSETS.wolf,      MESHY_LOADED.wolf      = resolveAsset("assets/models/survival/timber_wolf.glb",   "assets/models/hover_drone.glb",      "Timber Wolf")
    ASSETS.deer,      MESHY_LOADED.deer      = resolveAsset("assets/models/survival/island_deer.glb",   "assets/models/desert_rock.glb",      "Island Deer")
    ASSETS.raider,    MESHY_LOADED.raider    = resolveAsset("assets/models/survival/forest_raider.glb", "assets/models/character_rigged.glb", "Forest Raider")
    ASSETS.campfire,  MESHY_LOADED.campfire  = resolveAsset("assets/models/survival/campfire.glb",      "assets/models/desert_rock.glb",      "Campfire Station")
    ASSETS.plane,     MESHY_LOADED.plane     = resolveAsset("assets/models/survival/crashed_plane.glb", "assets/models/jet_plane.glb",        "Crashed Plane")
    ASSETS.shelter,   MESHY_LOADED.shelter   = resolveAsset("assets/models/survival/wood_shelter.glb",  "assets/models/alpine_cabin.glb",     "Wood Shelter")
    ASSETS.ore,       MESHY_LOADED.ore       = resolveAsset("assets/models/survival/ore_boulder.glb",   "assets/models/survival/survival_boulder.glb", "Ore Boulder")
    ASSETS.crate     = "assets/models/crate.glb"
    ASSETS.pine      = "assets/models/survival/survival_pine.glb"
    ASSETS.bush      = "assets/models/survival/survival_bush.glb"
    ASSETS.gull      = "assets/models/seagull.glb"
    ASSETS.heli      = "assets/models/helicopter.glb"
end

-- -----------------------------------------------------------------------------
-- Helpers
-- -----------------------------------------------------------------------------
local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function lerp(a, b, t)
    return a + (b - a) * clamp(t, 0.0, 1.0)
end

local function lerpAngle(curDeg, targetDeg, t)
    local diff = (targetDeg - curDeg) % 360.0
    if diff > 180.0 then diff = diff - 360.0 end
    if diff < -180.0 then diff = diff + 360.0 end
    return curDeg + diff * clamp(t, 0.0, 1.0)
end

local function degToRad(deg)
    return deg * (math.pi / 180.0)
end

local function dist2D(x1, z1, x2, z2)
    local dx, dz = x2 - x1, z2 - z1
    return math.sqrt(dx * dx + dz * dz)
end

local function getRegionName(x, y, z)
    if dist2D(x, z, 22.0, -4.0) < 42.0 then return "Emerald Freshwater Lake" end
    if dist2D(x, z, 80.0, -125.0) < 58.0 then return "Crescent Crash Cove" end
    if dist2D(x, z, -96.0, 128.0) < 44.0 then return "Echo Ridge Radio Outpost" end
    if dist2D(x, z, -126.0, -42.0) < 52.0 then return "Western Bluff Ruins" end
    if y > 28.0 then return "Mount Echo Snow Summit" end
    if y > 16.0 then return "Alpine Highland Ridge" end
    if y < 3.4 then return "Sandy Island Shoreline" end
    return "Whispering Pine Forest"
end

-- -----------------------------------------------------------------------------
-- Game State, Vitals, Inventory & Camera (1st-Person + 3rd-Person!)
-- -----------------------------------------------------------------------------
local gameState = "MENU" -- "MENU" | "PLAYING" | "PAUSED" | "DEAD" | "VICTORY"
local showCraftMenu = false
local showHud = true
local statusBanner = ""
local statusBannerTimer = 0.0

local worldClock = {
    day = 1,
    hour = 9.2,
    daySpeed = 0.088,
    weatherIdx = 1,
    weatherTimer = 60.0,
    rain = 0.0,
    wetness = 0.0,
    ambientCallTimer = 11.0,
    thunderTimer = 14.0,
}

local weatherPresets = {
    { name = "Clear Wilderness", rain = 0.00, wet = 0.00 },
    { name = "Coastal Drizzle",  rain = 0.35, wet = 0.55 },
    { name = "Island Rainstorm", rain = 0.92, wet = 1.00 },
}

local timeSkips = {
    { name = "Golden Dawn",   hour =  6.5 },
    { name = "Bright Noon",   hour = 12.6 },
    { name = "Amber Sunset",  hour = 17.5 },
    { name = "Starry Night",  hour = 22.8 },
}
local timeSkipIdx = 1

local player = {
    x = 62.0, y = 4.7, z = -108.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    yaw = -32.0, pitch = 0.0, roll = 0.0,
    firstPerson = false, -- Press [V] to switch 3rd-Person <-> 1st-Person!
    headBobPhase = 0.0,
    swingAnim = 0.0,
    attackAnimTimer = 0.0,
    grounded = true,
    swimming = false,
    currentAnim = "Idle",
    stepAccum = 0.0,
    stepToggle = false,
    splashAccum = 0.0,
    attackCooldown = 0.0,
    heavyCooldown = 0.0,
    hurtCooldown = 0.0,
    torchOn = false,
    torchFxAccum = 0.0,
    health = 100.0,
    hunger = 94.0,
    thirst = 92.0,
    warmth = 95.0,
    stamina = 100.0,
    nearFire = false,
    nearShelter = false,
    nearWater = false,
}

local inv = {
    wood = 4,
    stone = 2,
    iron = 0,
    berries = 2,
    rawMeat = 1,
    cookedMeat = 1,
    bandages = 1,
    radioParts = 0,
    hasSpear = false,
    hasBeacon = false,
}

local stats = {
    wolvesDefeated = 0,
    raidersDefeated = 0,
    deerHunted = 0,
    structuresBuilt = 0,
}

local camPresets = {
    { name = "Exploration",   dist = 5.4, height = 2.45, fov = 62.0 },
    { name = "Over-Shoulder", dist = 3.6, height = 1.92, fov = 60.0 },
    { name = "Wide Vista",    dist = 8.0, height = 3.45, fov = 66.0 },
}
local camPresetIdx = 1
local cam = { x = 58.0, y = 9.0, z = -102.0, tx = 62.0, ty = 6.2, tz = -108.0, fov = 62.0, orbitYaw = 0.0, orbitPitch = 8.0 }

local keyLatch = {
    E = false, F = false, Q = false, L = false, K = false,
    V = false, C = false, T = false, G = false, H = false, F6 = false,
    Escape = false, ["1"] = false, ["2"] = false, ["3"] = false, ["4"] = false,
}
local mouseLeftLatch = false

local function keyJustPressed(name)
    local down = input.isKeyDown(name)
    local trig = down and (not keyLatch[name])
    keyLatch[name] = down
    return trig
end

local function notify(msg, sec)
    statusBanner = msg
    statusBannerTimer = sec or 3.5
    sage.log("[Survival] " .. msg)
end

-- World Dynamic Lists
local harvestNodes = {}
local radioPickups = {}
local campfires = {}
local shelters = {}
local wildlife = {}
local raiders = {}
local minimapCells = {} -- Real sampled 32x32 organic island contour minimap!
local MINIMAP_N = 32

local seagulls = {
    { id = "surv_gull_1", cx = 92.0, cz = -142.0, r = 28.0, h = 16.0, spd =  0.42, phase = 0.0 },
    { id = "surv_gull_2", cx = 64.0, cz = -118.0, r = 34.0, h = 19.5, spd = -0.35, phase = 2.1 },
    { id = "surv_gull_3", cx = 22.0, cz =   -4.0, r = 24.0, h = 18.0, spd =  0.48, phase = 4.2 },
}

local rescue = {
    active = false,
    timer = 0.0,
    duration = 14.0,
    towerX = -96.0,
    towerZ = 128.0,
    heliX = -260.0,
    heliY = 78.0,
    heliZ = 270.0,
}

local nextBuildId = 0
local promptText = ""
local promptAction = nil
local titleAccum = 0.0
local crashSmokeAccum = 0.0

-- -----------------------------------------------------------------------------
-- Real Heightmap Contour Minimap Cache (Zero 4-Block Quadrants!)
-- -----------------------------------------------------------------------------
local function bakeOrganicMinimap()
    minimapCells = {}
    local span = 270.0
    for iz = 0, MINIMAP_N - 1 do
        for ix = 0, MINIMAP_N - 1 do
            local wx = -span + ((ix + 0.5) / MINIMAP_N) * (span * 2.0)
            local wz =  span - ((iz + 0.5) / MINIMAP_N) * (span * 2.0)
            local h = world.sampleTerrain(wx, wz)
            local r, g, b
            if h < -1.8 then
                r, g, b = 0.04, 0.22, 0.36      -- Deep ocean
            elseif h < 0.15 then
                r, g, b = 0.07, 0.48, 0.62      -- Turquoise coastal shallows & lake
            elseif h < 3.6 then
                r, g, b = 0.72, 0.62, 0.39      -- Warm golden beach sand
            elseif h < 15.5 then
                r, g, b = 0.14, 0.36, 0.16      -- Lush emerald pine valleys
            elseif h < 27.5 then
                r, g, b = 0.26, 0.30, 0.22      -- Alpine highland ridge
            elseif h < 36.0 then
                r, g, b = 0.40, 0.38, 0.37      -- Mountain granite cliffs
            else
                r, g, b = 0.88, 0.92, 0.96      -- Mount Echo snow summit
            end
            table.insert(minimapCells, { ix = ix, iz = iz, r = r, g = g, b = b })
        end
    end
end

-- -----------------------------------------------------------------------------
-- Persistent Save / Load (F6 Quick-Save -> saves/wild_frontier_save.dat)
-- -----------------------------------------------------------------------------
local function saveGame()
    local payload = string.format(
        "%.2f|%.2f|%.2f|%.2f|%.1f|%.1f|%.1f|%.1f|%.1f|%d|%.2f|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d",
        player.x, player.y, player.z, player.yaw,
        player.health, player.hunger, player.thirst, player.warmth, player.stamina,
        worldClock.day, worldClock.hour,
        inv.wood, inv.stone, inv.iron, inv.berries, inv.rawMeat, inv.cookedMeat, inv.bandages,
        inv.radioParts, inv.hasSpear and 1 or 0, inv.hasBeacon and 1 or 0,
        stats.wolvesDefeated, stats.raidersDefeated, stats.structuresBuilt
    )
    if sage.saveData("wild_frontier_save", payload) then
        playSfxSafe(SFX.craftOk, 0.80, 1.05)
        notify("Game Saved! (Day " .. worldClock.day .. ")", 3.0)
        return true
    end
    return false
end

local function hasSavedGame()
    local raw = sage.loadData("wild_frontier_save")
    return raw ~= nil and #raw > 10
end

local function loadSavedGame()
    local raw = sage.loadData("wild_frontier_save")
    if not raw or #raw < 10 then return false end
    local parts = {}
    for token in string.gmatch(raw, "([^|]+)") do
        table.insert(parts, tonumber(token) or 0)
    end
    if #parts < 21 then return false end
    player.x, player.y, player.z, player.yaw = parts[1], parts[2], parts[3], parts[4]
    player.health, player.hunger, player.thirst, player.warmth, player.stamina = parts[5], parts[6], parts[7], parts[8], parts[9]
    worldClock.day, worldClock.hour = math.max(1, math.floor(parts[10])), parts[11]
    inv.wood, inv.stone, inv.iron, inv.berries = math.floor(parts[12]), math.floor(parts[13]), math.floor(parts[14]), math.floor(parts[15])
    inv.rawMeat, inv.cookedMeat, inv.bandages, inv.radioParts = math.floor(parts[16]), math.floor(parts[17]), math.floor(parts[18]), math.floor(parts[19])
    inv.hasSpear = (parts[20] == 1)
    inv.hasBeacon = (parts[21] == 1)
    if parts[22] then stats.wolvesDefeated = math.floor(parts[22]) end
    if parts[23] then stats.raidersDefeated = math.floor(parts[23]) end
    if parts[24] then stats.structuresBuilt = math.floor(parts[24]) end

    player.y = math.max(player.y, world.sampleTerrain(player.x, player.z))
    world.setPosition("player_survivor", player.x, player.y, player.z)
    world.setTimeOfDay(worldClock.hour)
    for i, rp in ipairs(radioPickups) do
        if i <= inv.radioParts then
            rp.collected = true
            world.setVisible(rp.id, false)
            world.setVisible(rp.ringId, false)
            world.removePointLight(rp.lightId)
        end
    end
    notify("Loaded Saved Expedition (Day " .. worldClock.day .. ")!", 3.5)
    return true
end

-- -----------------------------------------------------------------------------
-- Structure Placement (Campfires & Shelters on Organic 3D Terrain!)
-- -----------------------------------------------------------------------------
local function placeCampfire(x, z, freeStarter)
    nextBuildId = nextBuildId + 1
    local id = "campfire_" .. nextBuildId
    local flameId = id .. "_flame"
    local lightId = id .. "_light"
    local y = world.sampleTerrain(x, z)

    world.spawnModel(id, ASSETS.campfire, x, y, z)
    if MESHY_LOADED.campfire then
        world.setScale(id, 1.0, 1.0, 1.0)
        world.playAnimation(id, "Idle", true, 0.85, 0.0) -- Gently swaying cast-iron cooking pot!
    else
        world.setScale(id, 0.72, 0.32, 0.72)
        world.setColor(id, 0.42, 0.36, 0.32)
    end
    world.spawnSphere(flameId, x, y + (MESHY_LOADED.campfire and 0.22 or 0.34), z)
    world.setScale(flameId, MESHY_LOADED.campfire and 0.17 or 0.42, MESHY_LOADED.campfire and 0.21 or 0.54, MESHY_LOADED.campfire and 0.17 or 0.42)
    world.setColor(flameId, 0.92, 0.36, 0.06)
    world.setEmissive(flameId, true)
    world.setPointLight(lightId, x, y + 1.35, z, 1.0, 0.56, 0.18, 24.0, 2.8)

    table.insert(campfires, { id = id, flameId = flameId, lightId = lightId, x = x, y = y, z = z, fxAccum = 0.0 })
    if not freeStarter then
        stats.structuresBuilt = stats.structuresBuilt + 1
        playSfxSafe(SFX.buildPlace, 0.90, 1.0)
        notify("Built Campfire! Stand near it for Warmth & press [E] to cook Raw Meat.", 4.0)
    end
end

local function placeWoodShelter(x, z, yawDeg)
    nextBuildId = nextBuildId + 1
    local id = "shelter_" .. nextBuildId
    local lightId = id .. "_lantern"
    local y = world.sampleTerrain(x, z)
    world.spawnModel(id, ASSETS.shelter, x, y, z)
    world.setRotation(id, 0.0, yawDeg, 0.0)
    world.setScale(id, 1.05, 1.05, 1.05)
    physics.addStaticBox(id, x, y + 2.2, z, 3.2, 2.2, 3.0)
    world.setPointLight(lightId, x, y + 3.2, z, 1.0, 0.76, 0.38, 18.0, 2.2)

    table.insert(shelters, { id = id, x = x, y = y, z = z })
    stats.structuresBuilt = stats.structuresBuilt + 1
    playSfxSafe(SFX.buildPlace, 0.95, 0.85)
    notify("Constructed Wilderness Shelter Cabin! Warm & safe from storms.", 4.0)
end

-- -----------------------------------------------------------------------------
-- First-Person Tool / Torch Viewmodel Helpers
-- -----------------------------------------------------------------------------
local function setFirstPersonViewmodelVisible(vis)
    world.setVisible("fp_tool_handle", vis)
    world.setVisible("fp_tool_head", vis)
end

local function toggleCameraPerspective()
    player.firstPerson = not player.firstPerson
    world.setVisible("player_survivor", not player.firstPerson)
    setFirstPersonViewmodelVisible(player.firstPerson)
    if player.firstPerson then
        cam.orbitYaw = 0.0
        notify("Camera -> 1ST-PERSON MODE (A/D Strafe, RMB Look, V = 3rd-Person)", 2.8)
    else
        player.pitch = 0.0
        notify("Camera -> 3RD-PERSON MODE (V = 1st-Person, C = Distance)", 2.4)
    end
end

-- -----------------------------------------------------------------------------
-- Spawn Organic Open-World Entities & Real Wilderness Ambience
-- -----------------------------------------------------------------------------
local function spawnSurvivalWorld()
    initAssets()
    world.setTerrainMode("island")
    bakeOrganicMinimap()

    -- 1. Playable 3rd-Person Rigged Survivor (grounded cleanly on Crash Beach!)
    local spawnY = world.sampleTerrain(player.x, player.z)
    player.y = spawnY
    world.spawnModel("player_survivor", ASSETS.survivor, player.x, player.y, player.z)
    world.setRotation("player_survivor", 0.0, player.yaw, 0.0)
    world.playAnimation("player_survivor", "Idle", true, 1.0, 0.0)
    player.currentAnim = "Idle"

    -- First-Person Held Survival Axe / Iron Spear Viewmodel (visible only in 1st-Person mode!)
    world.spawnBox("fp_tool_handle", player.x, player.y + 1.3, player.z)
    world.setScale("fp_tool_handle", 0.045, 0.56, 0.045)
    world.setColor("fp_tool_handle", 0.42, 0.26, 0.14)
    world.spawnBox("fp_tool_head", player.x, player.y + 1.55, player.z)
    world.setScale("fp_tool_head", 0.06, 0.16, 0.22)
    world.setColor("fp_tool_head", 0.62, 0.66, 0.72)
    setFirstPersonViewmodelVisible(false)

    -- Handheld Torch Visual
    world.spawnSphere("player_torch", player.x, player.y + 1.4, player.z)
    world.setScale("player_torch", 0.20, 0.28, 0.20)
    world.setColor("player_torch", 1.0, 0.60, 0.14)
    world.setEmissive("player_torch", true)
    world.setVisible("player_torch", false)

    -- 2. Crashed Bush Plane Wreckage resting on Crescent Crash Cove Beach!
    local planeX, planeZ = 74.0, -118.0
    local planeY = world.sampleTerrain(planeX, planeZ)
    world.spawnModel("crashed_plane", ASSETS.plane, planeX, planeY + (MESHY_LOADED.plane and -0.05 or 0.95), planeZ)
    if MESHY_LOADED.plane then
        world.setRotation("crashed_plane", 3.5, 142.0, -5.0)
        world.setScale("crashed_plane", 1.0, 1.0, 1.0)
        world.playAnimation("crashed_plane", "Idle", true, 0.65, 0.0) -- Coastal wind creaking propeller!
    else
        world.setRotation("crashed_plane", 12.0, 146.0, -16.0)
        world.setScale("crashed_plane", 0.95, 0.95, 0.95)
        world.setColor("crashed_plane", 0.86, 0.78, 0.72)
    end
    physics.addStaticBox("crashed_plane", planeX, planeY + 1.4, planeZ, 4.4, 1.6, 4.4)

    -- Starter Crash-Beach Campfire right beside spawn!
    placeCampfire(66.5, -111.5, true)

    -- Washed-up Jolt Physics Supply Crates along Crescent Beach!
    local crateSpots = {
        { 58.0, -113.0 }, { 60.5, -115.0 }, { 71.0, -106.0 },
        { 81.0, -112.0 }, { 76.0, -102.0 },
    }
    for i, cp in ipairs(crateSpots) do
        local cid = "crash_crate_" .. i
        local cy = world.sampleTerrain(cp[1], cp[2]) + 0.75
        world.spawnModel(cid, ASSETS.crate, cp[1], cy, cp[2])
        physics.addDynamicBox(cid, cp[1], cy, cp[2], 0.64, 0.64, 0.64, 18.0, 0.28, 0.68)
    end

    -- 3. Harvestable Open-World Resource Nodes (placed cleanly on dry organic island terrain!)
    harvestNodes = {}
    local function addNode(kind, label, x, z)
        local y = world.sampleTerrain(x, z)
        if y < 0.6 then return end
        local idx = #harvestNodes + 1
        local id = "res_" .. kind .. "_" .. idx
        if kind == "wood" then
            world.spawnModel(id, ASSETS.pine, x, y - 0.10, z)
            world.setScale(id, 0.92, 0.96, 0.92)
            world.setColor(id, 1.0, 1.0, 1.0)
        elseif kind == "ore" then
            world.spawnModel(id, ASSETS.ore, x, y - 0.12, z)
            world.setScale(id, 1.08, 0.92, 1.08)
            world.setColor(id, 0.80, 0.85, 0.94)
        elseif kind == "berries" then
            world.spawnModel(id, ASSETS.bush, x, y - 0.08, z)
            world.setScale(id, 1.05, 1.02, 1.05)
            world.setColor(id, 1.0, 1.0, 1.0)
        end
        table.insert(harvestNodes, {
            id = id, kind = kind, label = label,
            x = x, y = y, z = z, hp = 3, maxHp = 3,
            depleted = false, respawnTimer = 0.0,
        })
    end

    local woodPts = {
        { 52, -96 }, { 44, -104 }, { 64, -84 }, { 48, -62 },
        { 72, -32 }, { 88,  14 }, { 78,  48 }, { 42,  66 },
        { -42, 46 }, { -72, 92 }, { -58, -24 }, { 12, -52 },
    }
    for _, p in ipairs(woodPts) do addNode("wood", "Pine Timber", p[1], p[2]) end

    local orePts = {
        { 48, -114 }, { 82, -96 }, { 56, -42 }, { 92, 36 },
        { -78, -32 }, { -112, -36 }, { -86, 108 }, { -48, 72 },
    }
    for _, p in ipairs(orePts) do addNode("ore", "Iron Ore Vein", p[1], p[2]) end

    local berryPts = {
        { 56, -102 }, { 68, -98 }, { 44, -84 }, { 62, 18 },
        { 84, 38 }, { -78, 118 }, { -96, -48 }, { 14, -36 },
    }
    for _, p in ipairs(berryPts) do addNode("berries", "Wild Berry Bush", p[1], p[2]) end

    -- 4. Three Mission Radio Components (Crash Cove, Western Bluff Ruins, Lakeside Trapper Cabin!)
    radioPickups = {
        { id = "radio_part_1", name = "Flight Black Box",   x =  84.5, z = -121.0, collected = false },
        { id = "radio_part_2", name = "Ruins Power Cell",   x = -118.0, z =  -46.0, collected = false },
        { id = "radio_part_3", name = "Alpine Transceiver", x =  74.0, z =   34.0, collected = false },
    }
    for _, rp in ipairs(radioPickups) do
        local ry = world.sampleTerrain(rp.x, rp.z)
        rp.y = ry
        rp.ringId = rp.id .. "_ring"
        rp.lightId = rp.id .. "_light"
        world.spawnBox(rp.id, rp.x, ry + 0.50, rp.z)
        world.setScale(rp.id, 0.58, 0.40, 0.58)
        world.setColor(rp.id, 0.14, 0.78, 0.92)
        world.setEmissive(rp.id, true)
        world.spawnRing(rp.ringId, rp.x, ry + 0.08, rp.z)
        world.setScale(rp.ringId, 2.2, 0.14, 2.2)
        world.setColor(rp.ringId, 0.95, 0.72, 0.16)
        world.setEmissive(rp.ringId, true)
        world.setPointLight(rp.lightId, rp.x, ry + 1.4, rp.z, 0.20, 0.84, 0.96, 12.0, 1.4)
    end

    -- Echo Ridge Radio Tower Activation Ring!
    local towerY = world.sampleTerrain(rescue.towerX, rescue.towerZ)
    world.spawnRing("radio_tower_ring", rescue.towerX, towerY + 0.12, rescue.towerZ + 7.5)
    world.setScale("radio_tower_ring", 5.5, 0.22, 5.5)
    world.setColor("radio_tower_ring", 0.95, 0.26, 0.18)
    world.setEmissive("radio_tower_ring", true)

    -- 5. Living Wildlife Ecosystem (5 Timber Wolves + 5 Island Deer on Organic Terrain!)
    wildlife = {}
    local wolfSpawns = {
        {  38.0, -48.0 }, {  96.0,  58.0 }, { -34.0, -44.0 },
        { -68.0,  22.0 }, { -58.0,  94.0 },
    }
    for i, wp in ipairs(wolfSpawns) do
        local id = "wolf_" .. i
        local wy = world.sampleTerrain(wp[1], wp[2])
        world.spawnModel(id, ASSETS.wolf, wp[1], wy, wp[2])
        if MESHY_LOADED.wolf then
            world.setScale(id, 1.05, 1.05, 1.05)
            world.playAnimation(id, "Idle", true, 1.0, 0.0)
        else
            world.setScale(id, 0.88, 0.78, 1.15)
            world.setColor(id, 0.42, 0.44, 0.50)
            world.playAnimation(id, "", true, 1.0, 0.0)
        end
        table.insert(wildlife, {
            id = id, kind = "wolf", homeX = wp[1], homeZ = wp[2],
            x = wp[1], y = wy, z = wp[2], yaw = i * 65.0,
            hp = 60.0, maxHp = 60.0, alive = true, respawnTimer = 0.0,
            state = "PATROL", timer = 2.0, targetX = wp[1] + 12.0, targetZ = wp[2] - 8.0,
            attackCd = 0.0, attackAnimTimer = 0.0, anim = "Idle",
        })
    end

    local deerSpawns = {
        {  48.0, -76.0 }, {  76.0, -18.0 }, {  92.0,  22.0 },
        { -16.0,  36.0 }, { -44.0,  58.0 },
    }
    for i, dp in ipairs(deerSpawns) do
        local id = "deer_" .. i
        local dy = world.sampleTerrain(dp[1], dp[2])
        world.spawnModel(id, ASSETS.deer, dp[1], dy, dp[2])
        if MESHY_LOADED.deer then
            world.setScale(id, 1.0, 1.0, 1.0)
            world.playAnimation(id, "Idle", true, 1.0, 0.0)
        else
            world.setScale(id, 0.72, 0.92, 1.18)
            world.setColor(id, 0.66, 0.44, 0.24)
        end
        table.insert(wildlife, {
            id = id, kind = "deer", homeX = dp[1], homeZ = dp[2],
            x = dp[1], y = dy, z = dp[2], yaw = i * 80.0,
            hp = 40.0, maxHp = 40.0, alive = true, respawnTimer = 0.0,
            state = "GRAZE", timer = 2.5, pauseTimer = (i % 2 == 0) and 4.0 or 0.0,
            targetX = dp[1] - 10.0, targetZ = dp[2] + 10.0,
            attackCd = 0.0, anim = "Idle",
        })
    end

    -- 6. Hostile Forest Raiders guarding Western Bluff Ruins & Echo Ridge Radio Outpost!
    raiders = {}
    local raiderSpawns = {
        { -114.0, -40.0 }, { -132.0, -52.0 },
        {  -90.0, 118.0 }, { -104.0, 134.0 },
    }
    for i, rp in ipairs(raiderSpawns) do
        local id = "raider_" .. i
        local ry = world.sampleTerrain(rp[1], rp[2])
        world.spawnModel(id, ASSETS.raider, rp[1], ry, rp[2])
        if not MESHY_LOADED.raider then
            world.setColor(id, 0.82, 0.36, 0.30)
        end
        world.playAnimation(id, "Walk", true, 0.9, 0.0)
        table.insert(raiders, {
            id = id, homeX = rp[1], homeZ = rp[2],
            x = rp[1], y = ry, z = rp[2], yaw = i * 90.0,
            hp = 85.0, maxHp = 85.0, alive = true, respawnTimer = 0.0,
            state = "PATROL", timer = 2.0, targetX = rp[1] + 10.0, targetZ = rp[2] + 8.0,
            attackCd = 0.0, anim = "Walk", ragdolling = false,
        })
    end

    -- 7. Soaring Seagulls + Rescue Helicopter
    for _, g in ipairs(seagulls) do
        world.spawnModel(g.id, ASSETS.gull, g.cx + g.r, g.h, g.cz)
        world.setScale(g.id, 1.25, 1.25, 1.25)
        world.playAnimation(g.id, "", true, 1.35)
    end
    world.spawnModel("rescue_heli", ASSETS.heli, rescue.heliX, rescue.heliY, rescue.heliZ)
    world.setScale("rescue_heli", 1.35, 1.35, 1.35)
    world.playAnimation("rescue_heli", "", true, 2.6)
    world.setVisible("rescue_heli", false)

    -- 8. REAL Royalty-Free Wilderness Ambient Soundscape (8 Streaming 2D/3D Channels!)
    audio.stopAllAmbientLoops()
    audio.setAmbientLoop(0, AMB.forestWind,   0.38)
    audio.setAmbientLoop(1, AMB.birds,        0.34)
    audio.setAmbientLoop(2, AMB.nightCricket, 0.00)
    audio.setAmbientLoop(3, AMB.rainHeavy,    0.00)
    audio.setAmbientLoop(4, AMB.oceanWaves,   0.85,  88.0, 0.5, -138.0, 145.0) -- 3D Surf at Crash Cove
    audio.setAmbientLoop(5, AMB.campfire,     0.90,  66.5, world.sampleTerrain(66.5, -111.5) + 0.6, -111.5, 24.0)
    audio.setAmbientLoop(6, AMB.mountainWind, 0.72, -48.0, 32.0,  118.0, 155.0) -- 3D Alpine Summit Wind
    audio.setAmbientLoop(7, AMB.seagulls,     0.42,  82.0, 16.0, -132.0, 110.0) -- 3D Coastal Seagulls
end

-- -----------------------------------------------------------------------------
-- Combat, Gathering & Euphoria Knockback Actions (with Real CC0 Foley SFX!)
-- -----------------------------------------------------------------------------
local function damagePlayer(amount, sourceLabel)
    if player.hurtCooldown > 0.0 or gameState ~= "PLAYING" then return end
    player.health = math.max(0.0, player.health - amount)
    player.hurtCooldown = 0.75
    playSfxSafe(SFX.hit, 0.95, 0.85)
    world.spawnParticles(player.x, player.y + 1.1, player.z, 0.0, 2.8, 0.0, 0.95, 0.14, 0.12, 10, 0.16, 0.45, false, 12.0)
    notify("Hit by " .. sourceLabel .. "! (-" .. math.floor(amount) .. " HP)", 2.2)
    if player.health <= 0.0 then
        gameState = "DEAD"
        if player.firstPerson then toggleCameraPerspective() end
        world.triggerRagdoll("player_survivor", -math.sin(degToRad(player.yaw)) * 7.0, 5.5, -math.cos(degToRad(player.yaw)) * 7.0, 4.0)
        notify("YOU PERISHED IN THE WILDERNESS — Press Respawn to try again!", 6.0)
    end
end

local function performStrike(heavy)
    player.swingAnim = 1.0
    if MESHY_LOADED.survivor and not player.firstPerson then
        player.attackAnimTimer = heavy and 0.56 or 0.44
        player.currentAnim = "Attack"
        world.playAnimation("player_survivor", "Attack", false, heavy and 1.15 or 1.45, 0.08)
    end
    local dmg = (inv.hasSpear and 44.0 or 22.0) * (heavy and 1.65 or 1.0)
    local reach = heavy and 7.2 or 4.8
    local prad = degToRad(player.yaw)
    local fx, fz = math.sin(prad), math.cos(prad)

    if heavy then
        playSfxSafe(SFX.heavySlam, 0.95, 0.92)
        world.spawnParticles(player.x + fx * 1.8, player.y + 1.1, player.z + fz * 1.8, fx * 11.0, 2.6, fz * 11.0, 0.25, 0.88, 1.0, 18, 0.22, 0.48, true, 6.0)
        physics.applyExplosion(player.x + fx * 3.5, player.y + 0.8, player.z + fz * 3.5, 9.0, 240.0)
    else
        playSfxSafe(SFX.swing, 0.72, 1.05)
    end

    local hitSomething = false

    for _, r in ipairs(raiders) do
        if r.alive then
            local d = dist2D(player.x, player.z, r.x, r.z)
            if d <= reach then
                hitSomething = true
                r.hp = r.hp - dmg
                playSfxSafe(SFX.hit, 0.92, 1.0)
                local invD = d > 0.1 and (1.0 / d) or 1.0
                local kx, kz = (r.x - player.x) * invD, (r.z - player.z) * invD
                if heavy or r.hp <= 0.0 then
                    local power = heavy and 15.5 or 9.5
                    world.triggerRagdoll(r.id, kx * power, heavy and 9.5 or 6.5, kz * power, r.hp <= 0.0 and 3.5 or 2.2)
                    r.ragdolling = true
                end
                world.spawnParticles(r.x, r.y + 1.2, r.z, kx * 4.0, 3.2, kz * 4.0, 1.0, 0.35, 0.18, 10, 0.16, 0.42, true, 12.0)
                if r.hp <= 0.0 then
                    r.alive = false
                    r.respawnTimer = 65.0
                    stats.raidersDefeated = stats.raidersDefeated + 1
                    inv.iron = inv.iron + 2
                    inv.bandages = inv.bandages + 1
                    playSfxSafe(SFX.craftOk, 0.85, 1.0)
                    notify("Defeated Forest Raider! Looted +2 Iron, +1 Bandage", 3.5)
                else
                    notify(string.format("Struck Raider! (%d/%d HP)", math.ceil(r.hp), r.maxHp), 1.8)
                end
            end
        end
    end

    for _, w in ipairs(wildlife) do
        if w.alive then
            local d = dist2D(player.x, player.z, w.x, w.z)
            if d <= reach then
                hitSomething = true
                w.hp = w.hp - dmg
                playSfxSafe(SFX.hit, 0.88, 1.1)
                local invD = d > 0.1 and (1.0 / d) or 1.0
                local kx, kz = (w.x - player.x) * invD, (w.z - player.z) * invD
                if heavy or w.hp <= 0.0 then
                    world.triggerRagdoll(w.id, kx * 13.0, 8.5, kz * 13.0, 2.6)
                end
                world.spawnParticles(w.x, w.y + 0.9, w.z, kx * 3.5, 2.8, kz * 3.5, 1.0, 0.72, 0.32, 9, 0.15, 0.40, false, 12.0)
                if w.hp <= 0.0 then
                    w.alive = false
                    w.respawnTimer = 55.0
                    inv.rawMeat = inv.rawMeat + 2
                    if w.kind == "wolf" then
                        stats.wolvesDefeated = stats.wolvesDefeated + 1
                        notify("Hunted Timber Wolf! +2 Raw Meat", 3.2)
                    else
                        stats.deerHunted = stats.deerHunted + 1
                        notify("Hunted Island Stag! +2 Raw Meat", 3.2)
                    end
                    playSfxSafe(SFX.craftOk, 0.80, 1.0)
                else
                    if w.kind == "deer" then w.state = "FLEE"; w.timer = 4.5 end
                    notify(string.format("Hit %s (%d/%d HP)", w.kind == "wolf" and "Wolf" or "Stag", math.ceil(w.hp), w.maxHp), 1.6)
                end
            end
        end
    end

    if not hitSomething then
        for _, n in ipairs(harvestNodes) do
            if not n.depleted and dist2D(player.x, player.z, n.x, n.z) <= 4.6 then
                local hitPower = inv.hasSpear and 2 or 1
                n.hp = n.hp - hitPower
                if n.kind == "wood" then
                    playSfxSafe(math.random() < 0.5 and SFX.woodChop1 or SFX.woodChop2, 0.95, 1.0)
                elseif n.kind == "ore" then
                    playSfxSafe(math.random() < 0.5 and SFX.rockMine1 or SFX.rockMine2, 0.92, 1.0)
                else
                    playSfxSafe(SFX.heal, 0.75, 1.15)
                end
                local col = (n.kind == "wood") and {0.58, 0.40, 0.22} or ((n.kind == "ore") and {0.72, 0.80, 0.94} or {0.92, 0.18, 0.30})
                world.spawnParticles(n.x, n.y + 1.1, n.z, 0.0, 3.8, 0.0, col[1], col[2], col[3], 10, 0.16, 0.45, false, 12.0)
                if n.hp <= 0 then
                    n.depleted = true
                    n.respawnTimer = 45.0
                    world.setVisible(n.id, false)
                    playSfxSafe(SFX.craftOk, 0.80, 1.05)
                    if n.kind == "wood" then
                        inv.wood = inv.wood + 3
                        notify("Felled Pine Timber! +3 Wood", 2.4)
                    elseif n.kind == "ore" then
                        inv.stone = inv.stone + 2
                        inv.iron = inv.iron + 2
                        notify("Mined Ore Vein! +2 Stone, +2 Iron", 2.4)
                    else
                        inv.berries = inv.berries + 3
                        notify("Foraged Wild Berries! +3 Berries", 2.4)
                    end
                else
                    notify("Gathering " .. n.label .. " (" .. n.hp .. " hits left)", 1.2)
                end
                break
            end
        end
    end
end

-- -----------------------------------------------------------------------------
-- Craft & Consume Helpers
-- -----------------------------------------------------------------------------
local function craftCampfire()
    if inv.wood < 3 or inv.stone < 2 then notify("Need 3 Wood & 2 Stone to build a Campfire!", 2.5); return end
    inv.wood = inv.wood - 3; inv.stone = inv.stone - 2
    local rad = degToRad(player.yaw)
    placeCampfire(player.x + math.sin(rad) * 3.0, player.z + math.cos(rad) * 3.0, false)
end

local function craftShelter()
    if inv.wood < 8 or inv.stone < 4 then notify("Need 8 Wood & 4 Stone to build a Wooden Shelter!", 2.5); return end
    inv.wood = inv.wood - 8; inv.stone = inv.stone - 4
    local rad = degToRad(player.yaw)
    placeWoodShelter(player.x + math.sin(rad) * 6.5, player.z + math.cos(rad) * 6.5, player.yaw + 180.0)
end

local function craftSpear()
    if inv.hasSpear then notify("You already crafted the Iron Survival Spear!", 2.2); return end
    if inv.wood < 3 or inv.iron < 3 then notify("Need 3 Wood & 3 Iron for Iron Spear!", 2.5); return end
    inv.wood = inv.wood - 3; inv.iron = inv.iron - 3
    inv.hasSpear = true
    world.setColor("fp_tool_head", 0.78, 0.86, 0.96)
    world.setScale("fp_tool_head", 0.05, 0.34, 0.09)
    playSfxSafe(SFX.craftOk, 0.90, 1.1)
    notify("Crafted Iron Survival Spear! (2x Combat & Gathering Power!)", 3.8)
end

local function craftBandage()
    if inv.wood < 1 or inv.berries < 2 then notify("Need 1 Wood & 2 Berries to craft Herbal Bandage!", 2.4); return end
    inv.wood = inv.wood - 1; inv.berries = inv.berries - 2
    inv.bandages = inv.bandages + 1
    playSfxSafe(SFX.heal, 0.85, 1.0)
    notify("Crafted Herbal Bandage (+38 HP)!", 2.4)
end

local function craftRescueBeacon()
    if inv.hasBeacon then notify("Rescue Beacon ready! Bring it to Echo Ridge Radio Tower!", 3.0); return end
    if inv.radioParts < 3 or inv.iron < 4 then
        notify(string.format("Need 3 Radio Parts (%d/3) & 4 Iron (%d/4)!", inv.radioParts, inv.iron), 3.0)
        return
    end
    inv.iron = inv.iron - 4
    inv.hasBeacon = true
    playSfxSafe(SFX.craftOk, 0.95, 1.15)
    notify("CRAFTED RESCUE BEACON! Head to Echo Ridge Radio Tower (NW) and press [E]!", 5.0)
end

local function useBerry()
    if inv.berries <= 0 then notify("No Wild Berries left!", 1.8); return end
    inv.berries = inv.berries - 1
    player.hunger = math.min(100.0, player.hunger + 18.0)
    player.thirst = math.min(100.0, player.thirst + 10.0)
    playSfxSafe(SFX.heal, 0.70, 1.25)
    notify("Ate Wild Berries (+18 Hunger, +10 Thirst)", 2.0)
end

local function useSteak()
    if inv.cookedMeat <= 0 then notify("No Cooked Steak! Cook Raw Meat at a Campfire [E].", 2.4); return end
    inv.cookedMeat = inv.cookedMeat - 1
    player.hunger = math.min(100.0, player.hunger + 48.0)
    player.health = math.min(100.0, player.health + 24.0)
    playSfxSafe(SFX.heal, 0.80, 0.92)
    notify("Ate Roasted Steak (+48 Hunger, +24 HP!)", 2.4)
end

local function useBandage()
    if inv.bandages <= 0 then notify("No Bandages left! Craft one in [K].", 2.0); return end
    inv.bandages = inv.bandages - 1
    player.health = math.min(100.0, player.health + 38.0)
    playSfxSafe(SFX.heal, 0.90, 1.0)
    notify("Applied Herbal Bandage (+38 HP)", 2.2)
end

-- -----------------------------------------------------------------------------
-- FiveM-Style Lifecycle Callbacks
-- -----------------------------------------------------------------------------
function onStart()
    math.randomseed(777)
    spawnSurvivalWorld()
    world.setTimeOfDay(worldClock.hour)
    world.setWeather(0.0, 0.0)
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)

    local meshyCount = 0
    for _, v in pairs(MESHY_LOADED) do if v then meshyCount = meshyCount + 1 end end

    sage.log("=======================================================================")
    sage.log("  WILD FRONTIER: ECHO ISLAND — Organic 3D Open-World Survival (v1.6.0)")
    sage.log("  Press V = Toggle 1st-Person <-> 3rd-Person | Real CC0/MIT Audio Active!")
    sage.log("  Custom Meshy Hero Assets Active: " .. meshyCount .. " / 8 (assets/models/survival/)")
    sage.log("=======================================================================")
end

function onTick(dt)
    local timeNow = sage.getTime()
    if statusBannerTimer > 0.0 then statusBannerTimer = math.max(0.0, statusBannerTimer - dt) end
    if player.swingAnim > 0.0 then player.swingAnim = math.max(0.0, player.swingAnim - dt * 3.6) end

    -- Always animate seagulls, crash plane smoke, flickering campfires & radio beacons!
    for _, g in ipairs(seagulls) do
        local ga = timeNow * g.spd + g.phase
        local gx, gz = g.cx + math.cos(ga) * g.r, g.cz + math.sin(ga) * g.r
        local gy = g.h + math.sin(timeNow * 1.4 + g.phase) * 1.4
        world.setPosition(g.id, gx, gy, gz)
        world.setRotation(g.id, 0.0, math.deg(math.atan(-math.sin(ga) * g.spd, math.cos(ga) * g.spd)), g.spd > 0 and -12.0 or 12.0)
    end

    crashSmokeAccum = crashSmokeAccum + dt
    if crashSmokeAccum >= 0.24 then
        crashSmokeAccum = 0.0
        local py = world.sampleTerrain(74.0, -118.0)
        world.spawnParticles(74.0, py + 1.8, -118.0, 0.6, 2.6, 0.4, 0.26, 0.26, 0.28, 2, 0.42, 2.2, false, -0.6)
    end

    local nearestFire, nearestFireDist = nil, 9999.0
    for _, cf in ipairs(campfires) do
        local flicker = 2.55 + math.sin(timeNow * 11.0 + cf.x) * 0.36 + math.cos(timeNow * 17.0) * 0.20
        world.setPointLight(cf.lightId, cf.x, cf.y + 1.35, cf.z, 1.0, 0.54, 0.16, 24.0, flicker)
        local baseS = MESHY_LOADED.campfire and 0.17 or 0.46
        local s = baseS + (baseS * 0.18) * math.sin(timeNow * 9.5 + cf.z)
        world.setScale(cf.flameId, s, s * 1.22, s)
        cf.fxAccum = cf.fxAccum + dt
        if cf.fxAccum >= 0.14 then
            cf.fxAccum = 0.0
            world.spawnParticles(cf.x, cf.y + 0.42, cf.z, 0.0, 2.1, 0.0, 1.0, 0.52, 0.12, 2, 0.08, 0.85, true, -1.2)
        end
        local df = dist2D(player.x, player.z, cf.x, cf.z)
        if df < nearestFireDist then nearestFireDist = df; nearestFire = cf end
    end
    if nearestFire then
        audio.setAmbientLoop(5, AMB.campfire, 0.92, nearestFire.x, nearestFire.y + 0.6, nearestFire.z, 26.0)
    end

    for _, rp in ipairs(radioPickups) do
        if not rp.collected then
            world.setRotation(rp.id, 0.0, timeNow * 70.0, 0.0)
            world.setPosition(rp.id, rp.x, rp.y + 0.65 + math.sin(timeNow * 3.0) * 0.18, rp.z)
            world.setRotation(rp.ringId, 0.0, -timeNow * 45.0, 0.0)
        end
    end

    -- Dynamic Layered Wilderness Soundscape Mixer (Day Birds <-> Night Crickets <-> Rainstorm!)
    local dayFactor = clamp(math.sin(((worldClock.hour - 5.5) / 13.5) * math.pi), 0.0, 1.0)
    local rainQuiet = 1.0 - worldClock.rain * 0.72
    audio.setAmbientLoop(0, AMB.forestWind,   0.26 + worldClock.rain * 0.25 + clamp(player.y / 50.0, 0.0, 0.35))
    audio.setAmbientLoop(1, AMB.birds,        dayFactor * 0.38 * rainQuiet)
    audio.setAmbientLoop(2, AMB.nightCricket, (1.0 - dayFactor) * 0.44 * rainQuiet)
    audio.setAmbientLoop(3, AMB.rainHeavy,    worldClock.rain * 0.68)

    audio.setListener(cam.x, cam.y, cam.z, cam.tx - cam.x, cam.tz - cam.z)

    -- Escape Menu toggle
    if keyJustPressed("Escape") then
        if showCraftMenu then showCraftMenu = false
        elseif gameState == "PLAYING" then gameState = "PAUSED"
        elseif gameState == "PAUSED" then gameState = "PLAYING"
        end
    end

    if gameState ~= "PLAYING" then
        if gameState == "MENU" then
            local orbit = timeNow * 0.16
            local cx = player.x + math.sin(orbit) * 14.5
            local cz = player.z + math.cos(orbit) * 14.5
            local cy = math.max(world.sampleTerrain(cx, cz) + 2.2, player.y + 4.8)
            camera.setPosition(cx, cy, cz)
            camera.setTarget(player.x + 3.0, player.y + 1.4, player.z - 3.0)
            camera.setFov(58.0)
            sage.setTitle("Wild Frontier: Echo Island — Organic 3D Survival (SAGE Engine v1.6.0)")
        end
        return
    end

    -- Real-Time Day/Night Cycle, Storms & Periodic 3D Wildlife/Thunder Audio!
    worldClock.hour = worldClock.hour + worldClock.daySpeed * dt
    if worldClock.hour >= 24.0 then
        worldClock.hour = worldClock.hour - 24.0
        worldClock.day = worldClock.day + 1
        notify("Survived to Day " .. worldClock.day .. " on Echo Island!", 4.0)
    end
    world.setTimeOfDay(worldClock.hour)

    worldClock.weatherTimer = worldClock.weatherTimer - dt
    if worldClock.weatherTimer <= 0.0 then
        worldClock.weatherTimer = 55.0 + math.random() * 45.0
        local r = math.random()
        worldClock.weatherIdx = (r < 0.56) and 1 or ((r < 0.82) and 2 or 3)
        notify("Weather shifting -> " .. weatherPresets[worldClock.weatherIdx].name, 3.0)
    end
    local wp = weatherPresets[worldClock.weatherIdx]
    worldClock.rain = lerp(worldClock.rain, wp.rain, 0.35 * dt)
    worldClock.wetness = lerp(worldClock.wetness, wp.wet, 0.28 * dt)
    world.setWeather(worldClock.rain, worldClock.wetness)

    -- Atmospheric 3D Wolf Howls & Night Owls + Rolling Storm Thunder!
    local isNight = (worldClock.hour < 6.2 or worldClock.hour > 19.2)
    worldClock.ambientCallTimer = worldClock.ambientCallTimer - dt
    if worldClock.ambientCallTimer <= 0.0 then
        worldClock.ambientCallTimer = isNight and (16.0 + math.random() * 14.0) or (26.0 + math.random() * 18.0)
        if isNight then
            if math.random() < 0.55 then
                local w = wildlife[math.random(1, 5)]
                if w and w.alive then
                    audio.playSfx3D(SFX.wolfHowl, w.x, w.y + 1.2, w.z, 0.72, 14.0, 160.0, 0.96 + math.random() * 0.08)
                end
            else
                audio.playSfx3D(SFX.owl, player.x + (math.random() - 0.5) * 45.0, player.y + 8.0, player.z + (math.random() - 0.5) * 45.0, 0.55, 8.0, 95.0, 1.0)
            end
        end
    end
    if worldClock.rain > 0.65 then
        worldClock.thunderTimer = worldClock.thunderTimer - dt
        if worldClock.thunderTimer <= 0.0 then
            worldClock.thunderTimer = 14.0 + math.random() * 18.0
            playSfxSafe(SFX.thunder, 0.75, 0.95)
        end
    end

    -- Hotkeys (V = Toggle 1st/3rd Person!)
    if keyJustPressed("V") then toggleCameraPerspective() end
    if keyJustPressed("T") then
        timeSkipIdx = (timeSkipIdx % #timeSkips) + 1
        worldClock.hour = timeSkips[timeSkipIdx].hour
        world.setTimeOfDay(worldClock.hour)
        notify("Time -> " .. timeSkips[timeSkipIdx].name, 2.2)
    end
    if keyJustPressed("G") then
        worldClock.weatherIdx = (worldClock.weatherIdx % #weatherPresets) + 1
        worldClock.weatherTimer = 65.0
        notify("Weather -> " .. weatherPresets[worldClock.weatherIdx].name, 2.2)
    end
    if keyJustPressed("C") and not player.firstPerson then
        camPresetIdx = (camPresetIdx % #camPresets) + 1
        notify("3rd-Person Camera -> " .. camPresets[camPresetIdx].name, 1.8)
    end
    if keyJustPressed("K") then showCraftMenu = not showCraftMenu end
    if keyJustPressed("H") then showHud = not showHud end
    if keyJustPressed("F6") then saveGame() end
    if keyJustPressed("L") then
        player.torchOn = not player.torchOn
        world.setVisible("player_torch", player.torchOn)
        if not player.torchOn then world.removePointLight("player_torch_light") end
        playSfxSafe(SFX.woodChop1, 0.55, 1.35)
        notify(player.torchOn and "Fire Torch LIT (+Warmth & Night Light!)" or "Fire Torch extinguished", 2.0)
    end
    if keyJustPressed("1") then useBerry() end
    if keyJustPressed("2") then useSteak() end
    if keyJustPressed("3") then craftCampfire() end
    if keyJustPressed("4") then useBandage() end

    -- Combat Cooldowns & Input
    player.attackCooldown = math.max(0.0, player.attackCooldown - dt)
    player.heavyCooldown = math.max(0.0, player.heavyCooldown - dt)
    player.hurtCooldown = math.max(0.0, player.hurtCooldown - dt)

    local mouseLeft = input.isMouseDown("Left")
    local mouseLeftPressed = mouseLeft and (not mouseLeftLatch)
    mouseLeftLatch = mouseLeft

    if (keyJustPressed("F") or (mouseLeftPressed and not showCraftMenu)) and player.attackCooldown <= 0.0 and player.stamina >= 10.0 then
        player.attackCooldown = 0.42
        player.stamina = math.max(0.0, player.stamina - 10.0)
        performStrike(false)
    end
    if keyJustPressed("Q") and player.heavyCooldown <= 0.0 and player.stamina >= 22.0 then
        player.heavyCooldown = 1.15
        player.stamina = math.max(0.0, player.stamina - 22.0)
        performStrike(true)
    end

    -- -------------------------------------------------------------------------
    -- Survivor Movement (True FPS Strafe in 1st-Person, Classic 3rd-Person Turn!)
    -- -------------------------------------------------------------------------
    local moveF, strafeR, turnIn = 0.0, 0.0, 0.0
    if input.isKeyDown("W") or input.isKeyDown("Up") then moveF = moveF + 1.0 end
    if input.isKeyDown("S") or input.isKeyDown("Down") then moveF = moveF - 1.0 end

    if player.firstPerson then
        -- 1st-Person FPS controls: A/D strafe cleanly, Right-Mouse Drag & Left/Right arrows look around!
        if input.isKeyDown("D") then strafeR = strafeR + 1.0 end
        if input.isKeyDown("A") then strafeR = strafeR - 1.0 end
        if input.isKeyDown("Left") then turnIn = turnIn + 1.0 end
        if input.isKeyDown("Right") then turnIn = turnIn - 1.0 end
        player.yaw = player.yaw + turnIn * 135.0 * dt
        if input.isMouseDown("Right") then
            local mdx, mdy = input.getMouseDelta()
            player.yaw = player.yaw - mdx * 0.28
            player.pitch = clamp(player.pitch - mdy * 0.24, -72.0, 72.0)
        end
    else
        if input.isKeyDown("A") or input.isKeyDown("Left") then turnIn = turnIn + 1.0 end
        if input.isKeyDown("D") or input.isKeyDown("Right") then turnIn = turnIn - 1.0 end
        player.yaw = player.yaw + turnIn * 145.0 * dt
    end

    local prad = degToRad(player.yaw)
    local fwdX, fwdZ = math.sin(prad), math.cos(prad)
    local rightX, rightZ = -math.cos(prad), math.sin(prad)

    local moveMag = math.sqrt(moveF * moveF + strafeR * strafeR)
    local moveDirX, moveDirZ = 0.0, 0.0
    if moveMag > 0.01 then
        moveDirX = (fwdX * moveF + rightX * strafeR) / moveMag
        moveDirZ = (fwdZ * moveF + rightZ * strafeR) / moveMag
    end

    local wantsSprint = (input.isKeyDown("LeftShift") or input.isKeyDown("RightShift")) and (moveMag > 0.1) and (player.stamina > 6.0) and (not player.swimming)
    if wantsSprint then
        player.stamina = math.max(0.0, player.stamina - 22.0 * dt)
    else
        local regen = (player.hunger > 25.0 and player.thirst > 25.0) and 18.0 or 8.0
        player.stamina = math.min(100.0, player.stamina + regen * dt)
    end

    local moveSpd = player.swimming and 4.3 or (wantsSprint and 9.4 or 4.8)
    player.vx = lerp(player.vx, moveDirX * moveSpd, 11.0 * dt)
    player.vz = lerp(player.vz, moveDirZ * moveSpd, 11.0 * dt)
    player.x = clamp(player.x + player.vx * dt, -265.0, 265.0)
    player.z = clamp(player.z + player.vz * dt, -265.0, 265.0)

    local terrainY = world.sampleTerrain(player.x, player.z)
    local inW, waveY, _, wDepth = world.sampleWater(player.x, player.z)
    player.nearWater = inW and (wDepth > 0.03)
    player.swimming = inW and (wDepth >= 0.72)

    if player.swimming then
        player.y = lerp(player.y, waveY - 0.85, 6.0 * dt)
        player.vy = 0.0
        player.grounded = true
        player.splashAccum = player.splashAccum + dt
        if moveMag > 0.1 and player.splashAccum >= 0.34 then
            player.splashAccum = 0.0
            world.spawnParticles(player.x, waveY + 0.05, player.z, 0.0, 2.1, 0.0, 0.84, 0.96, 1.0, 4, 0.16, 0.45, false, 11.0)
            playSfxSafe(SFX.splash, 0.62, 1.0)
        end
    else
        if player.grounded and input.isKeyDown("Space") and player.stamina >= 8.0 then
            player.vy = 9.0
            player.grounded = false
            player.stamina = math.max(0.0, player.stamina - 8.0)
            playSfxSafe(player.y < 3.4 and SFX.stepSand or SFX.stepGrass1, 0.65, 0.88)
        end
        if not player.grounded then
            player.vy = player.vy - 24.0 * dt
            player.y = player.y + player.vy * dt
            if player.y <= terrainY then
                player.y = terrainY
                player.vy = 0.0
                player.grounded = true
                playSfxSafe(player.y < 3.4 and SFX.stepSand or SFX.stepRock, 0.78, 0.85)
            end
        else
            if player.y > terrainY + 0.45 then
                player.grounded = false
            else
                player.y = lerp(player.y, terrainY, 18.0 * dt)
            end
        end
    end

    local hitObs, rx, rz, rvx, rvz = physics.resolveCircleXZ(player.x, player.y, player.z, 0.58, player.vx, player.vz, 0.10)
    if hitObs then player.x, player.z, player.vx, player.vz = rx, rz, rvx, rvz end

    -- Interactive 76,000 GPU Grass Blades part dynamically around boots!
    world.setGrassInteractPos(player.x, player.y, player.z)

    -- 6-Clip GPU Skeletal Crossfade Animation (Idle/Walk/Run/Jump/Swim/Attack) + Surface Footsteps!
    local pSpd = math.sqrt(player.vx * player.vx + player.vz * player.vz)
    if player.attackAnimTimer > 0.0 then
        player.attackAnimTimer = math.max(0.0, player.attackAnimTimer - dt)
    end
    local desiredAnim, animRate, blendSec = "Idle", 1.0, 0.20
    if player.attackAnimTimer > 0.0 and MESHY_LOADED.survivor then
        desiredAnim, animRate, blendSec = "Attack", 1.40, 0.08
    elseif player.swimming then
        desiredAnim, animRate, blendSec = "Swim", pSpd > 0.5 and 1.15 or 0.55, 0.24
    elseif not player.grounded then
        desiredAnim, animRate, blendSec = "Jump", 1.0, 0.10
    elseif pSpd > 6.2 then
        desiredAnim, animRate, blendSec = "Run", clamp(pSpd / 7.2, 0.85, 1.38), 0.18
    elseif pSpd > 0.35 then
        desiredAnim, animRate, blendSec = "Walk", clamp(pSpd / 3.6, 0.55, 1.40), 0.16
    end
    if desiredAnim ~= player.currentAnim then
        player.currentAnim = desiredAnim
        world.playAnimation("player_survivor", desiredAnim, desiredAnim ~= "Attack", animRate, blendSec)
    elseif desiredAnim ~= "Attack" then
        world.setAnimationSpeed("player_survivor", animRate)
    end

    if player.grounded and not player.swimming and pSpd > 0.45 then
        player.headBobPhase = player.headBobPhase + dt * (desiredAnim == "Run" and 13.5 or 8.2)
        local stepInt = (desiredAnim == "Run") and (0.37 / animRate) or (0.56 / animRate)
        player.stepAccum = player.stepAccum + dt
        if player.stepAccum >= stepInt then
            player.stepAccum = 0.0
            player.stepToggle = not player.stepToggle
            local stepVol = (desiredAnim == "Run") and 0.68 or 0.44
            if player.nearWater then
                playSfxSafe(SFX.splash, stepVol, 1.15)
            elseif player.y < 3.4 then
                playSfxSafe(SFX.stepSand, stepVol, 1.0)
            elseif player.y > 19.0 then
                playSfxSafe(SFX.stepRock, stepVol, 1.0)
            else
                playSfxSafe(player.stepToggle and SFX.stepGrass1 or SFX.stepGrass2, stepVol, 1.0)
            end
        end
    else
        player.stepAccum = 0.20
    end

    player.roll = lerp(player.roll, (player.grounded and not player.swimming and not player.firstPerson) and (-turnIn * clamp(pSpd / 9.5, 0.0, 1.0) * 8.5) or 0.0, 10.0 * dt)
    world.setPosition("player_survivor", player.x, player.y, player.z)
    world.setRotation("player_survivor", 0.0, player.yaw, player.roll)

    -- -------------------------------------------------------------------------
    -- Camera Update: Seamless 1st-Person (FPS + Tool Viewmodel) OR 3rd-Person!
    -- -------------------------------------------------------------------------
    if player.firstPerson then
        local pitchRad = degToRad(player.pitch)
        local cp = math.cos(pitchRad)
        local lookX, lookY, lookZ = fwdX * cp, math.sin(pitchRad), fwdZ * cp
        local bobY = (player.grounded and pSpd > 0.45) and (math.sin(player.headBobPhase) * (wantsSprint and 0.065 or 0.036)) or 0.0
        local bobX = (player.grounded and pSpd > 0.45) and (math.cos(player.headBobPhase * 0.5) * 0.025) or 0.0

        cam.x = player.x + rightX * bobX
        cam.y = player.y + (player.swimming and 1.25 or 1.66) + bobY
        cam.z = player.z + rightZ * bobX
        cam.tx = cam.x + lookX * 12.0
        cam.ty = cam.y + lookY * 12.0
        cam.tz = cam.z + lookZ * 12.0
        cam.fov = lerp(cam.fov, wantsSprint and 78.0 or 72.0, 8.0 * dt)
        camera.setPosition(cam.x, cam.y, cam.z)
        camera.setTarget(cam.tx, cam.ty, cam.tz)
        camera.setFov(cam.fov)

        -- Position animated First-Person Axe/Spear Viewmodel in lower-right foreground!
        local swingT = math.sin(player.swingAnim * math.pi)
        local toolFwd = 0.68 + swingT * 0.32
        local toolRight = 0.34 - swingT * 0.14
        local toolDown = -0.26 - swingT * 0.10 + bobY * 0.65
        local hx = cam.x + lookX * toolFwd + rightX * toolRight
        local hy = cam.y + lookY * toolFwd + toolDown
        local hz = cam.z + lookZ * toolFwd + rightZ * toolRight
        local toolPitch = player.pitch - 18.0 + swingT * 62.0
        world.setPosition("fp_tool_handle", hx, hy, hz)
        world.setRotation("fp_tool_handle", toolPitch, player.yaw, -12.0)
        world.setPosition("fp_tool_head", hx + lookX * 0.10, hy + 0.24 - swingT * 0.14, hz + lookZ * 0.10)
        world.setRotation("fp_tool_head", toolPitch, player.yaw, -12.0)

        -- Torch in lower-left foreground when lit in 1st-Person!
        if player.torchOn then
            local tx = cam.x + lookX * 0.64 - rightX * 0.36
            local ty = cam.y + lookY * 0.64 - 0.18 + bobY * 0.5
            local tz = cam.z + lookZ * 0.64 - rightZ * 0.36
            world.setPosition("player_torch", tx, ty, tz)
            world.setPointLight("player_torch_light", tx, ty + 0.15, tz, 1.0, 0.58, 0.18, 20.0, 2.5 + math.sin(timeNow * 14.0) * 0.28)
        end
    else
        -- Smooth 3rd-Person Exploration / Over-Shoulder Camera
        if input.isMouseDown("Right") then
            local mdx, mdy = input.getMouseDelta()
            cam.orbitYaw = cam.orbitYaw - mdx * 0.35
            cam.orbitPitch = clamp(cam.orbitPitch + mdy * 0.22, -10.0, 42.0)
        elseif pSpd > 0.6 then
            cam.orbitYaw = lerp(cam.orbitYaw, 0.0, 1.8 * dt)
            cam.orbitPitch = lerp(cam.orbitPitch, 8.0, 1.5 * dt)
        end
        local cp = camPresets[camPresetIdx]
        local cyaw = degToRad(player.yaw + cam.orbitYaw)
        local cfx, cfz = math.sin(cyaw), math.cos(cyaw)
        local celev = math.sin(degToRad(cam.orbitPitch)) * cp.dist * 0.75
        local desiredCamX = player.x - cfx * cp.dist
        local desiredCamZ = player.z - cfz * cp.dist
        local camGround = world.sampleTerrain(desiredCamX, desiredCamZ)
        local desiredCamY = math.max(camGround + 0.95, player.y + cp.height + celev)

        cam.x = lerp(cam.x, desiredCamX, 7.5 * dt)
        cam.y = lerp(cam.y, desiredCamY, 7.5 * dt)
        cam.z = lerp(cam.z, desiredCamZ, 7.5 * dt)
        cam.tx = lerp(cam.tx, player.x + cfx * 1.8, 14.0 * dt)
        cam.ty = lerp(cam.ty, player.y + 1.45, 14.0 * dt)
        cam.tz = lerp(cam.tz, player.z + cfz * 1.8, 14.0 * dt)
        cam.fov = lerp(cam.fov, wantsSprint and (cp.fov + 5.0) or cp.fov, 6.0 * dt)
        camera.setPosition(cam.x, cam.y, cam.z)
        camera.setTarget(cam.tx, cam.ty, cam.tz)
        camera.setFov(cam.fov)

        if player.torchOn then
            local tx = player.x + fwdX * 0.55 - rightX * 0.42
            local ty = player.y + 1.42
            local tz = player.z + fwdZ * 0.55 - rightZ * 0.42
            world.setPosition("player_torch", tx, ty, tz)
            world.setPointLight("player_torch_light", tx, ty + 0.25, tz, 1.0, 0.58, 0.18, 18.0, 2.4 + math.sin(timeNow * 14.0) * 0.28)
            player.torchFxAccum = player.torchFxAccum + dt
            if player.torchFxAccum >= 0.12 then
                player.torchFxAccum = 0.0
                world.spawnParticles(tx, ty + 0.12, tz, 0.0, 1.6, 0.0, 1.0, 0.62, 0.14, 1, 0.08, 0.55, true, -0.8)
            end
        end
    end

    -- -------------------------------------------------------------------------
    -- Survival Vitals Update
    -- -------------------------------------------------------------------------
    player.nearFire = (nearestFireDist < 8.8)
    player.nearShelter = false
    for _, sh in ipairs(shelters) do
        if dist2D(player.x, player.z, sh.x, sh.z) < 8.5 then player.nearShelter = true; break end
    end
    if dist2D(player.x, player.z, 68.0, 28.0) < 9.0 or dist2D(player.x, player.z, -84.0, 122.0) < 9.0 then
        player.nearShelter = true
    end

    player.hunger = math.max(0.0, player.hunger - (wantsSprint and 0.65 or 0.26) * dt)
    player.thirst = math.max(0.0, player.thirst - (wantsSprint and 0.80 or 0.34) * dt)

    local coldRate = 0.0
    if isNight then coldRate = coldRate + 1.45 end
    if worldClock.rain > 0.2 then coldRate = coldRate + worldClock.rain * 1.85 end
    if player.y > 26.0 then coldRate = coldRate + 1.25 end -- High alpine cold!
    if player.swimming then coldRate = coldRate + 2.20 end
    if player.nearFire then
        player.warmth = math.min(100.0, player.warmth + 11.0 * dt)
    elseif player.nearShelter or player.torchOn then
        player.warmth = math.min(100.0, player.warmth + (player.nearShelter and 4.5 or 1.4) * dt)
    else
        player.warmth = clamp(player.warmth - coldRate * dt + (not isNight and worldClock.rain < 0.2 and 0.9 * dt or 0.0), 0.0, 100.0)
    end

    if player.hunger <= 0.1 or player.thirst <= 0.1 or player.warmth <= 0.1 then
        player.health = math.max(0.0, player.health - 2.2 * dt)
        if player.health <= 0.0 then gameState = "DEAD" end
    elseif player.hunger > 70.0 and player.thirst > 70.0 and player.warmth > 60.0 then
        player.health = math.min(100.0, player.health + 1.2 * dt)
    end

    -- -------------------------------------------------------------------------
    -- Context-Sensitive [E] Interaction
    -- -------------------------------------------------------------------------
    promptText = ""
    promptAction = nil

    if dist2D(player.x, player.z, rescue.towerX, rescue.towerZ + 7.5) < 6.5 and not rescue.active then
        if inv.hasBeacon then
            promptText = "[E] Activate Emergency Rescue Beacon & Summon Helicopter!"
            promptAction = function()
                rescue.active = true
                rescue.timer = 0.0
                world.setVisible("rescue_heli", true)
                world.setColor("radio_tower_ring", 0.20, 1.0, 0.45)
                playSfxSafe(SFX.craftOk, 0.95, 1.2)
                notify("RESCUE SIGNAL SENT! Rescue Helicopter arriving in 14s!", 5.0)
            end
        else
            promptText = string.format("Echo Ridge Radio Tower — Need Rescue Beacon (Craft in [K]: %d/3 Radio Parts)", inv.radioParts)
        end
    end

    if not promptAction then
        for _, rp in ipairs(radioPickups) do
            if not rp.collected and dist2D(player.x, player.z, rp.x, rp.z) < 4.6 then
                promptText = "[E] Salvage " .. rp.name .. " (Radio Part)"
                promptAction = function()
                    rp.collected = true
                    inv.radioParts = inv.radioParts + 1
                    world.setVisible(rp.id, false)
                    world.setVisible(rp.ringId, false)
                    world.removePointLight(rp.lightId)
                    playSfxSafe(SFX.craftOk, 0.92, 1.15)
                    notify(string.format("Salvaged %s! (%d / 3 Radio Parts found)", rp.name, inv.radioParts), 4.0)
                end
                break
            end
        end
    end

    if not promptAction and player.nearFire and inv.rawMeat > 0 then
        promptText = "[E] Roast Raw Meat over Campfire (+1 Cooked Steak)"
        promptAction = function()
            inv.rawMeat = inv.rawMeat - 1
            inv.cookedMeat = inv.cookedMeat + 1
            playSfxSafe(SFX.craftOk, 0.78, 1.0)
            notify("Cooked juicy Steak over the campfire! Press [2] to eat.", 2.6)
        end
    end

    if not promptAction and player.nearWater and player.thirst < 96.0 then
        promptText = "[E] Drink Fresh Water (+40 Thirst)"
        promptAction = function()
            player.thirst = math.min(100.0, player.thirst + 40.0)
            playSfxSafe(SFX.splash, 0.72, 1.2)
            notify("Drank fresh water (+40 Thirst)", 2.2)
        end
    end

    if not promptAction then
        for _, n in ipairs(harvestNodes) do
            if not n.depleted and dist2D(player.x, player.z, n.x, n.z) < 4.4 then
                promptText = "[E] or [F] Harvest " .. n.label
                promptAction = function() performStrike(false) end
                break
            end
        end
    end

    if keyJustPressed("E") and promptAction then promptAction() end

    for _, n in ipairs(harvestNodes) do
        if n.depleted then
            n.respawnTimer = n.respawnTimer - dt
            if n.respawnTimer <= 0.0 then
                n.depleted = false; n.hp = n.maxHp
                world.setVisible(n.id, true)
            end
        end
    end

    -- -------------------------------------------------------------------------
    -- Wildlife & Raider AI (Look-At IK + Fire Fear + Euphoria Ragdolls!)
    -- -------------------------------------------------------------------------
    for _, w in ipairs(wildlife) do
        if not w.alive then
            w.respawnTimer = w.respawnTimer - dt
            if w.respawnTimer <= 0.0 then
                w.alive = true; w.hp = w.maxHp
                w.x, w.z = w.homeX, w.homeZ; w.y = world.sampleTerrain(w.x, w.z)
                world.setVisible(w.id, true); world.setPosition(w.id, w.x, w.y, w.z)
            end
        else
            local ragAct = world.getRagdollState(w.id)
            if ragAct then
                local rx, ry, rz = world.getPosition(w.id)
                w.x, w.y, w.z = rx, ry, rz
            else
                w.attackCd = math.max(0.0, w.attackCd - dt)
                if w.attackAnimTimer and w.attackAnimTimer > 0.0 then
                    w.attackAnimTimer = math.max(0.0, w.attackAnimTimer - dt)
                end
                local dP = dist2D(w.x, w.z, player.x, player.z)
                local scaredByFire = false
                if w.kind == "wolf" then
                    for _, cf in ipairs(campfires) do
                        if dist2D(w.x, w.z, cf.x, cf.z) < 10.5 then scaredByFire = true; break end
                    end
                end
                local spd = 2.6
                if w.kind == "wolf" then
                    local aggroDist = isNight and 30.0 or 20.0
                    if scaredByFire then
                        w.state = "FLEE"
                        local invD = dP > 0.1 and (1.0 / dP) or 1.0
                        w.targetX, w.targetZ = w.x + (w.x - player.x) * invD * 18.0, w.z + (w.z - player.z) * invD * 18.0
                        spd = 6.8
                    elseif dP < aggroDist and not player.swimming then
                        w.state = "CHASE"
                        w.targetX, w.targetZ = player.x, player.z
                        spd = isNight and 6.6 or 5.6
                        if dP < 2.4 and w.attackCd <= 0.0 then
                            w.attackCd = 1.4
                            w.attackAnimTimer = 0.68
                            if MESHY_LOADED.wolf then
                                w.anim = "Attack"
                                world.playAnimation(w.id, "Attack", false, 1.25, 0.08)
                            end
                            damagePlayer(isNight and 16.0 or 12.0, "Timber Wolf")
                        end
                    else
                        w.state = "PATROL"
                        w.timer = w.timer - dt
                        if w.timer <= 0.0 or dist2D(w.x, w.z, w.targetX, w.targetZ) < 1.5 then
                            w.timer = 3.5 + math.random() * 2.5
                            w.targetX = w.homeX + (math.random() - 0.5) * 30.0
                            w.targetZ = w.homeZ + (math.random() - 0.5) * 30.0
                        end
                        spd = 2.8
                    end
                else
                    if dP < 15.0 then
                        w.state = "FLEE"
                        w.pauseTimer = 0.0
                        local invD = dP > 0.1 and (1.0 / dP) or 1.0
                        w.targetX = clamp(w.x + (w.x - player.x) * invD * 24.0, -210.0, 210.0)
                        w.targetZ = clamp(w.z + (w.z - player.z) * invD * 24.0, -210.0, 210.0)
                        spd = 8.4
                    else
                        w.state = "GRAZE"
                        if (w.pauseTimer or 0.0) > 0.0 then
                            w.pauseTimer = math.max(0.0, w.pauseTimer - dt)
                            spd = 0.0
                        else
                            w.timer = w.timer - dt
                            if w.timer <= 0.0 or dist2D(w.x, w.z, w.targetX, w.targetZ) < 1.5 then
                                w.pauseTimer = 4.2 + math.random() * 3.5
                                w.timer = 5.0 + math.random() * 3.0
                                w.targetX = w.homeX + (math.random() - 0.5) * 28.0
                                w.targetZ = w.homeZ + (math.random() - 0.5) * 28.0
                                spd = 0.0
                            else
                                spd = 2.4
                            end
                        end
                    end
                end
                local dx, dz = w.targetX - w.x, w.targetZ - w.z
                local dtarg = math.sqrt(dx * dx + dz * dz)
                local moved = false
                if spd > 0.05 and dtarg > 0.5 then
                    local ux, uz = dx / dtarg, dz / dtarg
                    local nx, nz = clamp(w.x + ux * spd * dt, -220.0, 220.0), clamp(w.z + uz * spd * dt, -220.0, 220.0)
                    if world.sampleTerrain(nx, nz) > 0.5 then
                        w.x, w.z = nx, nz
                        local targetYaw = math.deg(math.atan(ux, uz))
                        w.yaw = lerpAngle(w.yaw, targetYaw, (w.state == "CHASE" or w.state == "FLEE") and (9.5 * dt) or (5.5 * dt))
                        moved = true
                    else
                        w.timer = 0.0
                    end
                end
                if w.kind == "wolf" and MESHY_LOADED.wolf then
                    local wAnim, wRate, wBlend = "Idle", 1.0, 0.22
                    if (w.attackAnimTimer or 0.0) > 0.0 then
                        wAnim, wRate, wBlend = "Attack", 1.25, 0.08
                    elseif moved and (w.state == "CHASE" or w.state == "FLEE") then
                        wAnim, wRate, wBlend = "Run", clamp(spd / 5.0, 1.0, 1.55), 0.14
                    elseif moved then
                        wAnim, wRate, wBlend = "Walk", 1.15, 0.18
                    end
                    if wAnim ~= w.anim then
                        w.anim = wAnim
                        world.playAnimation(w.id, wAnim, wAnim ~= "Attack", wRate, wBlend)
                    elseif wAnim ~= "Attack" then
                        world.setAnimationSpeed(w.id, wRate)
                    end
                elseif w.kind == "deer" and MESHY_LOADED.deer then
                    local dAnim, dRate, dBlend = "Idle", 1.0, 0.26
                    if moved and w.state == "FLEE" then
                        dAnim, dRate, dBlend = "Run", 1.35, 0.12
                    elseif moved then
                        dAnim, dRate, dBlend = "Walk", 1.05, 0.20
                    elseif dP > 22.0 then
                        dAnim, dRate, dBlend = "Graze", 0.90, 0.35
                    else
                        dAnim, dRate, dBlend = "Idle", 1.05, 0.25
                    end
                    if dAnim ~= w.anim then
                        w.anim = dAnim
                        world.playAnimation(w.id, dAnim, true, dRate, dBlend)
                    else
                        world.setAnimationSpeed(w.id, dRate)
                    end
                end
                w.y = world.sampleTerrain(w.x, w.z)
                -- Smooth terrain-slope pitch alignment so wolves & deer climb/descend hills naturally!
                local wrad = degToRad(w.yaw)
                local wfx, wfz = math.sin(wrad), math.cos(wrad)
                local hFront = world.sampleTerrain(w.x + wfx * 0.70, w.z + wfz * 0.70)
                local hBack  = world.sampleTerrain(w.x - wfx * 0.70, w.z - wfz * 0.70)
                local targetPitch = clamp(-math.deg(math.atan(hFront - hBack, 1.4)), -24.0, 24.0)
                w.pitch = lerp(w.pitch or 0.0, targetPitch, 8.0 * dt)

                local bob = (w.kind == "wolf" and not MESHY_LOADED.wolf) and (0.75 + math.sin(timeNow * 6.0 + w.homeX) * 0.12) or 0.0
                world.setPosition(w.id, w.x, w.y + bob, w.z)
                world.setRotation(w.id, w.pitch, w.yaw, 0.0)
                local allowLookAt = (dP < 24.0) and (w.anim ~= "Graze") and (w.state ~= "FLEE")
                if allowLookAt then world.setLookAt(w.id, player.x, player.y + 1.4, player.z, 0.72) else world.clearLookAt(w.id) end
            end
        end
    end

    for _, r in ipairs(raiders) do
        if not r.alive then
            local ragAct = world.getRagdollState(r.id)
            if not ragAct then world.setVisible(r.id, false) end
            r.respawnTimer = r.respawnTimer - dt
            if r.respawnTimer <= 0.0 then
                r.alive = true; r.hp = r.maxHp
                r.x, r.z = r.homeX, r.homeZ; r.y = world.sampleTerrain(r.x, r.z)
                world.setVisible(r.id, true); world.setPosition(r.id, r.x, r.y, r.z)
            end
        else
            local ragAct = world.getRagdollState(r.id)
            if ragAct then
                local rx, ry, rz = world.getPosition(r.id)
                r.x, r.y, r.z = rx, ry, rz
                r.ragdolling = true
            else
                r.ragdolling = false
                r.attackCd = math.max(0.0, r.attackCd - dt)
                if r.attackAnimTimer and r.attackAnimTimer > 0.0 then
                    r.attackAnimTimer = math.max(0.0, r.attackAnimTimer - dt)
                end
                local dP = dist2D(r.x, r.z, player.x, player.z)
                local desiredA, spd = "Walk", 2.6
                if dP < 26.0 and not player.swimming then
                    r.state = "CHASE"; r.targetX, r.targetZ = player.x, player.z
                    spd = 5.4; desiredA = "Run"
                    if dP < 2.4 and r.attackCd <= 0.0 then
                        r.attackCd = 1.35
                        if MESHY_LOADED.raider then
                            r.attackAnimTimer = 0.58
                            r.anim = "Attack"
                            world.playAnimation(r.id, "Attack", false, 1.30, 0.08)
                        end
                        damagePlayer(15.0, "Forest Raider")
                    end
                else
                    r.state = "PATROL"
                    r.timer = r.timer - dt
                    if r.timer <= 0.0 or dist2D(r.x, r.z, r.targetX, r.targetZ) < 1.2 then
                        r.timer = 3.0 + math.random() * 2.0
                        r.targetX = r.homeX + (math.random() - 0.5) * 18.0
                        r.targetZ = r.homeZ + (math.random() - 0.5) * 18.0
                    end
                    desiredA = "Walk"; spd = 2.4
                end
                local dx, dz = r.targetX - r.x, r.targetZ - r.z
                local dtarg = math.sqrt(dx * dx + dz * dz)
                if dtarg > 0.6 then
                    local ux, uz = dx / dtarg, dz / dtarg
                    local nx, nz = clamp(r.x + ux * spd * dt, -220.0, 220.0), clamp(r.z + uz * spd * dt, -220.0, 220.0)
                    if world.sampleTerrain(nx, nz) > 0.5 then
                        r.x, r.z = nx, nz
                        r.yaw = lerpAngle(r.yaw, math.deg(math.atan(ux, uz)), 8.0 * dt)
                    end
                else
                    desiredA = "Idle"
                end
                if (r.attackAnimTimer or 0.0) > 0.0 and MESHY_LOADED.raider then
                    desiredA = "Attack"
                end
                if desiredA ~= r.anim then
                    r.anim = desiredA
                    world.playAnimation(r.id, desiredA, desiredA ~= "Attack", desiredA == "Run" and 1.05 or (desiredA == "Attack" and 1.30 or 0.92), desiredA == "Attack" and 0.08 or 0.18)
                end
                r.y = world.sampleTerrain(r.x, r.z)
                world.setPosition(r.id, r.x, r.y, r.z)
                world.setRotation(r.id, 0.0, r.yaw, 0.0)
                if dP < 28.0 then world.setLookAt(r.id, player.x, player.y + 1.45, player.z, 0.92) else world.clearLookAt(r.id) end
            end
        end
    end

    -- Rescue Helicopter Descent
    if rescue.active then
        rescue.timer = rescue.timer + dt
        local t = clamp(rescue.timer / rescue.duration, 0.0, 1.0)
        local smoothT = t * t * (3.0 - 2.0 * t)
        local landY = world.sampleTerrain(rescue.towerX, rescue.towerZ + 8.0) + 5.5
        rescue.heliX = lerp(-260.0, rescue.towerX, smoothT)
        rescue.heliY = lerp(78.0, landY, smoothT)
        rescue.heliZ = lerp(270.0, rescue.towerZ + 8.0, smoothT)
        world.setPosition("rescue_heli", rescue.heliX, rescue.heliY, rescue.heliZ)
        world.setRotation("rescue_heli", (1.0 - smoothT) * 12.0, 138.0, 0.0)
        world.setPointLight("rescue_searchlight", rescue.heliX, rescue.heliY - 3.0, rescue.heliZ, 0.95, 0.98, 1.0, 48.0, 4.2)
        audio.setSpatialEmitter(0, 2, rescue.heliX, rescue.heliY, rescue.heliZ, 180.0, 0.95)
        if rescue.timer >= rescue.duration then
            gameState = "VICTORY"
            playSfxSafe(SFX.craftOk, 0.95, 1.1)
            saveGame()
        end
    end

    titleAccum = titleAccum + dt
    if titleAccum >= 0.30 then
        titleAccum = 0.0
        sage.setTitle(string.format(
            "Wild Frontier [%s] | Day %d (%02d:%02d) | %s | HP %d%% | Radio %d/3",
            player.firstPerson and "1ST-PERSON" or "3RD-PERSON",
            worldClock.day, math.floor(worldClock.hour), math.floor((worldClock.hour % 1.0) * 60),
            getRegionName(player.x, player.y, player.z), math.floor(player.health), inv.radioParts
        ))
    end
end

-- -----------------------------------------------------------------------------
-- Survival HUD, FPS Crosshair, Crafting Menu & Real Heightmap Contour Radar
-- -----------------------------------------------------------------------------
function onRenderUI()
    local vpW, vpH = ui.getViewportSize()

    if gameState == "MENU" then
        if ui.beginCenteredWindow("WILD FRONTIER: ECHO ISLAND (SAGE ENGINE v1.5.0)", 510, 415) then
            ui.setFontScale(1.42)
            ui.textCentered("WILD FRONTIER: ECHO ISLAND")
            ui.setFontScale(1.0)
            ui.textCentered("Organic 3D Open-World Survival Showcase  |  1st & 3rd Person")
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("START NEW SURVIVAL EXPEDITION", 490, 44) then
                playSfxSafe(SFX.craftOk, 0.85, 1.05)
                gameState = "PLAYING"
                notify("Stranded on Echo Island! Press [V] for 1st/3rd Person, [K] Craft, [E] Gather!", 5.0)
            end
            if hasSavedGame() then
                ui.spacing()
                if ui.button("CONTINUE SAVED EXPEDITION", 490, 36) then
                    loadSavedGame()
                    playSfxSafe(SFX.craftOk, 0.85, 1.0)
                    gameState = "PLAYING"
                end
            end
            ui.spacing(); ui.separator(); ui.spacing()
            ui.text("AAA Engine Showcase:")
            ui.text("  * Organic 3D Sculpted Island (Crescent Cove, Emerald Lake, 70m Snow Peak)")
            ui.text("  * 76,000 Interactive Wind Grass Blades, Tiered 3-CSM, SSR & Volumetric God-Rays")
            ui.text("  * Real Royalty-Free Wilderness Ambience & 3D Spatial CC0 Foley Audio")
            ui.text("  * Press [V] Anytime to Switch Seamless 1st-Person <-> 3rd-Person Camera!")
            ui.text("  * Drop Meshy .glb Models into assets/models/survival/ + Press [F5] to Swap!")
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("Quit to Desktop", 490, 32) then sage.quit() end
        end
        ui.endWindow()
        return
    end

    if gameState == "PAUSED" then
        if ui.beginCenteredWindow("SURVIVAL PAUSED", 380, 320) then
            ui.setFontScale(1.25)
            ui.textCentered("EXPEDITION PAUSED")
            ui.setFontScale(1.0)
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("Resume Survival (Esc)", 360, 38) then gameState = "PLAYING" end
            ui.spacing()
            if ui.button(player.firstPerson and "Switch to 3rd-Person View (V)" or "Switch to 1st-Person View (V)", 360, 34) then
                toggleCameraPerspective()
                gameState = "PLAYING"
            end
            ui.spacing()
            if ui.button("Save Expedition (F6)", 360, 34) then saveGame() end
            ui.spacing()
            if ui.button("Return to Main Menu", 360, 34) then gameState = "MENU" end
            ui.spacing()
            if ui.button("Quit to Desktop", 360, 34) then sage.quit() end
        end
        ui.endWindow()
        return
    end

    if gameState == "DEAD" then
        if ui.beginCenteredWindow("YOU PERISHED", 420, 215) then
            ui.setFontScale(1.35)
            ui.textCentered("LOST TO THE WILDERNESS")
            ui.setFontScale(1.0)
            ui.spacing()
            ui.textCentered(string.format("Survived %d Day(s) | Radio Parts: %d/3", worldClock.day, inv.radioParts))
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("Respawn at Crash Cove Beach", 400, 40) then
                player.x, player.z = 62.0, -108.0
                player.y = world.sampleTerrain(player.x, player.z)
                player.health, player.hunger, player.thirst, player.warmth, player.stamina = 100.0, 85.0, 85.0, 90.0, 100.0
                world.setPosition("player_survivor", player.x, player.y, player.z)
                gameState = "PLAYING"
                playSfxSafe(SFX.craftOk, 0.85, 1.0)
            end
        end
        ui.endWindow()
        return
    end

    if gameState == "VICTORY" then
        if ui.beginCenteredWindow("RESCUED FROM ECHO ISLAND!", 460, 255) then
            ui.setFontScale(1.35)
            ui.textCentered("EXTRACTION COMPLETE!")
            ui.setFontScale(1.0)
            ui.spacing()
            ui.textCentered("The Rescue Helicopter extracted you from Echo Ridge!")
            ui.textCentered(string.format("Days Survived: %d | Wolves: %d | Raiders: %d | Structures: %d",
                worldClock.day, stats.wolvesDefeated, stats.raidersDefeated, stats.structuresBuilt))
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("Keep Exploring Echo Island", 440, 38) then rescue.active = false; gameState = "PLAYING" end
            ui.spacing()
            if ui.button("Return to Main Menu", 440, 32) then gameState = "MENU" end
        end
        ui.endWindow()
        return
    end

    -- Minimalist FPS Crosshair in 1st-Person Mode!
    if player.firstPerson and not showCraftMenu then
        if ui.beginWindow("##fps_reticle", vpW * 0.5 - 16, vpH * 0.5 - 16, 32, 32, true) then
            ui.drawCircle(16, 16, 2.5, 1.0, 1.0, 1.0, 0.90, true)
            ui.drawCircle(16, 16, 7.0, 0.9, 0.96, 1.0, 0.45, false)
        end
        ui.endWindow()
    end

    if statusBannerTimer > 0.0 and #statusBanner > 0 then
        if ui.beginWindow("##banner", vpW * 0.5 - 270, 14, 540, 42, true) then
            ui.textCentered(statusBanner)
        end
        ui.endWindow()
    end
    if #promptText > 0 then
        if ui.beginWindow("##prompt", vpW * 0.5 - 240, vpH - 120, 480, 42, true) then
            ui.textCentered(promptText)
        end
        ui.endWindow()
    end

    if showCraftMenu then
        if ui.beginCenteredWindow("SURVIVAL CRAFTING & BACKPACK (Press K to Close)", 480, 405) then
            ui.text(string.format("Materials:  Wood: %d  |  Stone: %d  |  Iron: %d  |  Radio Parts: %d / 3",
                inv.wood, inv.stone, inv.iron, inv.radioParts))
            ui.text(string.format("Supplies:   Berries: %d  |  Raw Meat: %d  |  Steak: %d  |  Bandages: %d",
                inv.berries, inv.rawMeat, inv.cookedMeat, inv.bandages))
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("Build Campfire (3 Wood, 2 Stone) — Warmth + Cooking + 3D Fire", 460, 32) then craftCampfire() end
            ui.spacing()
            if ui.button("Build Wooden Shelter Cabin (8 Wood, 4 Stone) — Storm Protection", 460, 32) then craftShelter() end
            ui.spacing()
            if ui.button(inv.hasSpear and "Iron Survival Spear [CRAFTED - 2x Power]" or "Craft Iron Survival Spear (3 Wood, 3 Iron) — 2x Combat/Harvest", 460, 32) then craftSpear() end
            ui.spacing()
            if ui.button("Craft Herbal Bandage (1 Wood, 2 Berries) — +38 Health", 460, 32) then craftBandage() end
            ui.spacing()
            if ui.button(inv.hasBeacon and "Emergency Rescue Beacon [READY - Bring to NW Radio Tower!]" or "Craft Rescue Beacon (3 Radio Parts, 4 Iron) — Summon Rescue!", 460, 34) then craftRescueBeacon() end
            ui.spacing(); ui.separator(); ui.spacing()
            if ui.button("Eat Berry [1]", 148, 30) then useBerry() end
            ui.sameLine()
            if ui.button("Eat Steak [2]", 148, 30) then useSteak() end
            ui.sameLine()
            if ui.button("Bandage [4]", 148, 30) then useBandage() end
        end
        ui.endWindow()
    end

    if showHud then
        local vitW, vitH = 304, 226
        if ui.beginWindow("Vitals (V = 1st/3rd Person | K = Craft)", 14, vpH - vitH - 14, vitW, vitH, true) then
            ui.text(string.format("Day %d  %02d:%02d  |  %s",
                worldClock.day, math.floor(worldClock.hour), math.floor((worldClock.hour % 1.0) * 60),
                weatherPresets[worldClock.weatherIdx].name))
            ui.text(getRegionName(player.x, player.y, player.z) .. (player.firstPerson and " [1ST]" or " [3RD]"))
            ui.progressBar(player.health / 100.0, string.format("HEALTH  %d%%", math.floor(player.health)))
            ui.progressBar(player.hunger / 100.0, string.format("HUNGER  %d%%", math.floor(player.hunger)))
            ui.progressBar(player.thirst / 100.0, string.format("THIRST  %d%%", math.floor(player.thirst)))
            local warmTag = player.nearFire and " [FIRE]" or (player.nearShelter and " [SHELTER]" or (player.torchOn and " [TORCH]" or ""))
            ui.progressBar(player.warmth / 100.0, string.format("WARMTH  %d%%%s", math.floor(player.warmth), warmTag))
            ui.progressBar(player.stamina / 100.0, string.format("STAMINA %d%%", math.floor(player.stamina)))
            ui.separator()
            if rescue.active then
                ui.text(string.format("RESCUE CHOPPER ARRIVING: %.1fs!", math.max(0.0, rescue.duration - rescue.timer)))
            elseif inv.hasBeacon then
                ui.text("Goal: Activate Echo Ridge Radio Tower (NW)!")
            else
                ui.text(string.format("Goal: Find Radio Parts (%d/3) & Craft Beacon", inv.radioParts))
            end
        end
        ui.endWindow()

        local bagW, bagH = 320, 142
        if ui.beginWindow("Pack & Controls (V = View | F6 = Save)", vpW - bagW - 14, vpH - bagH - 14, bagW, bagH, true) then
            ui.text(string.format("Wood: %d | Stone: %d | Iron: %d | Radio: %d/3", inv.wood, inv.stone, inv.iron, inv.radioParts))
            ui.text(string.format("Berries: %d | Raw: %d | Steak: %d | Bandage: %d", inv.berries, inv.rawMeat, inv.cookedMeat, inv.bandages))
            ui.separator()
            ui.text("[V] 1st/3rd Person | [E] Gather/Cook | [Q] Slam")
            ui.text("[K] Craft | [L] Torch | [T] Time | [G] Storm")
        end
        ui.endWindow()

        -- REAL ORGANIC HEIGHTMAP CONTOUR RADAR (Zero 4-Block Squares!)
        local mapSize = 160
        local mapW, mapH = mapSize + 18, mapSize + 36
        if ui.beginWindow("Echo Island Map (N ^)", vpW - mapW - 14, 14, mapW, mapH, true) then
            local ox, oy = 9, 27
            local cellPx = mapSize / MINIMAP_N
            for _, c in ipairs(minimapCells) do
                ui.drawRect(ox + c.ix * cellPx, oy + c.iz * cellPx, cellPx + 0.6, cellPx + 0.6, c.r, c.g, c.b, 0.96, true)
            end

            local half = mapSize * 0.5
            local function toMap(wx, wz)
                return ox + half + clamp(wx / 270.0, -1.0, 1.0) * half,
                       oy + half - clamp(wz / 270.0, -1.0, 1.0) * half
            end

            for _, cf in ipairs(campfires) do
                local cx, cy = toMap(cf.x, cf.z)
                ui.drawCircle(cx, cy, 3.0, 1.0, 0.54, 0.14, 1.0, true)
            end
            for _, rp in ipairs(radioPickups) do
                if not rp.collected then
                    local rx, ry = toMap(rp.x, rp.z)
                    ui.drawCircle(rx, ry, 4.2, 1.0, 0.86, 0.18, 0.95, false)
                    ui.drawCircle(rx, ry, 2.4, 1.0, 0.86, 0.18, 1.0, true)
                end
            end
            local rtx, rty = toMap(rescue.towerX, rescue.towerZ)
            ui.drawCircle(rtx, rty, 5.2, 0.22, 0.95, 1.0, 0.95, false)

            for _, w in ipairs(wildlife) do
                if w.alive then
                    local wx, wy = toMap(w.x, w.z)
                    if w.kind == "wolf" then
                        ui.drawCircle(wx, wy, 2.3, 1.0, 0.28, 0.22, 0.95, true)
                    else
                        ui.drawCircle(wx, wy, 1.9, 0.35, 0.95, 0.45, 0.90, true)
                    end
                end
            end
            for _, r in ipairs(raiders) do
                if r.alive then
                    local rx, ry = toMap(r.x, r.z)
                    ui.drawCircle(rx, ry, 2.7, 0.95, 0.12, 0.36, 0.98, true)
                end
            end

            local px, py = toMap(player.x, player.z)
            local dirRad = degToRad(player.yaw)
            ui.drawLine(px, py, px + math.sin(dirRad) * 9.0, py - math.cos(dirRad) * 9.0, 1.0, 1.0, 1.0, 0.95, 1.8)
            ui.drawCircle(px, py, 3.6, 1.0, 1.0, 1.0, 1.0, true)
            ui.drawRect(ox, oy, mapSize, mapSize, 0.45, 0.78, 0.62, 0.85, false, 2.0)
        end
        ui.endWindow()
    end
end

function onShutdown()
    audio.stopAllAmbientLoops()
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
    sage.log("Wild Frontier: Echo Island shutting down cleanly.")
end
