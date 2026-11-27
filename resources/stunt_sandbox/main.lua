-- =============================================================================
-- SAGE Engine v0.9 Showcase: 560m Grand 4-Biome Open World Sandbox
--   * GTA-Style Exit / Enter Supercar (Press F to walk/sprint/swim on foot!)
--   * 6 Autonomous AI Traffic Cruisers patrolling the Mid & Coastal Ring Highways
--   * Patrolling Animated Helicopter, Spinning 24m Ferris Wheel & Rocking Sailboats
--   * Realistic Water Submersion & Engine Flooding (Car sinks; Driver can swim!)
--   * 2-Cascade Soft Shadows, HDR Bloom, 16 Nearest Dynamic Lights & 3D Particles
-- =============================================================================
-- CONTROLS:
--   W / S (or Up / Down)   : Drive / Brake (or Walk Forward / Back on foot)
--   A / D (or Left / Right): Steer (or Turn on foot)
--   F                      : Exit / Enter Supercar (Walk & Swim as 3D Character!)
--   Space                  : Handbrake Drift (or Jump on foot)
--   Left Shift             : Nitro Boost (or Sprint on foot)
--   Right-Mouse Drag       : Orbit 3D Camera
--   C                      : Cycle Camera (Far Chase -> Close Action -> Hood Cam)
--   X                      : Physics Shockwave (blasts nearby crates/balls/cones!)
--   B                      : Drop fresh 6-crate Jolt Physics pyramid ahead
--   T                      : Cycle Time of Day (Sunset -> Noon -> Starry Night -> Sunrise)
--   H                      : Hide / Show Cockpit HUD
--   Esc                    : Pause Menu (Main Menu hides completely on Play!)
--   F1                     : Toggle Developer Dear ImGui Inspector
--   Tab                    : Toggle Free-Fly 3D Inspector Camera
--   F5 / Ctrl+S            : Live Hot-Reload (JSON scene + Lua scripts)
-- =============================================================================

local CONFIG = {
    crateModel    = "assets/models/crate.glb",
    coneModel     = "assets/models/traffic_cone.glb",
    ballModel     = "assets/models/stunt_ball.glb",
    droneModel    = "assets/models/hover_drone.glb",
    heliModel     = "assets/models/helicopter.glb",
    charModel     = "assets/models/character.glb",
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
    { name = "Lamborghini Aventador SVJ", model = "assets/models/aventador_svj.glb" },
    { name = "SAGE Sports Coupe",         model = "assets/models/sports_car.glb" },
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

local gameState = "MENU" -- "MENU" | "PLAYING" | "PAUSED"
local showHud = true

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
}

local player = {
    onFoot = false,
    x = 3.0, y = 0.0, z = -8.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    yaw = 0.0,
    grounded = true,
    swimming = false,
    moving = false,
    splashAccum = 0.0,
}

