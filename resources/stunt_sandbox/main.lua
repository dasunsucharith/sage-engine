-- =============================================================================
-- SAGE Engine v1.5.0 Showcase: GPU Prop Instancing + 3-CSM + Euphoria Ragdoll + Spline AI
--   * Automatic GPU-Instanced Static Prop Batching + 3-Split Cascaded Shadows
--   * Euphoria-Style Active-Physics Ragdoll Blending & Procedural Head Look-At IK
--   * 10 Smart-Yielding Catmull-Rom Spline AI Cruisers + 10 Reactive AI Citizens
--   * Q = Kinetic Repulsor Blast | X = Radial Shockwave | F = Walk/Swim/Drive/Boat
-- =============================================================================
-- CONTROLS:
--   W / S (or Up / Down)   : Drive / Brake (or Walk Forward / Back on foot)
--   A / D (or Left / Right): Steer (or Turn on foot)
--   F                      : Exit / Enter Supercar (Walk & Swim as 3D Character!)
--   Space                  : Handbrake Drift (or Jump on foot)
--   Left Shift             : Nitro Boost (or Sprint on foot)
--   Q                      : Kinetic Repulsor Blast (Ragdolls NPCs & blasts Jolt props!)
--   X                      : Radial Physics Shockwave (blasts props & NPCs around you!)
--   Right-Mouse Drag       : Orbit 3D Camera
--   C                      : Cycle Camera (Far Chase -> Close Action -> Hood Cam)
--   B                      : Drop fresh 6-crate Jolt Physics pyramid ahead
--   T                      : Cycle Time of Day (Sunset -> Noon -> Starry Night -> Sunrise)
--   G                      : Cycle Dynamic Weather (Clear -> Drizzle -> Rainstorm + SSR)
--   H                      : Hide / Show Cockpit HUD
--   Esc                    : Pause Menu (Main Menu hides completely on Play!)
--   F1                     : Toggle Developer Dear ImGui Inspector & GPU Profiler
--   Tab                    : Toggle Free-Fly 3D Inspector Camera
--   F5 / Ctrl+S            : Live Hot-Reload (JSON scene + Lua scripts)
-- =============================================================================

local CONFIG = {
    crateModel    = "assets/models/crate.glb",
    coneModel     = "assets/models/traffic_cone.glb",
    ballModel     = "assets/models/stunt_ball.glb",
    droneModel    = "assets/models/hover_drone.glb",
    heliModel     = "assets/models/helicopter.glb",
    charModel     = "assets/models/character_rigged.glb",
    boatModel     = "assets/models/speedboat.glb",
    gullModel     = "assets/models/seagull.glb",
    jetModel      = "assets/models/jet_plane.glb",
    trafficModel  = "assets/models/ai_sedan.glb",
    maxSpeed      = 44.0,   -- Fast grand-highway speed for the 560m open world!
    boostSpeed    = 64.0,   -- Nitro top speed (~230 km/h)
    acceleration  = 33.0,
    brakeForce    = 46.0,
    reverseAccel  = 21.0,
    lowSpeedTurn  = 160.0,
    highSpeedTurn = 80.0,
    normalGrip    = 9.5,
    driftGrip     = 2.2,
    gravity       = 26.0,
    arenaHalfSize = 272.0,  -- Massive 560m x 560m coastal island!
}

local vehicles = {
    { name = "Lamborghini Aventador SVJ", model = "assets/models/aventador_svj.glb", isBike = false, soundProfile = 0 },
    { name = "Cyberpunk Superbike",       model = "assets/models/superbike.glb",     isBike = true,  soundProfile = 1 },
    { name = "SAGE Sports Coupe",         model = "assets/models/sports_car.glb",    isBike = false, soundProfile = 3 },
}
local selectedVehicleIdx = 1

local biomeSpawns = {
    { name = "Downtown Metro",    x =    0.0, z =   -8.0, yaw =    0.0 },
    { name = "Pine Highlands",    x =  148.0, z =  100.0, yaw =    0.0 },
    { name = "Palm Coast Marina", x =  148.0, z = -172.0, yaw =  135.0 },
    { name = "Red Rock Desert",   x = -148.0, z = -148.0, yaw = -135.0 },
}
local selectedBiomeIdx = 1

local timePresets = {
    { hour = 17.4, name = "Golden Sunset" },
    { hour = 12.5, name = "Bright Noon" },
    { hour = 22.5, name = "Starry Night" },
    { hour =  6.4, name = "Early Sunrise" },
}
local timePresetIdx = 1

local weatherPresets = {
    { name = "Clear Sky",      rain = 0.00, wet = 0.00 },
    { name = "Light Drizzle",  rain = 0.35, wet = 0.55 },
    { name = "Neon Rainstorm", rain = 0.95, wet = 1.00 },
}
local weatherPresetIdx = 1

local gameState = "MENU" -- "MENU" | "PLAYING" | "PAUSED"
local showHud = true
local bestScore = 0
local heliPos = { x = 145.0, z = 0.0 }

-- -----------------------------------------------------------------------------
-- Vehicle & Playable 3rd-Person Character State (Press F to Exit / Enter Car!)
-- -----------------------------------------------------------------------------
local car = {
    x = 0.0, y = 0.0, z = -8.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    yaw = 0.0, pitch = 0.0, roll = 0.0,
    steerSmooth = 0.0,
    grounded = true,
    airTime = 0.0,
    boostEnergy = 100.0,
    boostTimer = 0.0,
    isDrifting = false,
    isBoosting = false,
    inWater = false,
    waterDepth = 0.0,
    flooded = false,
    floodTimer = 0.0,
    lastDryX = 0.0,
    lastDryZ = -8.0,
    lastDryYaw = 0.0,
    fxAccum = 0.0,
    gear = 1,
    rpm = 0.12,
    suspOffset = 0.0,
    suspVel = 0.0,
    terrainRoll = 0.0,
}

local player = {
    mode = "CAR", -- "CAR" | "FOOT" | "BOAT"
    onFoot = false,
    x = 3.0, y = 0.0, z = -8.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    yaw = 0.0,
    roll = 0.0,
    grounded = true,
    swimming = false,
    moving = false,
    currentAnim = "Idle",
    splashAccum = 0.0,
    stepAccum = 0.0,
    dustAccum = 0.0,
}

-- Drivable Luxury Racing Speedboat moored cleanly next to the 32m Marina Pier!
local boat = {
    x = 185.0, y = -0.26, z = -182.0,
    vx = 0.0, vz = 0.0,
    yaw = 25.0, pitch = 0.0, roll = 0.0,
    speed = 0.0,
    wakeAccum = 0.0,
}

local seagulls = {
    { id = "gull_1", cx = 192.0, cz = -192.0, r = 24.0, h = 14.0, spd =  0.48, phase = 0.0 },
    { id = "gull_2", cx = 188.0, cz = -186.0, r = 32.0, h = 17.5, spd = -0.38, phase = 1.6 },
    { id = "gull_3", cx = 198.0, cz = -198.0, r = 18.0, h = 11.5, spd =  0.56, phase = 3.1 },
    { id = "gull_4", cx = 165.0, cz = -192.0, r = 28.0, h = 15.5, spd = -0.44, phase = 4.7 },
}

-- 10 Autonomous Reactive Rigged 3D Citizens (Patrol -> Look-At IK -> Flee -> Euphoria Ragdoll!)
local crowdNpcs = {
    { id = "npc_plaza_1",  x =  14.2, z = -18.0, ax =  14.2, az = -24.0, bx =  14.2, bz =  16.0, dir =  1, yaw =   0.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.6, ragdolling = false },
    { id = "npc_plaza_2",  x = -14.2, z =  12.0, ax = -14.2, az =  22.0, bx = -14.2, bz = -18.0, dir =  1, yaw = 180.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.4, ragdolling = false },
    { id = "npc_plaza_3",  x =  24.0, z =  14.2, ax =  16.0, az =  14.2, bx =  52.0, bz =  14.2, dir =  1, yaw =  90.0, state = "IDLE", anim = "Idle", timer = 2.0, speed = 2.5, ragdolling = false },
    { id = "npc_plaza_4",  x = -26.0, z = -14.2, ax = -52.0, az = -14.2, bx = -16.0, bz = -14.2, dir =  1, yaw =  90.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.5, ragdolling = false },
    { id = "npc_high_1",   x = 136.0, z =  82.0, ax = 136.0, az =  66.0, bx = 136.0, bz = 116.0, dir =  1, yaw =   0.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.7, ragdolling = false },
    { id = "npc_high_2",   x = 160.0, z = 104.0, ax = 160.0, az =  78.0, bx = 160.0, bz = 128.0, dir = -1, yaw = 180.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.4, ragdolling = false },
    { id = "npc_marina_1", x = 148.0, z =-164.0, ax = 134.0, az =-164.0, bx = 162.0, bz =-164.0, dir =  1, yaw =  90.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.3, ragdolling = false },
    { id = "npc_marina_2", x = 134.0, z =-176.0, ax = 122.0, az =-176.0, bx = 154.0, bz =-176.0, dir =  1, yaw =  90.0, state = "IDLE", anim = "Idle", timer = 1.6, speed = 2.4, ragdolling = false },
    { id = "npc_desert_1", x =-136.0, z =-142.0, ax =-136.0, az =-162.0, bx =-136.0, bz =-122.0, dir =  1, yaw =   0.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.5, ragdolling = false },
    { id = "npc_metro_1",  x = -76.0, z =  76.0, ax = -76.0, az =  52.0, bx = -76.0, bz = 106.0, dir =  1, yaw =   0.0, state = "WALK", anim = "Walk", timer = 0.0, speed = 2.6, ragdolling = false },
}

-- Closed-Loop Catmull-Rom Highway Splines (native C++ world.evalSpline!) for 10 Smart-Yielding AI Cruisers!
local function buildRoundedRingSpline(R)
    local c = R * 0.90
    return {
        { -c, 0.07,  R }, {  0, 0.07,  R }, {  c, 0.07,  R },
        {  R, 0.07,  c }, {  R, 0.07,  0 }, {  R, 0.07, -c },
        {  c, 0.07, -R }, {  0, 0.07, -R }, { -c, 0.07, -R },
        { -R, 0.07, -c }, { -R, 0.07,  0 }, { -R, 0.07,  c },
    }
end

local trafficSplines = {
    inner_cw  = buildRoundedRingSpline(67.2),
    mid_cw    = buildRoundedRingSpline(151.4),
    mid_ccw   = buildRoundedRingSpline(144.6),
    outer_cw  = buildRoundedRingSpline(239.6),
    outer_ccw = buildRoundedRingSpline(232.4),
}