-- 6 Autonomous AI Traffic Cruisers looping the Mid (±148m) & Outer Coastal (±236m) Highways!
local trafficCars = {
    { id = "ai_car_1", ring = 148.0, laneOffset =  3.4, dir =  1, pos =   0.0, speed = 18.0, color = { 0.90, 0.16, 0.18 }, x = 0, z = 0, yaw = 0 },
    { id = "ai_car_2", ring = 148.0, laneOffset = -3.4, dir = -1, pos = 420.0, speed = 16.5, color = { 0.14, 0.56, 0.96 }, x = 0, z = 0, yaw = 0 },
    { id = "ai_car_3", ring = 148.0, laneOffset =  3.4, dir =  1, pos = 760.0, speed = 19.5, color = { 0.96, 0.96, 0.98 }, x = 0, z = 0, yaw = 0 },
    { id = "ai_car_4", ring = 236.0, laneOffset =  3.8, dir =  1, pos = 120.0, speed = 22.0, color = { 0.16, 0.82, 0.42 }, x = 0, z = 0, yaw = 0 },
    { id = "ai_car_5", ring = 236.0, laneOffset = -3.8, dir = -1, pos = 880.0, speed = 21.0, color = { 1.00, 0.72, 0.14 }, x = 0, z = 0, yaw = 0 },
    { id = "ai_car_6", ring = 236.0, laneOffset =  3.8, dir =  1, pos = 1450.0, speed = 23.0, color = { 0.72, 0.76, 0.84 }, x = 0, z = 0, yaw = 0 },
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

local keyLatch = { T = false, C = false, X = false, B = false, R = false, H = false, F = false, Escape = false }

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

local function respawnCarAtBiome(idx)
    local sp = biomeSpawns[idx] or biomeSpawns[1]
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

    -- Playable 3rd-Person Character (.glb — hidden while driving, steps out on Press F!)
    world.spawnModel("player_char", CONFIG.charModel, car.x + 2.5, car.y, car.z)
    world.stopAnimation("player_char")
    world.setVisible("player_char", false)

    -- 6 Autonomous AI Traffic Cruisers (.glb)
    for _, tc in ipairs(trafficCars) do
        local tx, tz, tyaw = sampleSquareLoop(tc.ring, tc.laneOffset, tc.dir, tc.pos)
        tc.x, tc.z, tc.yaw = tx, tz, tyaw
        world.spawnModel(tc.id, CONFIG.trafficModel, tx, 0.07, tz)
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
    for idx, tc in ipairs(trafficCars) do
        tc.pos = tc.pos + tc.speed * tc.dir * dt
        local tx, tz, tyaw = sampleSquareLoop(tc.ring, tc.laneOffset, tc.dir, tc.pos)
        tc.x, tc.z, tc.yaw = tx, tz, tyaw
        world.setPosition(tc.id, tx, 0.07, tz)
        world.setRotation(tc.id, 0.0, tyaw, 0.0)

        -- Live headlight point lights on AI cruisers (GPU automatically selects nearest 16!)
        local trad = degToRad(tyaw)
        world.setPointLight(
            tc.id .. "_head",
            tx + math.sin(trad) * 4.2, 1.15, tz + math.cos(trad) * 4.2,
            1.0, 0.94, 0.80, 18.0, 1.9
        )

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
                    world.spawnParticles((car.x + tx) * 0.5, 0.9, (car.z + tz) * 0.5, 0.0, 4.5, 0.0, 1.0, 0.78, 0.28, 12, 0.18, 0.45, true, 14.0)
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
    spawnDynamicPropsAndPhysics()
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)

    sage.log("===================================================================")
    sage.log("  SAGE Engine v0.9 — 560m Grand 4-Biome Open World Sandbox")
    sage.log("  Press F anytime to Exit/Enter Supercar! (Walk, Sprint & Swim!)")
    sage.log("  Features: 2-Cascade Shadows | AI Traffic | Ferris Wheel | 24 Models")
    sage.log("===================================================================")
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

    -- Patrolling Animated Helicopter circling wide over Downtown & Palm Coast!
    local ha = timeNow * 0.14
    local hx = math.cos(ha) * 145.0
    local hz = math.sin(ha) * 145.0
    world.setPosition("patrol_heli", hx, 32.0 + math.sin(timeNow * 0.8) * 2.5, hz)
    world.setRotation("patrol_heli", 6.0, math.deg(-ha), -10.0)

    -- Sweeping Rotating Golden Lighthouse Beacon Light shimmering across the 72m Marina Lagoon!
    local lhAngle = timeNow * 0.85
    world.setPointLight(
        "lighthouse_beam",
        214.0 + math.cos(lhAngle) * 20.0, 8.5, -214.0 + math.sin(lhAngle) * 20.0,
        1.0, 0.86, 0.42, 42.0, 3.4
    )

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
            sage.setTitle("SAGE Engine v0.9 — Main Menu | 560m Grand Open World")
        else
            sage.setTitle("SAGE Engine v0.9 — PAUSED (Press Esc to Resume)")
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

    if keyJustPressed("C") then
        camPresetIdx = (camPresetIdx % #camPresets) + 1
        cam.orbitYaw, cam.orbitPitch = 0.0, 0.0
        sage.log("Camera View -> " .. camPresets[camPresetIdx].name)
    end

    if keyJustPressed("H") then
        showHud = not showHud
    end

    -- GTA-Style Press F: Exit / Enter Supercar!
    if keyJustPressed("F") then
        if not player.onFoot then
            local rad = degToRad(car.yaw)
            local sideX, sideZ = math.cos(rad), -math.sin(rad)
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
            world.setVisible("car_flame", false)
            audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
            audio.playChime()
            sage.log("Stepped out of vehicle! WASD Walk | Shift Sprint | Space Jump | F to Re-Enter Car")
        else
            local dx = player.x - car.x
            local dz = player.z - car.z
            if (dx * dx + dz * dz) < 7.5 * 7.5 then
                player.onFoot = false
                world.setVisible("player_char", false)
                audio.playChime()
                sage.log("Entered " .. vehicles[selectedVehicleIdx].name .. "!")
            else
                sage.log("Move closer to your car to enter (or press R to respawn with vehicle)!")
            end
        end
    end

    if keyJustPressed("X") then
        local fx = player.onFoot and player.x or car.x
        local fy = player.onFoot and player.y or car.y
        local fz = player.onFoot and player.z or car.z
        local hitCount = physics.applyExplosion(fx, fy + 0.5, fz, 28.0, 420.0)
        audio.playExplosion()
        world.spawnParticles(fx, fy + 0.8, fz, 0.0, 7.5, 0.0, 1.0, 0.65, 0.18, 22, 0.34, 0.75, true, 12.0)
        sage.log("SHOCKWAVE! Blasted " .. hitCount .. " Jolt Physics objects!")
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

        player.yaw = player.yaw + turnInput * 155.0 * dt
        local prad = degToRad(player.yaw)
        local pfwdX, pfwdZ = math.sin(prad), math.cos(prad)
        local sprinting = input.isKeyDown("LeftShift") or input.isKeyDown("RightShift")
        local walkSpeed = player.swimming and 4.4 or (sprinting and 11.2 or 6.4)

        player.vx = lerp(player.vx, pfwdX * moveF * walkSpeed, 12.0 * dt)
        player.vz = lerp(player.vz, pfwdZ * moveF * walkSpeed, 12.0 * dt)
        player.x = clamp(player.x + player.vx * dt, -CONFIG.arenaHalfSize, CONFIG.arenaHalfSize)
        player.z = clamp(player.z + player.vz * dt, -CONFIG.arenaHalfSize, CONFIG.arenaHalfSize)

        -- Surface + Water Check (Driver can swim smoothly in the Lagoon!)
        local onSurf, surfY, rampH = physics.sampleRampSurface(player.x, player.z)
        local overDeck = onSurf and (rampH <= 0.01) and (surfY > 0.10) and (player.y >= surfY - 0.55)
        local inW, waveY, seaY, wDepth = world.sampleWater(player.x, player.z)
        local groundY = (inW and not overDeck) and seaY or surfY

        player.swimming = inW and (not overDeck) and (wDepth >= 0.65)
        if player.swimming then
            local swimTargetY = waveY - 0.85
            player.y = lerp(player.y, swimTargetY, 6.0 * dt)
            player.vy = 0.0
            player.grounded = true
            player.splashAccum = player.splashAccum + dt
            if math.abs(moveF) > 0.1 and player.splashAccum >= 0.12 then
                player.splashAccum = 0.0
                world.spawnParticles(player.x, waveY + 0.05, player.z, 0.0, 2.2, 0.0, 0.84, 0.96, 1.0, 4, 0.16, 0.45, false, 12.0)
            end
        else
            if player.grounded and input.isKeyDown("Space") then
                player.vy = 9.2
                player.grounded = false
            end
            if not player.grounded then
                player.vy = player.vy - 24.0 * dt
                player.y = player.y + player.vy * dt
                if player.y <= groundY then
                    player.y = groundY
                    player.vy = 0.0
                    player.grounded = true
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

        -- Toggle walking glTF animation based on movement speed
        local pSpd = math.sqrt(player.vx * player.vx + player.vz * player.vz)
        if pSpd > 0.45 then
            world.playAnimation("player_char", "", true, sprinting and 2.1 or 1.35)
        else
            world.stopAnimation("player_char")
        end
        world.setPosition("player_char", player.x, player.y, player.z)
        world.setRotation("player_char", player.swimming and 28.0 or 0.0, player.yaw, 0.0)

        checkPhysicsCrateSmashes(dt)
        updateDeliveryMission(dt, timeNow, player.x, player.y, player.z)

        -- Smooth Over-The-Shoulder 3rd-Person Character Camera
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
        local celev = math.sin(degToRad(cam.orbitPitch)) * 6.0
        cam.x = lerp(cam.x, player.x - cfx * 6.2, 8.0 * dt)
        cam.y = lerp(cam.y, math.max(0.9, player.y + 2.8 + celev), 8.0 * dt)
        cam.z = lerp(cam.z, player.z - cfz * 6.2, 8.0 * dt)
        cam.tx = lerp(cam.tx, player.x + cfx * 2.0, 14.0 * dt)
        cam.ty = lerp(cam.ty, player.y + 1.45, 14.0 * dt)
        cam.tz = lerp(cam.tz, player.z + cfz * 2.0, 14.0 * dt)
        cam.fov = lerp(cam.fov, 60.0, 6.0 * dt)
        camera.setPosition(cam.x, cam.y, cam.z)
        camera.setTarget(cam.tx, cam.ty, cam.tz)
        camera.setFov(cam.fov)

        titleUpdateAccum = titleUpdateAccum + dt
        if titleUpdateAccum >= 0.25 then
            titleUpdateAccum = 0.0
            local biome = getCurrentBiomeName(player.x, player.z)
            sage.setTitle(string.format("SAGE v0.9 | %s | ON FOOT (F = Enter Car) | Score: %d", biome, score))
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

        local grip = car.isDrifting and CONFIG.driftGrip or CONFIG.normalGrip
        lateralVel = lateralVel * math.exp(-grip * dt)

        car.vx = fwdX * forwardVel + leftX * lateralVel
        car.vz = fwdZ * forwardVel + leftZ * lateralVel
    else
        car.yaw = car.yaw + car.steerSmooth * 38.0 * dt
    end

    local prevY = car.y
    car.x = car.x + car.vx * dt
    car.z = car.z + car.vz * dt

    -- Unified Surface Elevation (12 Stunt Ramps + 32m Drivable Pier) + Realistic Water Physics!
    local onSurface, surfY, rampHeight = physics.sampleRampSurface(car.x, car.z)
    local isElevatedDeck = onSurface and (rampHeight <= 0.01) and (surfY > 0.10)
    local isRamp = onSurface and (rampHeight > 0.05)

    local inWaterVol, waveSurfaceY, seabedY = world.sampleWater(car.x, car.z)
    local overPier = isElevatedDeck and (car.y >= surfY - 0.45)
    local groundY = (inWaterVol and not overPier) and seabedY or surfY

    if car.grounded then
        if isRamp and groundY > 0.05 then
            local climbRate = (groundY - prevY) / math.max(dt, 0.001)
            car.y = groundY
            car.vy = math.max(climbRate, 0.0)
            car.pitch = lerp(car.pitch, -17.5, 14.0 * dt)
            if groundY > rampHeight * 0.78 and math.abs(forwardVel) > 10.0 then
                car.vy = math.max(car.vy, math.abs(forwardVel) * 0.44 + 4.8)
                car.boostTimer = math.max(car.boostTimer, 0.55)
                car.grounded = false
            end
        elseif overPier then
            car.y = lerp(car.y, surfY, 18.0 * dt)
            car.vy = 0.0
            car.pitch = lerp(car.pitch, accelPitchTarget, 9.0 * dt)
        else
            if car.y > groundY + 0.18 then
                car.grounded = false
            else
                car.y = groundY
                car.vy = 0.0
                car.pitch = lerp(car.pitch, accelPitchTarget, 9.0 * dt)
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
            car.y = groundY
            car.vy = 0.0
            car.grounded = true
            if car.airTime >= 0.42 and not (inWaterVol and not overPier) then
                stuntJumps = stuntJumps + 1
                local airBonus = math.floor(car.airTime * 150.0)
                score = score + airBonus
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
        local targetRoll = clamp(lateralVel * 0.95 + car.steerSmooth * speedNorm * 2.5, -8.5, 8.5)
        car.roll = lerp(car.roll, targetRoll, 9.0 * dt)
    end

    physics.moveKinematic("player_car", car.x, car.y + 0.65, car.z, car.pitch, car.yaw, car.roll, dt)
    checkPhysicsCrateSmashes(dt)
    updateDeliveryMission(dt, timeNow, car.x, car.y, car.z)

    world.setPosition("player_car", car.x, car.y, car.z)
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

    -- 6-Gear Transmission & V12 Synth
    local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    local gearSpan = CONFIG.boostSpeed / 6.0
    car.gear = clamp(math.floor(speed / gearSpan) + 1, 1, 6)
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
            "SAGE v0.9 | %s | G%d %d km/h | F = Walk | Score: %d | Esc = Menu",
            biome, car.gear, kmh, score
        ))
    end
end

-- -----------------------------------------------------------------------------
-- Main Menu, Pause Menu & Compact HUD (All Menus Hide Cleanly During Gameplay!)
-- -----------------------------------------------------------------------------
function onRenderUI()
    local vpW, vpH = ui.getViewportSize()

    if gameState == "MENU" then
        if ui.beginCenteredWindow("SAGE ENGINE v0.9 — 560m GRAND OPEN WORLD", 460, 405) then
            ui.setFontScale(1.35)
            ui.textCentered("SAGE STUNT & DELIVERY")
            ui.setFontScale(1.0)
            ui.textCentered("2-Cascade Shadows  |  Drive & Walk (F)  |  AI Traffic  |  24 Models")
            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("PLAY GAME", 440, 44) then
                respawnCarAtBiome(selectedBiomeIdx)
                applySelectedVehicle()
                audio.playChime()
                gameState = "PLAYING"
            end

            ui.spacing()
            ui.separator()
            ui.spacing()

            ui.text("Vehicle: " .. vehicles[selectedVehicleIdx].name)
            if ui.button("Switch Vehicle", 215, 30) then
                selectedVehicleIdx = (selectedVehicleIdx % #vehicles) + 1
                applySelectedVehicle()
            end

            ui.spacing()
            ui.text("Starting Biome (560m x 560m Island): " .. biomeSpawns[selectedBiomeIdx].name)
            if ui.button("Downtown", 104, 28) then selectedBiomeIdx = 1; respawnCarAtBiome(1) end
            ui.sameLine()
            if ui.button("Highlands", 104, 28) then selectedBiomeIdx = 2; respawnCarAtBiome(2) end
            ui.sameLine()
            if ui.button("Palm Marina", 104, 28) then selectedBiomeIdx = 3; respawnCarAtBiome(3) end
            ui.sameLine()
            if ui.button("Red Desert", 104, 28) then selectedBiomeIdx = 4; respawnCarAtBiome(4) end

            ui.spacing()
            ui.text("Sky & Lighting: " .. timePresets[timePresetIdx].name)
            if ui.button("Sunset", 104, 28) then timePresetIdx = 1; world.setTimeOfDay(17.4) end
            ui.sameLine()
            if ui.button("Noon", 104, 28) then timePresetIdx = 2; world.setTimeOfDay(12.5) end
            ui.sameLine()
            if ui.button("Starry Night", 104, 28) then timePresetIdx = 3; world.setTimeOfDay(22.5) end
            ui.sameLine()
            if ui.button("Sunrise", 104, 28) then timePresetIdx = 4; world.setTimeOfDay(6.4) end

            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("Quit to Desktop", 440, 32) then
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
        local hudW, hudH = 310, 186
        if ui.beginWindow("HUD (H = Hide | F = Walk/Drive | Esc = Menu)", vpW - hudW - 14, vpH - hudH - 14, hudW, hudH, true) then
            if player.onFoot then
                local modeStr = player.swimming and "SWIMMING" or "ON FOOT"
                ui.text("Zone: " .. biome .. " [" .. modeStr .. "]")
                local dCar = math.sqrt((player.x - car.x) ^ 2 + (player.z - car.z) ^ 2)
                if dCar < 7.5 then
                    ui.text("Press [F] to Enter " .. vehicles[selectedVehicleIdx].name)
                else
                    ui.text(string.format("Car Distance: %d m  (Walk back or press R)", math.floor(dCar)))
                end
                ui.text("Controls: WASD Walk | Shift Sprint | Space Jump")
            else
                local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
                local kmh = math.floor(speed * 3.6)
                local statusTag = car.flooded and " [FLOODED]" or (car.inWater and " [SHALLOW WATER]" or "")
                ui.text("Zone: " .. biome .. statusTag)
                ui.text(string.format("Speed: %d km/h   |   Gear: G%d   |   [F] Exit Car", kmh, car.gear))
                ui.progressBar(car.rpm, string.format("V12 RPM %d%%", math.floor(car.rpm * 100)))
                ui.progressBar(car.boostEnergy / 100.0, string.format("NITRO %d%%", math.floor(car.boostEnergy)))
            end
            ui.separator()
            ui.text("Next: " .. spot.label)
            ui.text(string.format("Score: %d | Deliveries: %d | Jumps: %d", score, deliveriesDone, stuntJumps))
        end
        ui.endWindow()
    end
end

function onShutdown()
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
    sage.log("v0.9 560m Grand Open World Sandbox shutting down cleanly.")
end