local trafficCars = {
    { id = "ai_car_1",  spline = "inner_cw",  dir =  1, t = 0.05, maxSpeed = 14.5, curSpeed = 14.5, perim =  540.0, color = { 0.92, 0.22, 0.20 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_2",  spline = "inner_cw",  dir =  1, t = 0.55, maxSpeed = 14.0, curSpeed = 14.0, perim =  540.0, color = { 0.18, 0.78, 0.95 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_3",  spline = "mid_cw",    dir =  1, t = 0.00, maxSpeed = 18.5, curSpeed = 18.5, perim = 1210.0, color = { 0.90, 0.16, 0.18 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_4",  spline = "mid_ccw",   dir = -1, t = 0.34, maxSpeed = 17.0, curSpeed = 17.0, perim = 1156.0, color = { 0.14, 0.56, 0.96 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_5",  spline = "mid_cw",    dir =  1, t = 0.62, maxSpeed = 19.5, curSpeed = 19.5, perim = 1210.0, color = { 0.96, 0.96, 0.98 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_6",  spline = "mid_ccw",   dir = -1, t = 0.80, maxSpeed = 18.0, curSpeed = 18.0, perim = 1156.0, color = { 0.96, 0.54, 0.14 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_7",  spline = "outer_cw",  dir =  1, t = 0.08, maxSpeed = 22.5, curSpeed = 22.5, perim = 1916.0, color = { 0.16, 0.82, 0.42 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_8",  spline = "outer_ccw", dir = -1, t = 0.44, maxSpeed = 21.0, curSpeed = 21.0, perim = 1859.0, color = { 1.00, 0.72, 0.14 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_9",  spline = "outer_cw",  dir =  1, t = 0.72, maxSpeed = 23.0, curSpeed = 23.0, perim = 1916.0, color = { 0.72, 0.76, 0.84 }, x = 0, z = 0, yaw = 0, braking = false },
    { id = "ai_car_10", spline = "outer_ccw", dir = -1, t = 0.92, maxSpeed = 21.5, curSpeed = 21.5, perim = 1859.0, color = { 0.62, 0.24, 0.92 }, x = 0, z = 0, yaw = 0, braking = false },
}

local camPresets = {
    { name = "Far Chase",    dist = 10.8, height = 4.4, lookAhead = 5.5, fov = 60.0 },
    { name = "Close Action", dist =  7.4, height = 2.9, lookAhead = 6.5, fov = 66.0 },
    { name = "Hood Cam",     dist = -0.7, height = 1.48, lookAhead = 18.0, fov = 72.0 },
}
local camPresetIdx = 1
local cam = {
    x = 10.0, y = 5.5, z = -14.0,
    tx = 0.0, ty = 1.2, tz = -8.0,
    fov = 60.0,
    orbitYaw = 0.0,
    orbitPitch = 0.0,
}

-- Cross-Biome Delivery Destinations across the massive 560m x 560m Open World!
local deliverySpots = {
    { x =  -64.0, z =   64.0, label = "Downtown Metro Plaza" },
    { x =  148.0, z =   86.0, label = "Alpine Cabin Lodge" },
    { x =  148.0, z = -192.0, label = "Palm Marina Pier Entrance" },
    { x =  118.0, z = -168.0, label = "Coastal Ferris Wheel Boardwalk" },
    { x = -148.0, z = -148.0, label = "Red Rock Cactus Outpost" },
    { x =    0.0, z =  236.0, label = "North Coastal Ring Highway" },
    { x =  236.0, z =    0.0, label = "East Oceanfront Boulevard" },
    { x =    0.0, z = -236.0, label = "South Beach Highway" },
    { x = -236.0, z =    0.0, label = "West Canyon Highway" },
    { x =  -82.0, z =  198.0, label = "North Neon Gas Station" },
    { x = -192.0, z =  -90.0, label = "Canyon Service Station" },
    { x =  148.0, z =  192.0, label = "Pine Wind Ridge Overlook" },
}

local keyLatch = { T = false, G = false, C = false, X = false, Q = false, B = false, R = false, H = false, F = false, Escape = false }

local crates = {}
local nextCrateSeq = 0
local nextBallSeq = 0
local impactSoundCooldown = 0.0

local activeDeliveryIdx = 1
local deliveriesDone = 0
local score = 0
local stuntJumps = 0
local cratesSmashed = 0
local deliveryTimer = 0.0
local titleUpdateAccum = 0.0

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

local function degToRad(deg)
    return deg * (math.pi / 180.0)
end

local function keyJustPressed(name)
    local down = input.isKeyDown(name)
    local triggered = down and (not keyLatch[name])
    keyLatch[name] = down
    return triggered
end

local function getCurrentBiomeName(x, z)
    if x < 0.0 and z >= 0.0 then
        return "Downtown Metro"
    elseif x >= 0.0 and z >= 0.0 then
        return "Pine Highlands"
    elseif x >= 0.0 and z < 0.0 then
        return "Palm Coast Marina"
    else
        return "Red Rock Desert"
    end
end

local function applyWeatherPreset(idx)
    weatherPresetIdx = idx
    local wp = weatherPresets[weatherPresetIdx] or weatherPresets[1]
    world.setWeather(wp.rain, wp.wet)
end

local function updateBestScore()
    if score > bestScore then
        bestScore = score
        sage.saveData("stunt_sandbox_best", tostring(bestScore))
    end
end

local function respawnCarAtBiome(idx)
    local sp = biomeSpawns[idx] or biomeSpawns[1]
    player.mode = "CAR"
    player.onFoot = false
    world.setVisible("player_char", false)

    car.x, car.y, car.z = sp.x, 0.0, sp.z
    car.vx, car.vy, car.vz = 0.0, 0.0, 0.0
    car.yaw, car.pitch, car.roll = sp.yaw, 0.0, 0.0
    car.grounded = true
    car.airTime = 0.0
    car.inWater = false
    car.waterDepth = 0.0
    car.flooded = false
    car.floodTimer = 0.0
    car.lastDryX, car.lastDryZ, car.lastDryYaw = sp.x, sp.z, sp.yaw
    car.boostEnergy = 100.0
    cam.orbitYaw, cam.orbitPitch = 0.0, 0.0
    world.setPosition("player_car", car.x, car.y, car.z)
    world.setRotation("player_car", 0.0, car.yaw, 0.0)
end

local function applySelectedVehicle()
    local v = vehicles[selectedVehicleIdx]
    world.spawnModel("player_car", v.model, car.x, car.y, car.z)
    world.setRotation("player_car", car.pitch, car.yaw, car.roll)
    audio.setVehicleProfile(v.soundProfile or 0)
end

-- Maps a 1D perimeter progress along a square highway ring (±R) into (x, z, yawDeg)
local function sampleSquareLoop(ringR, laneOff, dir, distAlong)
    local R = ringR + laneOff
    local side = R * 2.0
    local perim = side * 4.0
    local p = distAlong % perim
    if p < 0.0 then p = p + perim end

    if p < side then
        -- North side: West -> East (+X)
        local x = -R + p
        local z = R
        return x, z, (dir > 0) and 90.0 or -90.0
    elseif p < side * 2.0 then
        -- East side: North -> South (-Z)
        local x = R
        local z = R - (p - side)
        return x, z, (dir > 0) and 180.0 or 0.0
    elseif p < side * 3.0 then
        -- South side: East -> West (-X)
        local x = R - (p - side * 2.0)
        local z = -R
        return x, z, (dir > 0) and -90.0 or 90.0
    else
        -- West side: South -> North (+Z)
        local x = -R
        local z = -R + (p - side * 3.0)
        return x, z, (dir > 0) and 0.0 or 180.0
    end
end

-- -----------------------------------------------------------------------------
-- Dynamic Props, AI Traffic & Physics Setup
-- -----------------------------------------------------------------------------
local function spawnCrate(x, y, z)
    nextCrateSeq = nextCrateSeq + 1
    local id = "phys_crate_" .. nextCrateSeq
    world.spawnModel(id, CONFIG.crateModel, x, y, z)
    physics.addDynamicBox(id, x, y, z, 0.64, 0.64, 0.64, 18.0, 0.32, 0.62)
    table.insert(crates, { id = id, smashed = false })
end

local function spawnCratePyramid(cx, cz, yawDeg)
    local rad = degToRad(yawDeg or 0.0)
    local rightX, rightZ = math.cos(rad), -math.sin(rad)
    local step = 1.32
    for i = -1, 1 do
        spawnCrate(cx + rightX * (i * step), 0.66, cz + rightZ * (i * step))
    end
    for _, off in ipairs({ -0.5, 0.5 }) do
        spawnCrate(cx + rightX * (off * step), 1.96, cz + rightZ * (off * step))
    end
    spawnCrate(cx, 3.26, cz)
end

local function spawnDynamicPropsAndPhysics()
    -- Dashed yellow road markings along both 540m grand expressways (ZERO Z-fighting!)
    local dashId = 0
    for d = -262, 262, 12 do
        local ad = math.abs(d)
        if ad > 10 and math.abs(ad - 36.0) > 7 and math.abs(ad - 64.0) > 8 and math.abs(ad - 148.0) > 8 and math.abs(ad - 196.0) > 8 and math.abs(ad - 236.0) > 9 then
            dashId = dashId + 1
            local idNS = "dash_ns_" .. dashId
            world.spawnPlane(idNS, 0.0, 0.06, d)
            world.setScale(idNS, 0.38, 1.0, 4.8)
            world.setColor(idNS, 0.96, 0.78, 0.16)

            local idEW = "dash_ew_" .. dashId
            world.spawnPlane(idEW, d, 0.09, 0.0)
            world.setScale(idEW, 4.8, 1.0, 0.38)
            world.setColor(idEW, 0.96, 0.78, 0.16)
        end
    end

    -- Player Supercar (.glb) + Kinematic Jolt Hull
    applySelectedVehicle()
    physics.addKinematicBox("player_car", car.x, car.y + 0.65, car.z, 1.05, 0.62, 2.25)

    -- Playable 3rd-Person Rigged & Skinned Hero (.glb with 22-Bone Armature: Idle, Walk, Run, Swim!)
    world.spawnModel("player_char", CONFIG.charModel, car.x + 2.5, car.y, car.z)
    world.playAnimation("player_char", "Idle", true, 1.0)
    player.currentAnim = "Idle"
    world.setVisible("player_char", false)

    -- Spawn 6 Autonomous Reactive Rigged Citizens across all 4 Biomes!
    for _, npc in ipairs(crowdNpcs) do
        local ny = world.sampleTerrain(npc.x, npc.z)
        world.spawnModel(npc.id, CONFIG.charModel, npc.x, ny, npc.z)
        world.setRotation(npc.id, 0.0, npc.yaw, 0.0)
        world.playAnimation(npc.id, npc.anim, true, npc.anim == "Walk" and 0.95 or 0.90, 0.0)
    end

    -- Drivable Luxury Racing Speedboat moored next to the 32m Marina Pier!
    world.spawnModel("player_boat", CONFIG.boatModel, boat.x, boat.y, boat.z)
    world.setRotation("player_boat", 0.0, boat.yaw, 0.0)
    world.playAnimation("player_boat", "", true, 0.35)

    -- 4 Animated Flapping Coastal Seagulls soaring above the Marina Lagoon + Supersonic Patrol Jet!
    for _, g in ipairs(seagulls) do
        world.spawnModel(g.id, CONFIG.gullModel, g.cx + g.r, g.h, g.cz)
        world.setScale(g.id, 1.25, 1.25, 1.25)
        world.playAnimation(g.id, "", true, 1.35)
    end
    world.spawnModel("sky_jet", CONFIG.jetModel, -300.0, 78.0, 120.0)
    world.setScale("sky_jet", 1.4, 1.4, 1.4)

    -- 10 Autonomous Catmull-Rom Spline AI Traffic Cruisers (.glb)
    for _, tc in ipairs(trafficCars) do
        local splinePts = trafficSplines[tc.spline]
        local tx, ty, tz, _, _, _, tyaw = world.evalSpline(splinePts, tc.t, true)
        if tc.dir < 0 then tyaw = tyaw + 180.0 end
        tc.x, tc.z, tc.yaw = tx, tz, tyaw
        world.spawnModel(tc.id, CONFIG.trafficModel, tx, ty, tz)
        world.setRotation(tc.id, 0.0, tyaw, 0.0)
        world.setColor(tc.id, tc.color[1], tc.color[2], tc.color[3])
    end

    -- Nitro exhaust flame + floating 3D golden nav arrow
    world.spawnTriangle("car_flame", car.x, car.y + 0.42, car.z)
    world.setScale("car_flame", 0.95, 0.65, 2.5)
    world.setColor("car_flame", 0.15, 0.88, 1.0)
    world.setEmissive("car_flame", true)
    world.setVisible("car_flame", false)

    world.spawnTriangle("nav_arrow", car.x, car.y + 2.6, car.z)
    world.setScale("nav_arrow", 0.95, 0.6, 1.4)
    world.setColor("nav_arrow", 1.0, 0.86, 0.18)
    world.setEmissive("nav_arrow", true)

    -- Glowing 3D Delivery Beacon Ring
    local spot = deliverySpots[activeDeliveryIdx]
    world.spawnRing("delivery_ring", spot.x, 0.11, spot.z)
    world.setScale("delivery_ring", 7.5, 5.5, 7.5)
    world.setColor("delivery_ring", 1.0, 0.82, 0.14)
    world.setEmissive("delivery_ring", true)

    -- Patrolling Animated Hover Drones + NEW v0.9 Animated Patrol Helicopter!
    world.spawnModel("drone_1", CONFIG.droneModel, 18.0, 9.0, 0.0)
    world.playAnimation("drone_1", "", true, 1.2)
    world.spawnModel("drone_2", CONFIG.droneModel, 148.0, 13.0, -148.0)
    world.playAnimation("drone_2", "", true, 1.4)

    world.spawnModel("patrol_heli", CONFIG.heliModel, 0.0, 34.0, 80.0)
    world.setScale("patrol_heli", 1.25, 1.25, 1.25)
    world.playAnimation("patrol_heli", "", true, 2.4)

    -- 4 Stacked 3-tier Jolt Physics Crate Pyramids (24 tumbling crates across the biomes!)
    crates = {}
    spawnCratePyramid(   0.0,   64.0,  0.0)
    spawnCratePyramid(  64.0,    0.0, 90.0)
    spawnCratePyramid( -64.0,    0.0, 90.0)
    spawnCratePyramid(   0.0,  -64.0,  0.0)

    -- 5 Giant Neon Rocket-League Jolt Physics Stunt Balls (including one floating in the Marina Lagoon!)
    nextBallSeq = 0
    local ballSpots = {
        { 0.0, 3.0, 12.0 }, { 64.0, 3.0, -64.0 }, { -64.0, 3.0, -64.0 },
        { 148.0, 3.0, 64.0 }, { 190.0, 2.0, -180.0 }, -- Splashes & bobs in the 3D Lagoon!
    }
    for _, bp in ipairs(ballSpots) do
        nextBallSeq = nextBallSeq + 1
        local id = "stunt_ball_" .. nextBallSeq
        world.spawnModel(id, CONFIG.ballModel, bp[1], bp[2], bp[3])
        physics.addDynamicSphere(id, bp[1], bp[2], bp[3], 1.25, 32.0, 0.72, 0.35)
    end

    -- 8 Knockable Capsule Traffic Cones around central hub
    local coneSpots = {
        { -5.0,  14.0 }, {  5.0,  14.0 }, { -5.0, -14.0 }, {  5.0, -14.0 },
        { 14.0,   5.0 }, { 14.0,  -5.0 }, { -14.0,  5.0 }, { -14.0, -5.0 },
    }
    for i, cp in ipairs(coneSpots) do
        local id = "cone_" .. i
        world.spawnModel(id, CONFIG.coneModel, cp[1], 0.50, cp[2])
        physics.addDynamicCapsule(id, cp[1], 0.50, cp[2], 0.24, 0.26, 6.0, 0.28, 0.65)
    end
end

-- -----------------------------------------------------------------------------
-- Autonomous AI Traffic, Crates & Mission Loop
-- -----------------------------------------------------------------------------
local function updateAiTraffic(dt)
    local obsX = player.onFoot and player.x or car.x
    local obsZ = player.onFoot and player.z or car.z

    for idx, tc in ipairs(trafficCars) do
        local splinePts = trafficSplines[tc.spline]
        local tx, ty, tz, tdx, _, tdz, tyaw = world.evalSpline(splinePts, tc.t, true)
        if tc.dir < 0 then
            tdx, tdz = -tdx, -tdz
            tyaw = tyaw + 180.0
        end

        -- Smart Yielding & Braking AI: slow down cleanly if player or another cruiser is directly ahead!
        local shouldBrake = false
        local pdx, pdz = obsX - tx, obsZ - tz
        local pAhead = pdx * tdx + pdz * tdz
        local pSide = math.abs(pdx * (-tdz) + pdz * tdx)
        if pAhead > 1.5 and pAhead < 17.5 and pSide < 4.2 then
            shouldBrake = true
        end
        if not shouldBrake then
            for j, other in ipairs(trafficCars) do
                if j ~= idx then
                    local odx, odz = other.x - tx, other.z - tz
                    local oAhead = odx * tdx + odz * tdz
                    local oSide = math.abs(odx * (-tdz) + odz * tdx)
                    if oAhead > 1.0 and oAhead < 15.0 and oSide < 3.4 then
                        shouldBrake = true
                        break
                    end
                end
            end
        end

        tc.braking = shouldBrake
        local targetSpd = shouldBrake and 0.0 or tc.maxSpeed
        tc.curSpeed = lerp(tc.curSpeed, targetSpd, (shouldBrake and 7.5 or 2.8) * dt)
        tc.t = (tc.t + (tc.curSpeed * tc.dir * dt) / tc.perim) % 1.0
        if tc.t < 0.0 then tc.t = tc.t + 1.0 end

        tx, ty, tz, tdx, _, tdz, tyaw = world.evalSpline(splinePts, tc.t, true)
        if tc.dir < 0 then
            tdx, tdz = -tdx, -tdz
            tyaw = tyaw + 180.0
        end
        local groundY = math.max(world.sampleTerrain(tx, tz), 0.07)
        tc.x, tc.z, tc.yaw = tx, tz, tyaw
        world.setPosition(tc.id, tx, groundY, tz)
        world.setRotation(tc.id, 0.0, tyaw, 0.0)

        -- Live headlight / red brake-light glow (GPU automatically selects nearest 16!)
        local trad = degToRad(tyaw)
        if tc.braking then
            world.setPointLight(
                tc.id .. "_head",
                tx - math.sin(trad) * 2.6, groundY + 0.95, tz - math.cos(trad) * 2.6,
                1.0, 0.14, 0.10, 14.0, 2.6
            )
        else
            world.setPointLight(
                tc.id .. "_head",
                tx + math.sin(trad) * 4.2, groundY + 1.15, tz + math.cos(trad) * 4.2,
                1.0, 0.94, 0.80, 18.0, 1.9
            )
        end

        -- Solid vehicle-vs-AI-traffic collision bounce + sparks!
        if not player.onFoot then
            local dx = car.x - tx
            local dz = car.z - tz
            local distSq = dx * dx + dz * dz
            if distSq < 3.4 * 3.4 and distSq > 0.001 then
                local dist = math.sqrt(distSq)
                local nx, nz = dx / dist, dz / dist
                local push = 3.4 - dist
                car.x = car.x + nx * push
                car.z = car.z + nz * push
                car.vx = car.vx * 0.55 + nx * 9.0
                car.vz = car.vz * 0.55 + nz * 9.0
                if impactSoundCooldown <= 0.0 then
                    audio.playImpact(0.75)
                    world.spawnParticles((car.x + tx) * 0.5, groundY + 0.9, (car.z + tz) * 0.5, 0.0, 4.5, 0.0, 1.0, 0.78, 0.28, 12, 0.18, 0.45, true, 14.0)
                    impactSoundCooldown = 0.28
                end
            end
        end
    end
end

local function checkPhysicsCrateSmashes(dt)
    if impactSoundCooldown > 0.0 then
        impactSoundCooldown = impactSoundCooldown - dt
    end
    for _, c in ipairs(crates) do
        if not c.smashed then
            local spd = physics.getSpeed(c.id)
            if spd > 2.2 then
                c.smashed = true
                cratesSmashed = cratesSmashed + 1
                score = score + 25
                updateBestScore()
                world.setColor(c.id, 1.0, 0.65, 0.42)
                local cx, cy, cz = world.getPosition(c.id)
                world.spawnParticles(cx, cy + 0.4, cz, 0.0, 5.5, 0.0, 1.0, 0.72, 0.24, 8, 0.20, 0.55, true, 16.0)
                if impactSoundCooldown <= 0.0 then
                    audio.playImpact(clamp(spd / 18.0, 0.35, 1.0))
                    impactSoundCooldown = 0.11
                end
            end
        end
    end
end

local function updateDeliveryMission(dt, timeNow, focusX, focusY, focusZ)
    deliveryTimer = deliveryTimer + dt
    local spot = deliverySpots[activeDeliveryIdx]

    local ringRadius = 7.2 + 0.85 * math.sin(timeNow * 4.5)
    world.setScale("delivery_ring", ringRadius, 5.5, ringRadius)
    world.setRotation("delivery_ring", 0.0, timeNow * 55.0, 0.0)

    local dx = spot.x - focusX
    local dz = spot.z - focusZ
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist > 0.05 then
        local bob = 0.22 * math.sin(timeNow * 5.0)
        world.setPosition("nav_arrow", focusX, focusY + (player.onFoot and 2.55 or 2.65) + bob, focusZ)
        world.setRotation("nav_arrow", 0.0, math.atan(dx, dz) * (180.0 / math.pi), 0.0)
    end

    if dist < 5.5 then
        deliveriesDone = deliveriesDone + 1
        local speedBonus = math.floor(math.max(0.0, 40.0 - deliveryTimer) * 10.0)
        local earned = 150 + speedBonus
        score = score + earned
        updateBestScore()
        car.boostEnergy = 100.0
        audio.playChime()

        sage.log(string.format(
            "DELIVERY #%d at %s! +%d pts | Score: %d",
            deliveriesDone, spot.label, earned, score
        ))

        local nextIdx = activeDeliveryIdx
        while nextIdx == activeDeliveryIdx do
            nextIdx = math.random(1, #deliverySpots)
        end
        activeDeliveryIdx = nextIdx
        deliveryTimer = 0.0

        local nextSpot = deliverySpots[activeDeliveryIdx]
        world.setPosition("delivery_ring", nextSpot.x, 0.11, nextSpot.z)
        sage.log("Next drop-off -> " .. nextSpot.label)
    end
end

-- -----------------------------------------------------------------------------
-- FiveM-Style Lifecycle Callbacks
-- -----------------------------------------------------------------------------
function onStart()
    math.randomseed(42)
    nextCrateSeq = 0
    local savedBest = tonumber(sage.loadData("stunt_sandbox_best"))
    if savedBest and savedBest > 0 then
        bestScore = math.floor(savedBest)
    end
    spawnDynamicPropsAndPhysics()
    applyWeatherPreset(1)
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)

    sage.log("===================================================================")
    sage.log("  SAGE Engine v1.5.0 — Prop Instancing + 3-CSM + Euphoria Ragdoll + Spline AI!")
    sage.log("  Press F = Walk/Swim/Drive | Q = Kinetic Blast | X = Shockwave | F1 = Editor")
    sage.log("  All-Time Best Score Loaded: " .. tostring(bestScore))
    sage.log("===================================================================")

    -- Register 4 Live 3D Spatial Ambient Audio Emitters across the 560m Open World!
    audio.setSpatialEmitter(0, 0, 192.0,  0.5, -192.0, 115.0, 0.85) -- Palm Marina Ocean Surf
    audio.setSpatialEmitter(1, 1,   0.0,  2.0,    0.0,  46.0, 0.65) -- Downtown Plaza Fountain
    audio.setSpatialEmitter(2, 2, 145.0, 32.0,    0.0, 145.0, 0.85) -- Patrol Helicopter Overhead
    audio.setSpatialEmitter(3, 3, 195.0, 12.0,  195.0, 130.0, 0.72) -- Alpine Highlands Mountain Wind
end

function onTick(dt)
    local timeNow = sage.getTime()

    -- Always animate drones, patrol helicopter, AI traffic & rotating lighthouse!
    local d1a = timeNow * 0.42
    world.setPosition("drone_1", math.cos(d1a) * 22.0, 9.2, math.sin(d1a) * 22.0)
    world.setRotation("drone_1", 0.0, math.deg(-d1a), 0.0)

    local d2a = -timeNow * 0.30 + 1.2
    world.setPosition("drone_2", 148.0 + math.cos(d2a) * 24.0, 13.0, -148.0 + math.sin(d2a) * 24.0)
    world.setRotation("drone_2", 0.0, math.deg(-d2a + math.pi), 0.0)

    -- Patrolling Animated Helicopter + Flapping Coastal Seagulls + Supersonic Sky Jet!
    local ha = timeNow * 0.14
    local hx = math.cos(ha) * 145.0
    local hz = math.sin(ha) * 145.0
    local hy = 32.0 + math.sin(timeNow * 0.8) * 2.5
    heliPos.x, heliPos.z = hx, hz
    world.setPosition("patrol_heli", hx, hy, hz)
    world.setRotation("patrol_heli", 6.0, math.deg(-ha), -10.0)
    -- Live 3D spatial helicopter rotor emitter pans across stereo speakers as it flies overhead!
    audio.setSpatialEmitter(2, 2, hx, hy, hz, 145.0, 0.85)
    audio.setListener(cam.x, cam.y, cam.z, cam.tx - cam.x, cam.tz - cam.z)

    for _, g in ipairs(seagulls) do
        local ga = timeNow * g.spd + g.phase
        local gx = g.cx + math.cos(ga) * g.r
        local gz = g.cz + math.sin(ga) * g.r
        local gy = g.h + math.sin(timeNow * 1.4 + g.phase) * 1.3
        local heading = math.deg(math.atan(-math.sin(ga) * g.spd, math.cos(ga) * g.spd))
        world.setPosition(g.id, gx, gy, gz)
        world.setRotation(g.id, 0.0, heading, g.spd > 0 and -12.0 or 12.0)
    end

    local jetAngle = timeNow * 0.22 + 0.8
    local jx = math.cos(jetAngle) * 320.0
    local jz = math.sin(jetAngle) * 320.0
    world.setPosition("sky_jet", jx, 76.0 + math.sin(timeNow * 0.45) * 6.0, jz)
    world.setRotation("sky_jet", 0.0, math.deg(math.atan(-math.sin(jetAngle), math.cos(jetAngle))), -22.0)

    -- Gently bob moored Speedboat on the 3D animated waves when unoccupied!
    if player.mode ~= "BOAT" then
        local _, bWaveY = world.sampleWater(boat.x, boat.z)
        boat.y = lerp(boat.y, bWaveY - 0.10, 6.0 * dt)
        world.setPosition("player_boat", boat.x, boat.y, boat.z)
        world.setRotation("player_boat", math.sin(timeNow * 1.9) * 2.8, boat.yaw, math.cos(timeNow * 1.5) * 3.5)
    end

    -- Sweeping Rotating Golden Lighthouse Beacon Light shimmering across the 72m Marina Lagoon!
    local lhAngle = timeNow * 0.85
    world.setPointLight(
        "lighthouse_beam",
        214.0 + math.cos(lhAngle) * 20.0, 8.5, -214.0 + math.sin(lhAngle) * 20.0,
        1.0, 0.86, 0.42, 42.0, 3.4
    )

    -- Update Interactive GPU Grass Bending around the active player car or character!
    local gx = player.onFoot and player.x or car.x
    local gy = player.onFoot and player.y or car.y
    local gz = player.onFoot and player.z or car.z
    world.setGrassInteractPos(gx, gy, gz)

    -- Autonomous Reactive GTA-Style Rigged Citizen AI (Patrol <-> Look-At IK <-> Flee <-> Euphoria Ragdoll!)
    local carSpdNow = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    local lookX = player.onFoot and player.x or car.x
    local lookY = (player.onFoot and player.y or car.y) + 1.45
    local lookZ = player.onFoot and player.z or car.z

    for _, npc in ipairs(crowdNpcs) do
        local ragActive = world.getRagdollState(npc.id)
        if ragActive then
            local rx, _, rz = world.getPosition(npc.id)
            npc.x, npc.z = rx, rz
            npc.ragdolling = true
            npc.state = "FLEE"
            npc.timer = 2.6
        else
            if npc.ragdolling then
                npc.ragdolling = false
                npc.state = "FLEE"
                npc.timer = 2.4
            end

            local dxCar = npc.x - car.x
            local dzCar = npc.z - car.z
            local distCar = math.sqrt(dxCar * dxCar + dzCar * dzCar)

            -- Euphoria Ragdoll Launch if hit directly by player vehicle at speed!
            if (not player.onFoot) and (carSpdNow > 4.0) and (distCar < 2.65) then
                local invD = distCar > 0.05 and (1.0 / distCar) or 1.0
                local lx = (dxCar * invD) * (carSpdNow * 0.55 + 5.0) + car.vx * 0.45
                local lz = (dzCar * invD) * (carSpdNow * 0.55 + 5.0) + car.vz * 0.45
                local ly = clamp(5.5 + carSpdNow * 0.24, 6.0, 14.5)
                world.triggerRagdoll(npc.id, lx, ly, lz, 2.5)
                world.clearLookAt(npc.id)
                npc.ragdolling = true
                if impactSoundCooldown <= 0.0 then
                    audio.playImpact(clamp(carSpdNow / 24.0, 0.45, 1.0))
                    world.spawnParticles(npc.x, world.sampleTerrain(npc.x, npc.z) + 0.9, npc.z, lx * 0.2, 4.5, lz * 0.2, 1.0, 0.85, 0.45, 10, 0.18, 0.45, true, 12.0)
                    impactSoundCooldown = 0.22
                end
            else
                local threatened = (not player.onFoot) and (carSpdNow > 5.0) and (distCar < 12.0)
                if threatened then
                    npc.state = "FLEE"
                    npc.timer = 1.8
                end

                local targetAnim = "Idle"
                local animSpeed = 0.92
                local blendTime = 0.20

                if npc.state == "FLEE" then
                    npc.timer = npc.timer - dt
                    local invD = distCar > 0.1 and (1.0 / distCar) or 1.0
                    local fx, fz = dxCar * invD, dzCar * invD
                    local fleeSpd = 7.4
                    npc.x = clamp(npc.x + fx * fleeSpd * dt, -CONFIG.arenaHalfSize + 4.0, CONFIG.arenaHalfSize - 4.0)
                    npc.z = clamp(npc.z + fz * fleeSpd * dt, -CONFIG.arenaHalfSize + 4.0, CONFIG.arenaHalfSize - 4.0)
                    npc.yaw = math.deg(math.atan(fx, fz))
                    targetAnim = "Run"
                    animSpeed = 1.15
                    blendTime = 0.14
                    if npc.timer <= 0.0 and distCar > 14.5 then
                        npc.state = "IDLE"
                        npc.timer = 1.4
                    end
                elseif npc.state == "IDLE" then
                    npc.timer = npc.timer - dt
                    targetAnim = "Idle"
                    animSpeed = 0.90
                    blendTime = 0.24
                    if npc.timer <= 0.0 then
                        npc.state = "WALK"
                    end
                else
                    local tx = (npc.dir > 0) and npc.bx or npc.ax
                    local tz = (npc.dir > 0) and npc.bz or npc.az
                    local dtx, dtz = tx - npc.x, tz - npc.z
                    local distT = math.sqrt(dtx * dtx + dtz * dtz)
                    if distT < 0.8 then
                        npc.dir = -npc.dir
                        npc.state = "IDLE"
                        npc.timer = 1.8
                        targetAnim = "Idle"
                    else
                        local ux, uz = dtx / distT, dtz / distT
                        npc.x = npc.x + ux * npc.speed * dt
                        npc.z = npc.z + uz * npc.speed * dt
                        npc.yaw = math.deg(math.atan(ux, uz))
                        targetAnim = "Walk"
                        animSpeed = clamp(npc.speed / 2.8, 0.75, 1.15)
                        blendTime = 0.20
                    end
                end

                if targetAnim ~= npc.anim then
                    npc.anim = targetAnim
                    world.playAnimation(npc.id, targetAnim, true, animSpeed, blendTime)
                end
                local ny = world.sampleTerrain(npc.x, npc.z)
                world.setPosition(npc.id, npc.x, ny, npc.z)
                world.setRotation(npc.id, 0.0, npc.yaw, 0.0)

                -- Procedural Head & Chest Look-At IK: citizens turn heads to watch the player pass by!
                local dLook = math.sqrt((npc.x - lookX) ^ 2 + (npc.z - lookZ) ^ 2)
                if dLook < 22.0 and npc.state ~= "FLEE" then
                    world.setLookAt(npc.id, lookX, lookY, lookZ, clamp(1.0 - dLook / 24.0, 0.35, 0.95))
                else
                    world.clearLookAt(npc.id)
                end
            end
        end
    end

    updateAiTraffic(dt)

    -- Toggle Pause Menu on Escape
    if keyJustPressed("Escape") then
        if gameState == "PLAYING" then
            gameState = "PAUSED"
            audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
        elseif gameState == "PAUSED" then
            gameState = "PLAYING"
        end
    end

    if gameState ~= "PLAYING" then
        audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
        world.setVisible("nav_arrow", false)
        world.setVisible("car_flame", false)

        if gameState == "MENU" then
            local orbit = timeNow * 0.28
            camera.setPosition(car.x + math.sin(orbit) * 10.8, car.y + 3.8, car.z + math.cos(orbit) * 10.8)
            camera.setTarget(car.x, car.y + 1.1, car.z)
            camera.setFov(55.0)
            sage.setTitle("SAGE Engine v1.5.0 — Main Menu | GPU Prop Instancing, 3-CSM, Euphoria Ragdoll & Spline AI")
        else
            sage.setTitle("SAGE Engine v1.5.0 — PAUSED (Press Esc to Resume)")
        end
        return
    end

    world.setVisible("nav_arrow", true)

    -- One-shot hotkeys
    if keyJustPressed("T") then
        timePresetIdx = (timePresetIdx % #timePresets) + 1
        local preset = timePresets[timePresetIdx]
        world.setTimeOfDay(preset.hour)
        sage.log("Sky Time of Day -> " .. preset.name)
    end

    if keyJustPressed("G") then
        applyWeatherPreset((weatherPresetIdx % #weatherPresets) + 1)
        sage.log("Weather -> " .. weatherPresets[weatherPresetIdx].name)
    end

    if keyJustPressed("C") then
        camPresetIdx = (camPresetIdx % #camPresets) + 1
        cam.orbitYaw, cam.orbitPitch = 0.0, 0.0
        sage.log("Camera View -> " .. camPresets[camPresetIdx].name)
    end

    if keyJustPressed("H") then
        showHud = not showHud
    end

    -- GTA-Style Press F: Exit / Enter Land Vehicle OR Marina Speedboat!
    if keyJustPressed("F") then
        if player.mode == "CAR" then
            local rad = degToRad(car.yaw)
            local sideX, sideZ = math.cos(rad), -math.sin(rad)
            player.mode = "FOOT"
            player.onFoot = true
            player.x = car.x + sideX * 2.6
            player.y = math.max(car.y, 0.0)
            player.z = car.z + sideZ * 2.6
            player.vx, player.vy, player.vz = 0.0, 0.0, 0.0
            player.yaw = car.yaw
            player.grounded = true
            world.setVisible("player_char", true)
            world.setPosition("player_char", player.x, player.y, player.z)
            world.setRotation("player_char", 0.0, player.yaw, 0.0)
            world.playAnimation("player_char", "Idle", true, 1.0)
            player.currentAnim = "Idle"
            world.setVisible("car_flame", false)
            audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
            audio.playChime()
            sage.log("Stepped out on foot! Walk to the Marina Pier to pilot the Speedboat (F)!")
        elseif player.mode == "BOAT" then
            local rad = degToRad(boat.yaw)
            player.mode = "FOOT"
            player.onFoot = true
            player.x = boat.x + math.cos(rad) * 2.8
            player.y = math.max(boat.y + 0.4, -0.3)
            player.z = boat.z - math.sin(rad) * 2.8
            player.vx, player.vy, player.vz = 0.0, 0.0, 0.0
            player.yaw = boat.yaw
            world.setVisible("player_char", true)
            world.setPosition("player_char", player.x, player.y, player.z)
            world.setAnimationSpeed("player_boat", 0.35)
            audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
            audio.playChime()
            sage.log("Exited Speedboat! Swimming/walking on foot.")
        else
            local dBoat = math.sqrt((player.x - boat.x) ^ 2 + (player.z - boat.z) ^ 2)
            local dCar  = math.sqrt((player.x - car.x) ^ 2 + (player.z - car.z) ^ 2)
            if dBoat < 9.5 and dBoat <= dCar then
                player.mode = "BOAT"
                player.onFoot = false
                world.setVisible("player_char", false)
                audio.setVehicleProfile(2) -- Twin Marine V8 + Water Wake!
                audio.playChime()
                sage.log("Boarded Luxury Racing Speedboat! WASD to cruise the 72m Marina Lagoon!")
            elseif dCar < 8.0 then
                player.mode = "CAR"
                player.onFoot = false
                world.setVisible("player_char", false)
                audio.setVehicleProfile(vehicles[selectedVehicleIdx].soundProfile or 0)
                audio.playChime()
                sage.log("Entered " .. vehicles[selectedVehicleIdx].name .. "!")
            else
                sage.log("Move closer to your car or the Marina Speedboat to board (or press R)!")
            end
        end
    end

    -- Q: Forward Kinetic Repulsor Blast (launches Jolt props & triggers Euphoria Ragdoll on NPCs ahead!)
    if keyJustPressed("Q") then
        local fx = player.onFoot and player.x or car.x
        local fy = player.onFoot and player.y or car.y
        local fz = player.onFoot and player.z or car.z
        local fyaw = player.onFoot and player.yaw or car.yaw
        local frad = degToRad(fyaw)
        local dirX, dirZ = math.sin(frad), math.cos(frad)
        local blastX, blastZ = fx + dirX * 9.5, fz + dirZ * 9.5

        local hitProps = physics.applyExplosion(blastX, fy + 1.0, blastZ, 16.0, 360.0)
        local hitNpcs = 0
        for _, npc in ipairs(crowdNpcs) do
            local ndx, ndz = npc.x - fx, npc.z - fz
            local dist = math.sqrt(ndx * ndx + ndz * ndz)
            local ahead = ndx * dirX + ndz * dirZ
            if dist < 24.0 and ahead > -1.0 then
                local invD = dist > 0.1 and (1.0 / dist) or 1.0
                local pwr = clamp(1.0 - dist / 26.0, 0.35, 1.0)
                world.triggerRagdoll(npc.id, (ndx * invD * 14.0 + dirX * 8.0) * pwr, 10.5 * pwr, (ndz * invD * 14.0 + dirZ * 8.0) * pwr, 2.8)
                hitNpcs = hitNpcs + 1
            end
        end
        audio.playExplosion()
        world.spawnParticles(fx + dirX * 2.2, fy + 1.3, fz + dirZ * 2.2, dirX * 18.0, 3.2, dirZ * 18.0, 0.18, 0.90, 1.0, 24, 0.24, 0.55, true, 4.0)
        sage.log(string.format("KINETIC BLAST! Launched %d Jolt prop(s) & %d Euphoria ragdoll(s)!", hitProps, hitNpcs))
    end

    if keyJustPressed("X") then
        local fx = player.onFoot and player.x or car.x
        local fy = player.onFoot and player.y or car.y
        local fz = player.onFoot and player.z or car.z
        local hitCount = physics.applyExplosion(fx, fy + 0.5, fz, 28.0, 420.0)
        local ragCount = 0
        for _, npc in ipairs(crowdNpcs) do
            local ndx, ndz = npc.x - fx, npc.z - fz
            local dist = math.sqrt(ndx * ndx + ndz * ndz)
            if dist < 24.0 then
                local invD = dist > 0.1 and (1.0 / dist) or 1.0
                local pwr = clamp(1.0 - dist / 26.0, 0.35, 1.0)
                world.triggerRagdoll(npc.id, ndx * invD * 18.0 * pwr, 11.5 * pwr, ndz * invD * 18.0 * pwr, 2.8)
                ragCount = ragCount + 1
            end
        end
        audio.playExplosion()
        world.spawnParticles(fx, fy + 0.8, fz, 0.0, 7.5, 0.0, 1.0, 0.65, 0.18, 22, 0.34, 0.75, true, 12.0)
        sage.log(string.format("SHOCKWAVE! Blasted %d Jolt object(s) & %d Euphoria ragdoll(s)!", hitCount, ragCount))
    end

    if keyJustPressed("B") then
        local fx = player.onFoot and player.x or car.x
        local fz = player.onFoot and player.z or car.z
        local fyaw = player.onFoot and player.yaw or car.yaw
        local rad = degToRad(fyaw)
        local sx = clamp(fx + math.sin(rad) * 14.0, -CONFIG.arenaHalfSize + 8.0, CONFIG.arenaHalfSize - 8.0)
        local sz = clamp(fz + math.cos(rad) * 14.0, -CONFIG.arenaHalfSize + 8.0, CONFIG.arenaHalfSize - 8.0)
        spawnCratePyramid(sx, sz, fyaw)
        audio.playChime()
    end

    if keyJustPressed("R") then
        respawnCarAtBiome(selectedBiomeIdx)
    end

    -- =========================================================================
    -- MODE A: ON-FOOT 3RD-PERSON CHARACTER (Walk, Sprint, Jump & SWIM!)
    -- =========================================================================
    if player.onFoot then
        -- Parked car decelerates gently to a stop
        car.vx = car.vx * math.exp(-4.5 * dt)
        car.vz = car.vz * math.exp(-4.5 * dt)
        car.x = car.x + car.vx * dt
        car.z = car.z + car.vz * dt
        world.setPosition("player_car", car.x, car.y, car.z)
        physics.moveKinematic("player_car", car.x, car.y + 0.65, car.z, car.pitch, car.yaw, car.roll, dt)

        local moveF = 0.0
        local turnInput = 0.0
        if input.isKeyDown("W") or input.isKeyDown("Up") then moveF = moveF + 1.0 end
        if input.isKeyDown("S") or input.isKeyDown("Down") then moveF = moveF - 1.0 end
        if input.isKeyDown("A") or input.isKeyDown("Left") then turnInput = turnInput + 1.0 end
        if input.isKeyDown("D") or input.isKeyDown("Right") then turnInput = turnInput - 1.0 end

        player.yaw = player.yaw + turnInput * 145.0 * dt
        local prad = degToRad(player.yaw)
        local pfwdX, pfwdZ = math.sin(prad), math.cos(prad)
        local sprinting = input.isKeyDown("LeftShift") or input.isKeyDown("RightShift")
        local walkSpeed = player.swimming and 4.4 or (sprinting and 9.8 or 4.6)

        player.vx = lerp(player.vx, pfwdX * moveF * walkSpeed, 11.0 * dt)
        player.vz = lerp(player.vz, pfwdZ * moveF * walkSpeed, 11.0 * dt)
        player.x = clamp(player.x + player.vx * dt, -CONFIG.arenaHalfSize, CONFIG.arenaHalfSize)
        player.z = clamp(player.z + player.vz * dt, -CONFIG.arenaHalfSize, CONFIG.arenaHalfSize)

        -- Surface + 3D Heightmap Terrain + Water Check (Hero climbs hills & swims in the Lagoon!)
        local onSurf, surfY, rampH = physics.sampleRampSurface(player.x, player.z)
        local terrainY = world.sampleTerrain(player.x, player.z)
        local overDeck = onSurf and (rampH <= 0.01) and (surfY > 0.10) and (player.y >= surfY - 0.55)
        local inW, waveY, seaY, wDepth = world.sampleWater(player.x, player.z)
        local dryY = math.max(surfY, terrainY)
        local groundY = (inW and not overDeck) and seaY or dryY

        -- Determine acoustic ground surface for procedural footstep synth (0=Asphalt, 1=Grass/Sand, 2=Wood Pier, 3=Water)
        local footSurface = 0
        if inW and not overDeck and wDepth > 0.05 then
            footSurface = 3
        elseif overDeck then
            footSurface = 2
        elseif math.abs(player.x) > 10.0 and math.abs(player.z) > 10.0 and
               math.abs(math.abs(player.x) - 64.0) > 8.0 and math.abs(math.abs(player.z) - 64.0) > 8.0 and
               math.abs(math.abs(player.x) - 148.0) > 9.0 and math.abs(math.abs(player.z) - 148.0) > 9.0 then
            footSurface = 1
        end

        player.swimming = inW and (not overDeck) and (wDepth >= 0.65)
        if player.swimming then
            local swimTargetY = waveY - 0.85
            player.y = lerp(player.y, swimTargetY, 6.0 * dt)
            player.vy = 0.0
            player.grounded = true
            player.splashAccum = player.splashAccum + dt
            if math.abs(moveF) > 0.1 and player.splashAccum >= 0.16 then
                player.splashAccum = 0.0
                world.spawnParticles(player.x, waveY + 0.05, player.z, 0.0, 2.2, 0.0, 0.84, 0.96, 1.0, 4, 0.16, 0.45, false, 12.0)
                audio.playFootstep(3, 0.45)
            end
        else
            if player.grounded and input.isKeyDown("Space") then
                player.vy = 9.0
                player.grounded = false
                audio.playFootstep(footSurface, 0.72)
            end
            if not player.grounded then
                player.vy = player.vy - 24.0 * dt
                player.y = player.y + player.vy * dt
                if player.y <= groundY then
                    player.y = groundY
                    player.vy = 0.0
                    player.grounded = true
                    audio.playFootstep(footSurface, 0.92) -- Solid landing thud!
                end
            else
                if player.y > groundY + 0.25 then
                    player.grounded = false
                else
                    player.y = lerp(player.y, groundY, 18.0 * dt)
                end
            end
        end

        -- Solid world obstacle collision for walking character
        local hitObs, rx, rz, rvx, rvz = physics.resolveCircleXZ(player.x, player.y, player.z, 0.55, player.vx, player.vz, 0.1)
        if hitObs then
            player.x, player.z, player.vx, player.vz = rx, rz, rvx, rvz
        end

        -- Seamless 5-Clip Crossfaded Hero Animation State Machine: Idle / Walk / Run / Jump / Swim!
        local pSpd = math.sqrt(player.vx * player.vx + player.vz * player.vz)
        local desiredAnim = "Idle"
        local animRate = 1.0
        local blendSec = 0.18
        if player.swimming then
            desiredAnim = "Swim"
            animRate = pSpd > 0.5 and 1.20 or 0.55
            blendSec = 0.24
        elseif not player.grounded then
            desiredAnim = "Jump"
            animRate = 1.0
            blendSec = 0.10
        elseif pSpd > 6.2 then
            desiredAnim = "Run"
            -- Velocity-matched stride rate so sneakers plant cleanly without foot-sliding!
            animRate = clamp(pSpd / 7.2, 0.85, 1.42)
            blendSec = 0.20
        elseif pSpd > 0.35 then
            desiredAnim = "Walk"
            animRate = clamp(pSpd / 3.6, 0.55, 1.45)
            blendSec = 0.16
        else
            blendSec = 0.22
        end

        if desiredAnim ~= player.currentAnim then
            player.currentAnim = desiredAnim
            world.playAnimation("player_char", desiredAnim, true, animRate, blendSec)
        else
            world.setAnimationSpeed("player_char", animRate)
        end

        -- Synced Surface-Aware Procedural Footsteps + Sprint Dust Puffs!
        if player.grounded and not player.swimming and pSpd > 0.45 then
            local stepInterval = (desiredAnim == "Run") and (0.38 / animRate) or (0.58 / animRate)
            player.stepAccum = player.stepAccum + dt
            if player.stepAccum >= stepInterval then
                player.stepAccum = 0.0
                local stepVol = (desiredAnim == "Run") and 0.78 or 0.48
                audio.playFootstep(footSurface, stepVol)
                if desiredAnim == "Run" and footSurface ~= 3 then
                    world.spawnParticles(player.x, player.y + 0.08, player.z, -player.vx * 0.2, 0.8, -player.vz * 0.2, 0.76, 0.72, 0.66, 2, 0.11, 0.32, false, 3.5)
                end
            end
        else
            player.stepAccum = 0.20
        end

        -- Subtle athletic banking lean when turning while running!
        local targetBank = (player.grounded and not player.swimming)
            and (-turnInput * clamp(pSpd / 9.5, 0.0, 1.0) * 9.5)
            or 0.0
        player.roll = lerp(player.roll, targetBank, 10.0 * dt)

        world.setPosition("player_char", player.x, player.y, player.z)
        world.setRotation("player_char", 0.0, player.yaw, player.roll)

        checkPhysicsCrateSmashes(dt)
        updateDeliveryMission(dt, timeNow, player.x, player.y, player.z)

        -- Smooth Over-The-Shoulder 3rd-Person Character Camera (with dynamic sprint FOV!)
        if input.isMouseDown("Right") then
            local mdx, mdy = input.getMouseDelta()
            cam.orbitYaw = cam.orbitYaw - mdx * 0.35
            cam.orbitPitch = clamp(cam.orbitPitch + mdy * 0.22, -12.0, 35.0)
        elseif pSpd > 0.5 then
            cam.orbitYaw = lerp(cam.orbitYaw, 0.0, 2.0 * dt)
            cam.orbitPitch = lerp(cam.orbitPitch, 0.0, 2.0 * dt)
        end
        local cyaw = degToRad(player.yaw + cam.orbitYaw)
        local cfx, cfz = math.sin(cyaw), math.cos(cyaw)
        local celev = math.sin(degToRad(cam.orbitPitch)) * 5.0
        local camDist = (desiredAnim == "Run") and 4.85 or 4.35
        cam.x = lerp(cam.x, player.x - cfx * camDist, 8.0 * dt)
        cam.y = lerp(cam.y, math.max(0.85, player.y + 2.20 + celev), 8.0 * dt)
        cam.z = lerp(cam.z, player.z - cfz * camDist, 8.0 * dt)
        cam.tx = lerp(cam.tx, player.x + cfx * 1.4, 14.0 * dt)
        cam.ty = lerp(cam.ty, player.y + 1.38, 14.0 * dt)
        cam.tz = lerp(cam.tz, player.z + cfz * 1.4, 14.0 * dt)
        local targetFov = (desiredAnim == "Run") and 66.0 or 60.0
        cam.fov = lerp(cam.fov, targetFov, 6.0 * dt)
        camera.setPosition(cam.x, cam.y, cam.z)
        camera.setTarget(cam.tx, cam.ty, cam.tz)
        camera.setFov(cam.fov)

        titleUpdateAccum = titleUpdateAccum + dt
        if titleUpdateAccum >= 0.25 then
            titleUpdateAccum = 0.0
            local biome = getCurrentBiomeName(player.x, player.z)
            sage.setTitle(string.format("SAGE v1.5 | %s | ON FOOT [%s] (F = Car/Boat | Q = Kinetic Blast)", biome, player.currentAnim))
        end
        return
    end

    -- =========================================================================
    -- MODE A2: DRIVABLE LUXURY RACING SPEEDBOAT (Glides on 3D Ocean Waves!)
    -- =========================================================================
    if player.mode == "BOAT" then
        local bThrottle = 0.0
        local bSteer = 0.0
        if input.isKeyDown("W") or input.isKeyDown("Up") then bThrottle = bThrottle + 1.0 end
        if input.isKeyDown("S") or input.isKeyDown("Down") then bThrottle = bThrottle - 1.0 end
        if input.isKeyDown("A") or input.isKeyDown("Left") then bSteer = bSteer + 1.0 end
        if input.isKeyDown("D") or input.isKeyDown("Right") then bSteer = bSteer - 1.0 end
        local bBoost = input.isKeyDown("LeftShift") or input.isKeyDown("RightShift")

        boat.yaw = boat.yaw + bSteer * (72.0 + math.min(math.abs(boat.speed) * 2.2, 42.0)) * dt
        local brad = degToRad(boat.yaw)
        local bfx, bfz = math.sin(brad), math.cos(brad)

        local topBoatSpd = bBoost and 26.0 or 18.0
        if bThrottle > 0.0 then
            boat.speed = clamp(boat.speed + (bBoost and 20.0 or 13.0) * dt, -7.0, topBoatSpd)
        elseif bThrottle < 0.0 then
            boat.speed = clamp(boat.speed - 14.0 * dt, -7.0, topBoatSpd)
        else
            boat.speed = boat.speed * math.exp(-1.6 * dt)
        end

        local nx = boat.x + bfx * boat.speed * dt
        local nz = boat.z + bfz * boat.speed * dt
        -- Keep Speedboat cleanly inside deep navigable water (prevents beaching onto dry sand!)
        local inW, waveY, _, wDepth = world.sampleWater(nx, nz)
        if inW and wDepth > 0.42 then
            boat.x, boat.z = nx, nz
        else
            boat.speed = boat.speed * -0.35
        end

        -- Avoid pier & lighthouse quay solid colliders
        local hitB, brx, brz = physics.resolveCircleXZ(boat.x, 0.2, boat.z, 2.1, bfx * boat.speed, bfz * boat.speed, 0.25)
        if hitB then
            boat.x, boat.z = brx, brz
            boat.speed = boat.speed * 0.4
        end

        local _, curWaveY = world.sampleWater(boat.x, boat.z)
        boat.y = lerp(boat.y, curWaveY - 0.08 + clamp(boat.speed * 0.012, 0.0, 0.25), 9.0 * dt)
        boat.pitch = lerp(boat.pitch, -clamp(boat.speed * 0.48, -4.0, 11.0) + math.sin(timeNow * 2.2) * 2.0, 6.0 * dt)
        boat.roll = lerp(boat.roll, -bSteer * clamp(math.abs(boat.speed) * 0.9, 0.0, 14.0) + math.cos(timeNow * 1.8) * 2.2, 6.0 * dt)

        world.setPosition("player_boat", boat.x, boat.y, boat.z)
        world.setRotation("player_boat", boat.pitch, boat.yaw, boat.roll)
        world.setAnimationSpeed("player_boat", 0.5 + math.abs(boat.speed) * 0.35)
        world.setPointLight("car_headlight", boat.x + bfx * 5.0, boat.y + 1.5, boat.z + bfz * 5.0, 0.25, 0.90, 1.0, 26.0, 2.8)

        -- White-cyan churning wake spray behind twin outboard propellers!
        boat.wakeAccum = boat.wakeAccum + dt
        if math.abs(boat.speed) > 2.0 and boat.wakeAccum >= 0.045 then
            boat.wakeAccum = 0.0
            world.spawnParticles(boat.x - bfx * 3.1, curWaveY + 0.1, boat.z - bfz * 3.1, -bfx * 2.5, 2.2 + math.abs(boat.speed) * 0.1, -bfz * 2.5, 0.85, 0.97, 1.0, 3, 0.24, 0.55, false, 11.0)
        end

        audio.setVehicleState(clamp(0.18 + math.abs(boat.speed) / 26.0 * 0.78, 0.12, 0.95), bThrottle > 0 and 0.9 or 0.2, 0.0, bBoost and 0.8 or 0.0, false)
        updateDeliveryMission(dt, timeNow, boat.x, boat.y, boat.z)

        -- Dynamic Boat Chase Camera
        if input.isMouseDown("Right") then
            local mdx, mdy = input.getMouseDelta()
            cam.orbitYaw = cam.orbitYaw - mdx * 0.35
            cam.orbitPitch = clamp(cam.orbitPitch + mdy * 0.22, -10.0, 32.0)
        else
            cam.orbitYaw = lerp(cam.orbitYaw, 0.0, 2.0 * dt)
            cam.orbitPitch = lerp(cam.orbitPitch, 0.0, 2.0 * dt)
        end
        local cyaw = degToRad(boat.yaw + cam.orbitYaw)
        local cfx, cfz = math.sin(cyaw), math.cos(cyaw)
        cam.x = lerp(cam.x, boat.x - cfx * 11.8, 6.5 * dt)
        cam.y = lerp(cam.y, math.max(1.2, boat.y + 4.4 + math.sin(degToRad(cam.orbitPitch)) * 8.0), 6.5 * dt)
        cam.z = lerp(cam.z, boat.z - cfz * 11.8, 6.5 * dt)
        cam.tx = lerp(cam.tx, boat.x + cfx * 5.0, 12.0 * dt)
        cam.ty = lerp(cam.ty, boat.y + 1.2, 12.0 * dt)
        cam.tz = lerp(cam.tz, boat.z + cfz * 5.0, 12.0 * dt)
        camera.setPosition(cam.x, cam.y, cam.z)
        camera.setTarget(cam.tx, cam.ty, cam.tz)
        camera.setFov(lerp(cam.fov, 62.0 + math.abs(boat.speed) * 0.35, 5.0 * dt))

        titleUpdateAccum = titleUpdateAccum + dt
        if titleUpdateAccum >= 0.25 then
            titleUpdateAccum = 0.0
            sage.setTitle(string.format("SAGE v1.5 | PALM MARINA SPEEDBOAT | %d knots | F = Exit Boat", math.floor(math.abs(boat.speed) * 1.94)))
        end
        return
    end

    -- =========================================================================
    -- MODE B: DRIVING SUPERCAR (6-Gear, Drift, Nitro & Realistic Water Physics!)
    -- =========================================================================
    local throttle = 0.0
    local rawSteer = 0.0
    if not car.flooded then
        if input.isKeyDown("W") or input.isKeyDown("Up") then throttle = throttle + 1.0 end
        if input.isKeyDown("S") or input.isKeyDown("Down") then throttle = throttle - 1.0 end
        if input.isKeyDown("A") or input.isKeyDown("Left") then rawSteer = rawSteer + 1.0 end
        if input.isKeyDown("D") or input.isKeyDown("Right") then rawSteer = rawSteer - 1.0 end
    end
    car.steerSmooth = lerp(car.steerSmooth, rawSteer, 13.0 * dt)
    car.isDrifting = (not car.flooded) and (not car.inWater) and input.isKeyDown("Space")

    local shiftHeld = (not car.flooded) and (not car.inWater) and (input.isKeyDown("LeftShift") or input.isKeyDown("RightShift"))
    if car.boostTimer > 0.0 then car.boostTimer = car.boostTimer - dt end
    car.isBoosting = (shiftHeld and car.boostEnergy > 1.0) or (car.boostTimer > 0.0)
    if shiftHeld and car.boostEnergy > 0.0 then
        car.boostEnergy = math.max(0.0, car.boostEnergy - 32.0 * dt)
    else
        car.boostEnergy = math.min(100.0, car.boostEnergy + 18.0 * dt)
    end

    local rad = degToRad(car.yaw)
    local fwdX, fwdZ = math.sin(rad), math.cos(rad)
    local leftX, leftZ = math.cos(rad), -math.sin(rad)

    local forwardVel = car.vx * fwdX + car.vz * fwdZ
    local lateralVel = car.vx * leftX + car.vz * leftZ
    local speedNorm = clamp(math.abs(forwardVel) / CONFIG.maxSpeed, 0.0, 1.0)

    local accelPitchTarget = 0.0
    if car.grounded then
        local turnRate = lerp(CONFIG.lowSpeedTurn, CONFIG.highSpeedTurn, speedNorm)
        local speedFactor = clamp(math.abs(forwardVel) / 4.5, 0.0, 1.0)
        local reverseSign = forwardVel >= -0.25 and 1.0 or -1.0
        local driftBonus = car.isDrifting and 1.32 or 1.0
        car.yaw = car.yaw + car.steerSmooth * turnRate * driftBonus * speedFactor * reverseSign * dt

        rad = degToRad(car.yaw)
        fwdX, fwdZ = math.sin(rad), math.cos(rad)
        leftX, leftZ = math.cos(rad), -math.sin(rad)

        local topSpeed = car.isBoosting and CONFIG.boostSpeed or CONFIG.maxSpeed
        if throttle > 0.0 then
            local accel = car.isBoosting and (CONFIG.acceleration * 1.95) or CONFIG.acceleration
            forwardVel = forwardVel + accel * dt
            accelPitchTarget = car.isBoosting and -3.2 or -1.4
        elseif throttle < 0.0 then
            if forwardVel > 0.8 then
                forwardVel = forwardVel - CONFIG.brakeForce * dt
                accelPitchTarget = 2.6
            else
                forwardVel = forwardVel - CONFIG.reverseAccel * dt
                accelPitchTarget = 0.8
            end
        else
            forwardVel = forwardVel * math.exp(-1.5 * dt)
        end

        if car.isBoosting and throttle >= 0.0 then
            forwardVel = math.max(forwardVel, topSpeed * 0.72)
        end
        forwardVel = clamp(forwardVel, -15.0, topSpeed)

        local wetGripMult = 1.0 - weatherPresets[weatherPresetIdx].wet * 0.34
        local grip = (car.isDrifting and CONFIG.driftGrip or CONFIG.normalGrip) * wetGripMult
        lateralVel = lateralVel * math.exp(-grip * dt)

        car.vx = fwdX * forwardVel + leftX * lateralVel
        car.vz = fwdZ * forwardVel + leftZ * lateralVel
    else
        car.yaw = car.yaw + car.steerSmooth * 38.0 * dt
    end

    local prevY = car.y
    car.x = car.x + car.vx * dt
    car.z = car.z + car.vz * dt

    -- Unified 4-Wheel Surface Elevation (3D Heightmap Terrain + 12 Stunt Ramps + Drivable Pier) + Water Physics!
    local function sampleWheelGround(wx, wz)
        local th = world.sampleTerrain(wx, wz)
        local onS, sy = physics.sampleRampSurface(wx, wz)
        if onS and sy > th then return sy end
        return th
    end

    local onSurface, surfY, rampHeight = physics.sampleRampSurface(car.x, car.z)
    local terrainCenterY = world.sampleTerrain(car.x, car.z)
    local isElevatedDeck = onSurface and (rampHeight <= 0.01) and (surfY > 0.10)
    local isRamp = onSurface and (rampHeight > 0.05)

    local inWaterVol, waveSurfaceY, seabedY = world.sampleWater(car.x, car.z)
    local overPier = isElevatedDeck and (car.y >= surfY - 0.45)
    local dryGroundY = math.max(surfY, terrainCenterY)
    local groundY = (inWaterVol and not overPier) and seabedY or dryGroundY

    -- True 4-Corner Wheel Contact Sampling (FL, FR, RL, RR) for Realistic Hill Pitch & Side-Slope Roll!
    local wb, trk = 1.45, 0.95
    local hFL = sampleWheelGround(car.x + fwdX * wb + leftX * trk, car.z + fwdZ * wb + leftZ * trk)
    local hFR = sampleWheelGround(car.x + fwdX * wb - leftX * trk, car.z + fwdZ * wb - leftZ * trk)
    local hRL = sampleWheelGround(car.x - fwdX * wb + leftX * trk, car.z - fwdZ * wb + leftZ * trk)
    local hRR = sampleWheelGround(car.x - fwdX * wb - leftX * trk, car.z - fwdZ * wb - leftZ * trk)
    local slopePitchDeg = -math.deg(math.atan(0.5 * (hFL + hFR) - 0.5 * (hRL + hRR), wb * 2.0))
    local slopeRollDeg  = -math.deg(math.atan(0.5 * (hFL + hRL) - 0.5 * (hFR + hRR), trk * 2.0))
    car.terrainRoll = lerp(car.terrainRoll, clamp(slopeRollDeg, -24.0, 24.0), 10.0 * dt)

    -- Spring-Damper Vertical Suspension Oscillation (kSpring=95, kDamper=11 -> tactile landing & hill bounce!)
    car.suspVel = car.suspVel + (-95.0 * car.suspOffset - 11.0 * car.suspVel) * dt
    car.suspOffset = clamp(car.suspOffset + car.suspVel * dt, -0.22, 0.18)

    if car.grounded then
        if isRamp and surfY > terrainCenterY + 0.05 then
            local climbRate = (groundY - prevY) / math.max(dt, 0.001)
            car.y = groundY
            car.vy = math.max(climbRate, 0.0)
            car.pitch = lerp(car.pitch, -17.5, 14.0 * dt)
            if surfY > rampHeight * 0.78 and math.abs(forwardVel) > 10.0 then
                car.vy = math.max(car.vy, math.abs(forwardVel) * 0.44 + 4.8)
                car.boostTimer = math.max(car.boostTimer, 0.55)
                car.grounded = false
            end
        elseif overPier then
            car.y = lerp(car.y, surfY, 18.0 * dt)
            car.vy = 0.0
            car.pitch = lerp(car.pitch, accelPitchTarget, 9.0 * dt)
        else
            local climbRate = (groundY - prevY) / math.max(dt, 0.001)
            -- Cresting a steep Alpine hill at high speed launches the vehicle naturally into the air!
            if car.y > groundY + 0.24 then
                car.vy = clamp(car.vy, 0.0, 14.0)
                car.grounded = false
            else
                car.y = groundY
                car.vy = math.max(climbRate * 0.85, 0.0)
                local targetPitch = clamp(slopePitchDeg + accelPitchTarget, -28.0, 26.0)
                car.pitch = lerp(car.pitch, targetPitch, 11.0 * dt)
            end
        end
    end

    if not car.grounded then
        car.airTime = car.airTime + dt
        local effGrav = (inWaterVol and not overPier and car.y < waveSurfaceY) and (CONFIG.gravity * 0.34) or CONFIG.gravity
        car.vy = car.vy - effGrav * dt
        car.y = car.y + car.vy * dt
        car.pitch = lerp(car.pitch, clamp(-car.vy * 2.1, -26.0, 22.0), 6.0 * dt)

        if car.y <= groundY then
            local landingImpact = math.abs(car.vy)
            car.y = groundY
            car.vy = 0.0
            car.grounded = true
            -- Compress 4-wheel spring-damper suspension cleanly on landing!
            car.suspVel = car.suspVel - clamp(landingImpact * 0.28, 0.0, 3.2)
            if car.airTime >= 0.42 and not (inWaterVol and not overPier) then
                stuntJumps = stuntJumps + 1
                local airBonus = math.floor(car.airTime * 150.0)
                score = score + airBonus
                updateBestScore()
                car.boostEnergy = math.min(100.0, car.boostEnergy + 45.0)
                audio.playImpact(0.75)
                audio.playChime()
                physics.applyExplosion(car.x, car.y + 0.3, car.z, 10.0, 110.0)
                sage.log(string.format("STUNT JUMP #%d! Air: %.2fs (+%d pts)", stuntJumps, car.airTime, airBonus))
            end
            car.airTime = 0.0
        end
    end

    -- Realistic Water Interaction: Drag, 3D Splashes & Engine Flooding
    local wasInWater = car.inWater
    car.inWater = inWaterVol and (not overPier) and (car.y < waveSurfaceY + 0.12)
    car.waterDepth = car.inWater and math.max(0.0, waveSurfaceY - car.y) or 0.0

    if not inWaterVol and car.grounded and not car.flooded then
        car.lastDryX, car.lastDryZ, car.lastDryYaw = car.x, car.z, car.yaw
    end

    if car.inWater then
        local spdNow = math.sqrt(car.vx * car.vx + car.vz * car.vz)
        if not wasInWater then
            audio.playImpact(clamp(0.45 + spdNow * 0.03, 0.45, 1.0))
            world.spawnParticles(car.x, waveSurfaceY + 0.15, car.z, car.vx * 0.25, 5.2 + spdNow * 0.18, car.vz * 0.25, 0.82, 0.94, 1.0, 24, 0.34, 0.85, false, 15.0)
        end
        local dragFactor = math.exp(-(3.8 + car.waterDepth * 4.5) * dt)
        car.vx = car.vx * dragFactor
        car.vz = car.vz * dragFactor

        car.fxAccum = car.fxAccum + dt
        if spdNow > 1.2 and car.fxAccum >= 0.07 then
            car.fxAccum = 0.0
            world.spawnParticles(car.x, waveSurfaceY + 0.08, car.z, -car.vx * 0.15, 2.8 + spdNow * 0.14, -car.vz * 0.15, 0.86, 0.96, 1.0, 5, 0.22, 0.55, false, 14.0)
        end

        if car.waterDepth >= 0.45 and not car.flooded then
            car.flooded = true
            car.floodTimer = 2.6
            car.boostTimer = 0.0
            car.isBoosting = false
            audio.playImpact(0.95)
            world.spawnParticles(car.x, waveSurfaceY + 0.2, car.z, 0.0, 6.2, 0.0, 0.84, 0.95, 1.0, 32, 0.38, 1.0, false, 13.0)
            sage.log("SPLASH! Engine flooded in deep water — rescuing back to dry shore...")
        end
    end

    if car.flooded then
        car.pitch = lerp(car.pitch, 10.5, 4.0 * dt)
        car.roll = lerp(car.roll, math.sin(timeNow * 2.6) * 5.0, 4.0 * dt)
        car.floodTimer = car.floodTimer - dt
        if car.fxAccum >= 0.14 then
            car.fxAccum = 0.0
            world.spawnParticles(car.x, waveSurfaceY + 0.05, car.z, 0.0, 2.4, 0.0, 0.85, 0.96, 1.0, 4, 0.22, 0.65, false, 6.0)
        else
            car.fxAccum = car.fxAccum + dt
        end
        if car.floodTimer <= 0.0 then
            car.x, car.y, car.z = car.lastDryX, 0.0, car.lastDryZ
            car.vx, car.vy, car.vz = 0.0, 0.0, 0.0
            car.yaw, car.pitch, car.roll = car.lastDryYaw, 0.0, 0.0
            car.grounded = true
            car.inWater = false
            car.waterDepth = 0.0
            car.flooded = false
            audio.playChime()
        end
    end

    -- Solid Collision vs ALL Buildings, Trees, Cacti, Cabins, Ferris Wheel, Gas Stations & Seawalls!
    local hitObs, rx, rz, rvx, rvz = physics.resolveCircleXZ(car.x, car.y, car.z, 1.55, car.vx, car.vz, 0.32)
    if hitObs then
        local preSpeed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
        car.x, car.z, car.vx, car.vz = rx, rz, rvx, rvz
        if preSpeed > 4.0 and impactSoundCooldown <= 0.0 then
            audio.playImpact(clamp(preSpeed / 24.0, 0.3, 1.0))
            impactSoundCooldown = 0.22
        end
    end

    local S = CONFIG.arenaHalfSize
    car.x = clamp(car.x, -S, S)
    car.z = clamp(car.z, -S, S)

    if not car.flooded then
        local bikeLean = vehicles[selectedVehicleIdx].isBike and (car.steerSmooth * speedNorm * 22.0) or 0.0
        local targetRoll = clamp(lateralVel * 0.95 + car.steerSmooth * speedNorm * 2.5 - bikeLean + car.terrainRoll, -28.0, 28.0)
        car.roll = lerp(car.roll, targetRoll, 9.0 * dt)
    end

    physics.moveKinematic("player_car", car.x, car.y + 0.65, car.z, car.pitch, car.yaw, car.roll, dt)
    checkPhysicsCrateSmashes(dt)
    updateDeliveryMission(dt, timeNow, car.x, car.y, car.z)

    world.setPosition("player_car", car.x, car.y + car.suspOffset, car.z)
    world.setRotation("player_car", car.pitch, car.yaw, car.roll)

    if car.isBoosting then
        local flicker = 1.65 + 0.55 * math.sin(timeNow * 48.0)
        world.setVisible("car_flame", true)
        world.setPosition("car_flame", car.x - fwdX * 2.7, car.y + 0.42, car.z - fwdZ * 2.7)
        world.setRotation("car_flame", car.pitch, car.yaw + 180.0, 0.0)
        world.setScale("car_flame", 0.85, 0.6, flicker)
        world.spawnParticles(car.x - fwdX * 2.6, car.y + 0.35, car.z - fwdZ * 2.6, -fwdX * 6.0, 1.4, -fwdZ * 6.0, 0.18, 0.88, 1.0, 2, 0.16, 0.35, true, 6.0)
    else
        world.setVisible("car_flame", false)
    end

    -- Real-Time Car Headlights + Tail/Nitro Underglow Point Lights!
    local headPower = car.flooded and 0.4 or 3.0
    world.setPointLight("car_headlight", car.x + fwdX * 6.2, car.y + 1.35, car.z + fwdZ * 6.2, 1.0, 0.94, 0.82, 26.0, headPower)
    if car.isBoosting then
        world.setPointLight("car_tail", car.x - fwdX * 2.8, car.y + 0.8, car.z - fwdZ * 2.8, 0.15, 0.86, 1.0, 16.0, 2.8)
    elseif throttle < 0.0 and not car.flooded then
        world.setPointLight("car_tail", car.x - fwdX * 2.6, car.y + 0.8, car.z - fwdZ * 2.6, 1.0, 0.12, 0.10, 12.0, 2.0)
    else
        world.setPointLight("car_tail", car.x - fwdX * 2.6, car.y + 0.8, car.z - fwdZ * 2.6, 0.95, 0.15, 0.12, 8.0, 0.7)
    end

    -- 6-Gear Transmission & Combustion Synth (with realistic gear-shift exhaust pops!)
    local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    local gearSpan = CONFIG.boostSpeed / 6.0
    local prevGear = car.gear
    car.gear = clamp(math.floor(speed / gearSpan) + 1, 1, 6)
    if car.gear > prevGear and throttle > 0.0 and not car.flooded then
        audio.playShiftPop(car.isBoosting and 0.95 or 0.72)
    end
    local gearProgress = (speed - (car.gear - 1) * gearSpan) / gearSpan
    local targetRpm = car.flooded and 0.0 or clamp(0.14 + gearProgress * 0.72 + (car.isBoosting and 0.18 or 0.0), 0.08, 1.0)
    if not car.grounded and throttle > 0.0 and not car.flooded then targetRpm = 0.96 end
    car.rpm = lerp(car.rpm, targetRpm, 10.0 * dt)

    local driftIntensity = (car.grounded and not car.inWater and not car.flooded and math.abs(lateralVel) > 2.4)
        and clamp((math.abs(lateralVel) - 2.4) / 8.0, 0.0, 1.0)
        or 0.0
    if driftIntensity > 0.18 then
        world.spawnParticles(car.x - fwdX * 1.6 + leftX * 0.85, car.y + 0.22, car.z - fwdZ * 1.6 + leftZ * 0.85, 0.0, 1.6, 0.0, 0.82, 0.84, 0.88, 1, 0.30, 0.55, false, -1.2)
        world.spawnParticles(car.x - fwdX * 1.6 - leftX * 0.85, car.y + 0.22, car.z - fwdZ * 1.6 - leftZ * 0.85, 0.0, 1.6, 0.0, 0.82, 0.84, 0.88, 1, 0.30, 0.55, false, -1.2)
    end
    if car.flooded then
        audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
    else
        audio.setVehicleState(car.rpm, throttle > 0.0 and 1.0 or 0.15, driftIntensity, car.isBoosting and 1.0 or 0.0, not car.grounded)
    end

    -- 3D Chase Camera + Right-Mouse Orbit
    local preset = camPresets[camPresetIdx]
    if input.isMouseDown("Right") then
        local mdx, mdy = input.getMouseDelta()
        cam.orbitYaw = cam.orbitYaw - mdx * 0.35
        cam.orbitPitch = clamp(cam.orbitPitch + mdy * 0.22, -12.0, 35.0)
    elseif speed > 2.5 then
        cam.orbitYaw = lerp(cam.orbitYaw, 0.0, 2.4 * dt)
        cam.orbitPitch = lerp(cam.orbitPitch, 0.0, 2.4 * dt)
    end

    local totalYawRad = degToRad(car.yaw + cam.orbitYaw)
    local camFwdX, camFwdZ = math.sin(totalYawRad), math.cos(totalYawRad)
    local orbitElev = math.sin(degToRad(cam.orbitPitch)) * math.max(preset.dist, 0.0)

    local desiredCamX = car.x - camFwdX * preset.dist
    local desiredCamY = math.max(0.8, car.y + preset.height + orbitElev + clamp(speed * 0.03, 0.0, 1.2))
    local desiredCamZ = car.z - camFwdZ * preset.dist
    local desiredTargetX = car.x + camFwdX * preset.lookAhead
    local desiredTargetY = car.y + 1.35
    local desiredTargetZ = car.z + camFwdZ * preset.lookAhead
    local desiredFov = preset.fov + clamp(speed * 0.26, 0.0, 14.0) + (car.isBoosting and 6.0 or 0.0)

    local camSmooth = (camPresetIdx == 3) and 28.0 or 6.5
    cam.x = lerp(cam.x, desiredCamX, camSmooth * dt)
    cam.y = lerp(cam.y, desiredCamY, camSmooth * dt)
    cam.z = lerp(cam.z, desiredCamZ, camSmooth * dt)
    cam.tx = lerp(cam.tx, desiredTargetX, 14.0 * dt)
    cam.ty = lerp(cam.ty, desiredTargetY, 14.0 * dt)
    cam.tz = lerp(cam.tz, desiredTargetZ, 14.0 * dt)
    cam.fov = lerp(cam.fov, desiredFov, 5.5 * dt)

    camera.setPosition(cam.x, cam.y, cam.z)
    camera.setTarget(cam.tx, cam.ty, cam.tz)
    camera.setFov(cam.fov)

    titleUpdateAccum = titleUpdateAccum + dt
    if titleUpdateAccum >= 0.25 then
        titleUpdateAccum = 0.0
        local kmh = math.floor(speed * 3.6)
        local biome = getCurrentBiomeName(car.x, car.z)
        sage.setTitle(string.format(
            "SAGE v1.5 | %s | G%d %d km/h | %s | Score: %d (Best: %d)",
            biome, car.gear, kmh, weatherPresets[weatherPresetIdx].name, score, bestScore
        ))
    end
end

-- -----------------------------------------------------------------------------
-- Main Menu, Pause Menu & Compact HUD (All Menus Hide Cleanly During Gameplay!)
-- -----------------------------------------------------------------------------
function onRenderUI()
    local vpW, vpH = ui.getViewportSize()

    if gameState == "MENU" then
        if ui.beginCenteredWindow("SAGE ENGINE v1.5.0 — AAA PROP INSTANCING, 3-CSM & EUPHORIA AI", 490, 460) then
            ui.setFontScale(1.38)
            ui.textCentered("SAGE STUNT & DELIVERY")
            ui.setFontScale(1.0)
            ui.textCentered("v1.5.0  |  GPU Prop Batching  |  3-CSM  |  Euphoria Ragdoll  |  Spline AI")
            ui.textCentered("All-Time Best Score: " .. tostring(bestScore))
            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("PLAY GAME", 450, 44) then
                respawnCarAtBiome(selectedBiomeIdx)
                applySelectedVehicle()
                audio.playChime()
                gameState = "PLAYING"
            end

            ui.spacing()
            ui.separator()
            ui.spacing()

            ui.text("Vehicle: " .. vehicles[selectedVehicleIdx].name)
            if ui.button("Switch Vehicle", 220, 30) then
                selectedVehicleIdx = (selectedVehicleIdx % #vehicles) + 1
                applySelectedVehicle()
            end

            ui.spacing()
            ui.text("Starting Biome (560m x 560m Island): " .. biomeSpawns[selectedBiomeIdx].name)
            if ui.button("Downtown", 106, 28) then selectedBiomeIdx = 1; respawnCarAtBiome(1) end
            ui.sameLine()
            if ui.button("Highlands", 106, 28) then selectedBiomeIdx = 2; respawnCarAtBiome(2) end
            ui.sameLine()
            if ui.button("Palm Marina", 106, 28) then selectedBiomeIdx = 3; respawnCarAtBiome(3) end
            ui.sameLine()
            if ui.button("Red Desert", 106, 28) then selectedBiomeIdx = 4; respawnCarAtBiome(4) end

            ui.spacing()
            ui.text("Sky & Weather: " .. timePresets[timePresetIdx].name .. "  /  " .. weatherPresets[weatherPresetIdx].name)
            if ui.button("Sunset", 106, 26) then timePresetIdx = 1; world.setTimeOfDay(17.4) end
            ui.sameLine()
            if ui.button("Noon", 106, 26) then timePresetIdx = 2; world.setTimeOfDay(12.5) end
            ui.sameLine()
            if ui.button("Starry Night", 106, 26) then timePresetIdx = 3; world.setTimeOfDay(22.5) end
            ui.sameLine()
            if ui.button("Sunrise", 106, 26) then timePresetIdx = 4; world.setTimeOfDay(6.4) end

            if ui.button("Clear Sky", 145, 26) then applyWeatherPreset(1) end
            ui.sameLine()
            if ui.button("Light Drizzle", 145, 26) then applyWeatherPreset(2) end
            ui.sameLine()
            if ui.button("Neon Rainstorm", 145, 26) then applyWeatherPreset(3) end

            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("Quit to Desktop", 450, 32) then
                sage.quit()
            end
        end
        ui.endWindow()
        return
    end

    if gameState == "PAUSED" then
        if ui.beginCenteredWindow("PAUSED", 360, 295) then
            ui.setFontScale(1.25)
            ui.textCentered("GAME PAUSED")
            ui.setFontScale(1.0)
            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("Resume Game (Esc)", 340, 38) then
                gameState = "PLAYING"
            end
            ui.spacing()
            if ui.button("Fast Travel -> " .. biomeSpawns[(selectedBiomeIdx % #biomeSpawns) + 1].name, 340, 32) then
                selectedBiomeIdx = (selectedBiomeIdx % #biomeSpawns) + 1
                respawnCarAtBiome(selectedBiomeIdx)
                gameState = "PLAYING"
            end
            ui.spacing()
            if ui.button("Switch Vehicle", 340, 32) then
                selectedVehicleIdx = (selectedVehicleIdx % #vehicles) + 1
                applySelectedVehicle()
            end
            ui.spacing()
            if ui.button("Return to Main Menu", 340, 32) then
                gameState = "MENU"
            end
            ui.spacing()
            if ui.button("Quit to Desktop", 340, 32) then
                sage.quit()
            end
        end
        ui.endWindow()
        return
    end

    if car.flooded and not player.onFoot then
        if ui.beginCenteredWindow("WATER HAZARD", 390, 108) then
            ui.setFontScale(1.25)
            ui.textCentered("ENGINE FLOODED!")
            ui.setFontScale(1.0)
            ui.spacing()
            ui.textCentered(string.format("Rescuing vehicle to dry shore in %.1fs... (or press R)", math.max(car.floodTimer, 0.0)))
        end
        ui.endWindow()
    end

    if showHud then
        local spot = deliverySpots[activeDeliveryIdx]
        local fx = player.onFoot and player.x or car.x
        local fz = player.onFoot and player.z or car.z
        local biome = getCurrentBiomeName(fx, fz)

        -- 1. LIVE GTA-STYLE 2D CORNER MINIMAP RADAR (Bottom-Left!)
        local mapSize = 166
        local mapWinW, mapWinH = mapSize + 18, mapSize + 36
        if ui.beginWindow("Radar (N ^) | G = Weather", 14, vpH - mapWinH - 14, mapWinW, mapWinH, true) then
            local ox, oy = 9, 27
            local half = mapSize * 0.5
            local function toMap(wx, wz)
                -- SAGE +X is West/Left, +Z is North/Up -> map X = -wx/280, map Y = -wz/280
                local mx = ox + half + clamp(wx / 280.0, -1.0, 1.0) * half
                local my = oy + half - clamp(wz / 280.0, -1.0, 1.0) * half
                return mx, my
            end

            -- 4 Biome Quadrants + Carved Turquoise Palm Lagoon
            ui.drawRect(ox,        oy,        half, half, 0.16, 0.18, 0.24, 0.92, true) -- NW Downtown
            ui.drawRect(ox + half, oy,        half, half, 0.13, 0.28, 0.16, 0.92, true) -- NE Highlands
            ui.drawRect(ox,        oy + half, half, half, 0.52, 0.30, 0.17, 0.92, true) -- SW Red Desert
            ui.drawRect(ox + half, oy + half, half, half, 0.62, 0.54, 0.34, 0.92, true) -- SE Palm Coast

            local lx0, ly0 = toMap(156.0, -156.0)
            local lx1, ly1 = toMap(228.0, -228.0)
            ui.drawRect(lx0, ly0, lx1 - lx0, ly1 - ly0, 0.06, 0.58, 0.72, 0.96, true, 3.0)

            -- Highway Cross & 3 Ring Loops
            ui.drawLine(ox + half, oy, ox + half, oy + mapSize, 0.30, 0.32, 0.38, 0.95, 2.0)
            ui.drawLine(ox, oy + half, ox + mapSize, oy + half, 0.30, 0.32, 0.38, 0.95, 2.0)
            for _, r in ipairs({ 64.0, 148.0, 236.0 }) do
                local rx0, ry0 = toMap(-r, r)
                local rx1, ry1 = toMap(r, -r)
                ui.drawRect(rx0, ry0, rx1 - rx0, ry1 - ry0, 0.36, 0.38, 0.44, 0.85, false)
            end

            -- AI Traffic Cruisers (Silver/Red braking), Citizens (Yellow/Orange ragdoll), Helicopter, Delivery Target
            for _, tc in ipairs(trafficCars) do
                local tx, ty = toMap(tc.x, tc.z)
                if tc.braking then
                    ui.drawCircle(tx, ty, 2.8, 1.0, 0.25, 0.18, 0.98, true)
                else
                    ui.drawCircle(tx, ty, 2.5, 0.85, 0.88, 0.94, 0.95, true)
                end
            end
            for _, npc in ipairs(crowdNpcs) do
                local nx, ny = toMap(npc.x, npc.z)
                if npc.ragdolling then
                    ui.drawCircle(nx, ny, 2.8, 1.0, 0.35, 0.12, 1.0, true)
                else
                    ui.drawCircle(nx, ny, 2.0, 0.96, 0.88, 0.35, 0.90, true)
                end
            end
            local hmx, hmy = toMap(heliPos.x, heliPos.z)
            ui.drawCircle(hmx, hmy, 3.0, 1.0, 0.52, 0.12, 0.95, true)

            local dmx, dmy = toMap(spot.x, spot.z)
            ui.drawCircle(dmx, dmy, 5.2, 1.0, 0.84, 0.14, 0.95, false)
            ui.drawCircle(dmx, dmy, 3.0, 1.0, 0.84, 0.14, 1.0, true)

            -- Player Supercar (Cyan), Speedboat (Emerald) + Walking Character (Bright White)
            local cmx, cmy = toMap(car.x, car.z)
            ui.drawCircle(cmx, cmy, 4.0, 0.14, 0.92, 1.0, 1.0, true)
            local bmx, bmy = toMap(boat.x, boat.z)
            ui.drawCircle(bmx, bmy, 3.6, 0.16, 0.95, 0.56, 1.0, true)
            if player.mode == "FOOT" then
                local pmx, pmy = toMap(player.x, player.z)
                ui.drawCircle(pmx, pmy, 3.6, 1.0, 1.0, 1.0, 1.0, true)
            end
            ui.drawRect(ox, oy, mapSize, mapSize, 0.35, 0.72, 0.95, 0.85, false, 2.0)
        end
        ui.endWindow()

        -- 2. COMPACT DRIVING / WALKING HUD (Bottom-Right!)
        local hudW, hudH = 316, 192
        if ui.beginWindow("HUD (H = Hide | F = Walk | G = Rain)", vpW - hudW - 14, vpH - hudH - 14, hudW, hudH, true) then
            if player.mode == "BOAT" then
                ui.text("Zone: Palm Marina Lagoon [SPEEDBOAT]")
                ui.text(string.format("Speed: %d knots   |   [F] Exit Boat", math.floor(math.abs(boat.speed) * 1.94)))
                ui.text("Controls: WASD Pilot | Shift Turbo | F Exit")
            elseif player.mode == "FOOT" then
                local modeStr = string.upper(player.currentAnim)
                ui.text("Zone: " .. biome .. " [" .. modeStr .. "]")
                local dBoat = math.sqrt((player.x - boat.x) ^ 2 + (player.z - boat.z) ^ 2)
                local dCar  = math.sqrt((player.x - car.x) ^ 2 + (player.z - car.z) ^ 2)
                if dBoat < 9.5 then
                    ui.text("Press [F] to Pilot Racing Speedboat!")
                elseif dCar < 8.0 then
                    ui.text("Press [F] to Enter " .. vehicles[selectedVehicleIdx].name)
                else
                    ui.text(string.format("Car: %dm | Boat: %dm (F to Board)", math.floor(dCar), math.floor(dBoat)))
                end
                ui.text("Weather: " .. weatherPresets[weatherPresetIdx].name .. " (G = Cycle)")
            else
                local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
                local kmh = math.floor(speed * 3.6)
                local statusTag = car.flooded and " [FLOODED]" or (car.inWater and " [SHALLOW WATER]" or "")
                ui.text("Zone: " .. biome .. statusTag)
                ui.text(string.format("Speed: %d km/h  |  G%d  |  %s", kmh, car.gear, weatherPresets[weatherPresetIdx].name))
                ui.progressBar(car.rpm, string.format("V12 RPM %d%%", math.floor(car.rpm * 100)))
                ui.progressBar(car.boostEnergy / 100.0, string.format("NITRO %d%%", math.floor(car.boostEnergy)))
            end
            ui.separator()
            ui.text("Next: " .. spot.label)
            ui.text(string.format("Score: %d (Best: %d) | Deliv: %d", score, bestScore, deliveriesDone))
        end
        ui.endWindow()
    end
end

function onShutdown()
    updateBestScore()
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
    sage.log("v1.5.0 AAA GPU Instancing, 3-CSM, Euphoria Ragdoll & Spline AI Sandbox shutting down cleanly.")
end
