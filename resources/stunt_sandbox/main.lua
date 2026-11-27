-- =============================================================================
-- SAGE Engine v0.8 Showcase: Dynamic HDR 4-Biome Open World Sandbox
--   * Realistic Water Submersion & Engine Flooding (no driving on water!)
--   * Real-Time Car Headlights, Nitro Glow & Rotating Lighthouse Point Light
--   * 3D Particle FX (Water Splashes, Tire Drift Smoke, Nitro Sparks)
-- =============================================================================
-- BIOMES TO EXPLORE (320m x 320m Open World):
--   NW: Downtown Metro       | NE: Pine Highlands & Wind Ridge
--   SE: Palm Coast & Beach   | SW: Red Rock Desert Canyon
--
-- CONTROLS:
--   W / S (or Up / Down)   : Accelerate / Brake & Reverse (6-Gear Auto!)
--   A / D (or Left / Right): Speed-sensitive Steering
--   Space                  : Handbrake Drift
--   Left Shift             : Nitro Boost
--   Right-Mouse Drag       : Orbit 3D Chase Camera
--   C                      : Cycle Camera (Far Chase -> Close Action -> Hood Cam)
--   X                      : Physics Shockwave (blasts nearby crates/balls/cones!)
--   B                      : Drop fresh 6-crate Jolt Physics pyramid ahead
--   T                      : Cycle Time of Day
--   H                      : Hide / Show Cockpit HUD
--   Esc                    : Open / Close Pause Menu (Main Menu hides on Play!)
--   F1                     : Toggle Developer Dear ImGui Inspector
--   Tab                    : Toggle Free-Fly 3D Inspector Camera
--   F5 / Ctrl+S            : Live Hot-Reload (JSON scene + Lua scripts)
-- =============================================================================

local CONFIG = {
    crateModel    = "assets/models/crate.glb",
    coneModel     = "assets/models/traffic_cone.glb",
    ballModel     = "assets/models/stunt_ball.glb",
    droneModel    = "assets/models/hover_drone.glb",
    maxSpeed      = 36.0,   -- Fast highway top speed for the larger 320m open world!
    boostSpeed    = 52.0,   -- Nitro highway cruise (~187 km/h)
    acceleration  = 29.0,
    brakeForce    = 40.0,
    reverseAccel  = 19.0,
    lowSpeedTurn  = 160.0,
    highSpeedTurn = 82.0,
    normalGrip    = 9.5,
    driftGrip     = 2.2,
    gravity       = 26.0,
    arenaHalfSize = 152.0,
}

local vehicles = {
    { name = "Lamborghini Aventador SVJ", model = "assets/models/aventador_svj.glb" },
    { name = "SAGE Sports Coupe",         model = "assets/models/sports_car.glb" },
}
local selectedVehicleIdx = 1

local biomeSpawns = {
    { name = "Downtown Metro",    x =   0.0, z =  -6.0, yaw =   0.0 },
    { name = "Pine Highlands",    x =  54.0, z =  54.0, yaw =  45.0 },
    { name = "Palm Coast Beach",  x =  54.0, z = -54.0, yaw = 135.0 },
    { name = "Red Rock Desert",   x = -54.0, z = -54.0, yaw = -135.0 },
}
local selectedBiomeIdx = 1

local timePresets = {
    { hour = 17.4, name = "Golden Sunset" },
    { hour = 12.5, name = "Bright Noon" },
    { hour = 22.5, name = "Neon Night" },
    { hour =  6.4, name = "Early Sunrise" },
}
local timePresetIdx = 1

-- Game UI state: starts cleanly on Main Menu; hides completely once you press PLAY!
local gameState = "MENU" -- "MENU" | "PLAYING" | "PAUSED"
local showHud = true

-- -----------------------------------------------------------------------------
-- Vehicle, Camera & Mission State
-- -----------------------------------------------------------------------------
local car = {
    x = 0.0, y = 0.0, z = -6.0,
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
    lastDryZ = -6.0,
    lastDryYaw = 0.0,
    fxAccum = 0.0,
    gear = 1,
    rpm = 0.12,
}

local camPresets = {
    { name = "Far Chase",    dist = 10.6, height = 4.3, lookAhead = 5.5, fov = 60.0 },
    { name = "Close Action", dist =  7.4, height = 2.9, lookAhead = 6.5, fov = 66.0 },
    { name = "Hood Cam",     dist = -0.7, height = 1.48, lookAhead = 18.0, fov = 72.0 },
}
local camPresetIdx = 1
local cam = {
    x = 10.0, y = 5.5, z = -14.0,
    tx = 0.0, ty = 1.2, tz = -6.0,
    fov = 60.0,
    orbitYaw = 0.0,
    orbitPitch = 0.0,
}

-- Cross-Biome Delivery Destinations across the entire 320m Open World!
local deliverySpots = {
    { x =  -54.0, z =   54.0, label = "Downtown Metro Plaza" },
    { x =   54.0, z =   54.0, label = "Pine Highlands Lodge" },
    { x =   54.0, z =  -54.0, label = "Palm Coast Boardwalk" },
    { x =  -54.0, z =  -54.0, label = "Red Rock Outpost" },
    { x =    0.0, z =  120.0, label = "North Scenic Highway" },
    { x =  120.0, z =    0.0, label = "East Ridge Expressway" },
    { x =    0.0, z = -120.0, label = "South Coast Highway" },
    { x = -120.0, z =    0.0, label = "West Canyon Highway" },
    { x =  -86.0, z =   46.0, label = "Metro Delivery Depot" },
    { x =   62.0, z =  -88.0, label = "Palm Lagoon Pier" },
    { x =  -86.0, z =  -72.0, label = "Desert Wind Depot" },
    { x =   86.0, z =   72.0, label = "Alpine Forestry Depot" },
}

local keyLatch = { T = false, C = false, X = false, B = false, R = false, H = false, Escape = false }

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
        return "Palm Coast Beach"
    else
        return "Red Rock Desert"
    end
end

local function respawnCarAtBiome(idx)
    local sp = biomeSpawns[idx] or biomeSpawns[1]
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

-- -----------------------------------------------------------------------------
-- Dynamic Props & Physics Setup (Scene colliders are auto-loaded from JSON!)
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
    -- Dashed yellow road markings cleanly layered above NS (0.04) and EW (0.07) asphalt (ZERO Z-fighting!)
    local dashId = 0
    for d = -144, 144, 10 do
        if math.abs(d) > 8 and math.abs(math.abs(d) - 28.0) > 6 and math.abs(math.abs(d) - 54.0) > 6 and math.abs(math.abs(d) - 120.0) > 7 then
            dashId = dashId + 1
            local idNS = "dash_ns_" .. dashId
            world.spawnPlane(idNS, 0.0, 0.06, d)
            world.setScale(idNS, 0.35, 1.0, 4.2)
            world.setColor(idNS, 0.96, 0.78, 0.16)

            local idEW = "dash_ew_" .. dashId
            world.spawnPlane(idEW, d, 0.09, 0.0)
            world.setScale(idEW, 4.2, 1.0, 0.35)
            world.setColor(idEW, 0.96, 0.78, 0.16)
        end
    end

    -- Player Vehicle (.glb) + Kinematic Jolt Hull
    applySelectedVehicle()
    physics.addKinematicBox("player_car", car.x, car.y + 0.65, car.z, 1.05, 0.62, 2.25)

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
    world.setScale("delivery_ring", 7.0, 5.2, 7.0)
    world.setColor("delivery_ring", 1.0, 0.82, 0.14)
    world.setEmissive("delivery_ring", true)

    -- Patrolling Animated Blender Hover Drones over Downtown & Beach!
    world.spawnModel("drone_1", CONFIG.droneModel, 14.0, 8.0, 0.0)
    world.playAnimation("drone_1", "", true, 1.2)
    world.spawnModel("drone_2", CONFIG.droneModel, 64.0, 11.0, -64.0)
    world.playAnimation("drone_2", "", true, 1.4)

    -- 4 Stacked 3-tier Jolt Physics Crate Pyramids (24 tumbling crates across the biomes!)
    crates = {}
    spawnCratePyramid(  0.0,  54.0,  0.0)
    spawnCratePyramid( 54.0,   0.0, 90.0)
    spawnCratePyramid(-54.0,   0.0, 90.0)
    spawnCratePyramid(  0.0, -54.0,  0.0)

    -- 4 Giant Neon Rocket-League Jolt Physics Stunt Balls!
    nextBallSeq = 0
    for _, bp in ipairs({ { 0.0, 3.0, 10.0 }, { 54.0, 3.0, -54.0 }, { -54.0, 3.0, -54.0 }, { 54.0, 3.0, 54.0 } }) do
        nextBallSeq = nextBallSeq + 1
        local id = "stunt_ball_" .. nextBallSeq
        world.spawnModel(id, CONFIG.ballModel, bp[1], bp[2], bp[3])
        physics.addDynamicSphere(id, bp[1], bp[2], bp[3], 1.25, 32.0, 0.72, 0.35)
    end

    -- 8 Knockable Capsule Traffic Cones around central hub
    local coneSpots = {
        { -4.5,  12.0 }, {  4.5,  12.0 }, { -4.5, -12.0 }, {  4.5, -12.0 },
        { 12.0,   4.5 }, { 12.0,  -4.5 }, { -12.0,  4.5 }, { -12.0, -4.5 },
    }
    for i, cp in ipairs(coneSpots) do
        local id = "cone_" .. i
        world.spawnModel(id, CONFIG.coneModel, cp[1], 0.50, cp[2])
        physics.addDynamicCapsule(id, cp[1], 0.50, cp[2], 0.24, 0.26, 6.0, 0.28, 0.65)
    end
end

-- -----------------------------------------------------------------------------
-- Collisions, Crates & Mission Loop
-- -----------------------------------------------------------------------------
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

local function updateDeliveryMission(dt, timeNow)
    deliveryTimer = deliveryTimer + dt
    local spot = deliverySpots[activeDeliveryIdx]

    local ringRadius = 6.8 + 0.8 * math.sin(timeNow * 4.5)
    world.setScale("delivery_ring", ringRadius, 5.2, ringRadius)
    world.setRotation("delivery_ring", 0.0, timeNow * 55.0, 0.0)

    local dx = spot.x - car.x
    local dz = spot.z - car.z
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist > 0.05 then
        local bob = 0.22 * math.sin(timeNow * 5.0)
        world.setPosition("nav_arrow", car.x, car.y + 2.55 + bob, car.z)
        world.setRotation("nav_arrow", 0.0, math.atan(dx, dz) * (180.0 / math.pi), 0.0)
    end

    if dist < 5.0 then
        deliveriesDone = deliveriesDone + 1
        local speedBonus = math.floor(math.max(0.0, 30.0 - deliveryTimer) * 10.0)
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
    sage.log("  SAGE Engine v0.8 — Dynamic HDR Open World Stunt & Delivery Sandbox")
    sage.log("  Features: Realistic Water Physics | Dynamic Lights | 3D Particles")
    sage.log("  Click PLAY on the Main Menu to drive (Esc = Pause Menu, F1 = Editor)")
    sage.log("===================================================================")
end

function onTick(dt)
    local timeNow = sage.getTime()

    -- Always animate patrolling drones so the world feels alive even behind the Main Menu!
    local d1a = timeNow * 0.42
    world.setPosition("drone_1", math.cos(d1a) * 18.0, 8.2, math.sin(d1a) * 18.0)
    world.setRotation("drone_1", 0.0, math.deg(-d1a), 0.0)

    local d2a = -timeNow * 0.30 + 1.2
    world.setPosition("drone_2", 64.0 + math.cos(d2a) * 20.0, 11.0, -64.0 + math.sin(d2a) * 20.0)
    world.setRotation("drone_2", 0.0, math.deg(-d2a + math.pi), 0.0)

    -- Sweeping Rotating Golden Lighthouse Beacon Light shimmering across the Palm Lagoon waves!
    local lhAngle = timeNow * 0.85
    world.setPointLight(
        "lighthouse_beam",
        102.0 + math.cos(lhAngle) * 16.0, 7.5, -102.0 + math.sin(lhAngle) * 16.0,
        1.0, 0.86, 0.42, 34.0, 3.2
    )

    -- Toggle Pause Menu on Escape
    if keyJustPressed("Escape") then
        if gameState == "PLAYING" then
            gameState = "PAUSED"
            audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
        elseif gameState == "PAUSED" then
            gameState = "PLAYING"
        end
    end

    -- Cinematic orbiting showcase camera while in Main Menu or Pause Menu
    if gameState ~= "PLAYING" then
        audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
        world.setVisible("nav_arrow", false)
        world.setVisible("car_flame", false)

        if gameState == "MENU" then
            local orbit = timeNow * 0.28
            camera.setPosition(car.x + math.sin(orbit) * 10.5, car.y + 3.8, car.z + math.cos(orbit) * 10.5)
            camera.setTarget(car.x, car.y + 1.1, car.z)
            camera.setFov(55.0)
            sage.setTitle("SAGE Engine v0.8 — Main Menu | Dynamic HDR Open World")
        else
            sage.setTitle("SAGE Engine v0.8 — PAUSED (Press Esc to Resume)")
        end
        return
    end

    world.setVisible("nav_arrow", true)

    -- One-shot gameplay hotkeys
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

    if keyJustPressed("X") then
        local hitCount = physics.applyExplosion(car.x, car.y + 0.5, car.z, 26.0, 400.0)
        audio.playExplosion()
        world.spawnParticles(car.x, car.y + 0.8, car.z, 0.0, 7.5, 0.0, 1.0, 0.65, 0.18, 22, 0.34, 0.75, true, 12.0)
        sage.log("SHOCKWAVE! Blasted " .. hitCount .. " Jolt Physics objects!")
    end

    if keyJustPressed("B") then
        local rad = degToRad(car.yaw)
        local sx = clamp(car.x + math.sin(rad) * 14.0, -144.0, 144.0)
        local sz = clamp(car.z + math.cos(rad) * 14.0, -144.0, 144.0)
        spawnCratePyramid(sx, sz, car.yaw)
        audio.playChime()
    end

    if keyJustPressed("R") then
        respawnCarAtBiome(selectedBiomeIdx)
    end

    -- Driving inputs (disabled automatically if the car's engine is flooded underwater!)
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
        car.boostEnergy = math.max(0.0, car.boostEnergy - 34.0 * dt)
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
        forwardVel = clamp(forwardVel, -14.0, topSpeed)

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

    -- Unified C++ Surface Elevation (Ramps + Drivable Pier Decks) + Realistic Water Physics!
    local onSurface, surfY, rampHeight = physics.sampleRampSurface(car.x, car.z)
    local isElevatedDeck = onSurface and (rampHeight <= 0.01) and (surfY > 0.10)
    local isRamp = onSurface and (rampHeight > 0.05)

    local inWaterVol, waveSurfaceY, seabedY, waterDepth = world.sampleWater(car.x, car.z)
    -- If driving on top of the wooden pier deck, the car stays dry above the water!
    local overPier = isElevatedDeck and (car.y >= surfY - 0.45)
    local groundY = surfY
    if inWaterVol and not overPier then
        groundY = seabedY
    end

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

    -- Realistic Water Interaction: Hydrodynamic Drag, 3D Splashes & Engine Flooding (NO driving on water!)
    local wasInWater = car.inWater
    car.inWater = inWaterVol and (not overPier) and (car.y < waveSurfaceY + 0.12)
    car.waterDepth = car.inWater and math.max(0.0, waveSurfaceY - car.y) or 0.0

    if not inWaterVol and car.grounded and not car.flooded then
        car.lastDryX, car.lastDryZ, car.lastDryYaw = car.x, car.z, car.yaw
    end

    if car.inWater then
        local spdNow = math.sqrt(car.vx * car.vx + car.vz * car.vz)
        -- First splash impact burst when hitting the water!
        if not wasInWater then
            audio.playImpact(clamp(0.45 + spdNow * 0.03, 0.45, 1.0))
            world.spawnParticles(car.x, waveSurfaceY + 0.15, car.z, car.vx * 0.25, 5.2 + spdNow * 0.18, car.vz * 0.25, 0.82, 0.94, 1.0, 24, 0.34, 0.85, false, 15.0)
        end

        -- Heavy hydrodynamic water drag slows vehicle sharply in water
        local dragFactor = math.exp(-(3.8 + car.waterDepth * 4.5) * dt)
        car.vx = car.vx * dragFactor
        car.vz = car.vz * dragFactor

        -- Continuous wheel spray droplets in shallow water
        car.fxAccum = car.fxAccum + dt
        if spdNow > 1.2 and car.fxAccum >= 0.07 then
            car.fxAccum = 0.0
            world.spawnParticles(car.x, waveSurfaceY + 0.08, car.z, -car.vx * 0.15, 2.8 + spdNow * 0.14, -car.vz * 0.15, 0.86, 0.96, 1.0, 5, 0.22, 0.55, false, 14.0)
        end

        -- Deep water (>= 0.45m submersion): ENGINE FLOODS & SINKS!
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
        -- Nose-heavy sinking tilt & gentle wave bobbing while flooded underwater
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

    -- Unified C++ Solid Collision vs EVERY Building, Tree, Wind Turbine, Street Lamp, Rock & Lighthouse!
    local hitObs, rx, rz, rvx, rvz = physics.resolveCircleXZ(car.x, car.y, car.z, 1.55, car.vx, car.vz, 0.32)
    if hitObs then
        local preSpeed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
        car.x, car.z, car.vx, car.vz = rx, rz, rvx, rvz
        if preSpeed > 4.0 and impactSoundCooldown <= 0.0 then
            audio.playImpact(clamp(preSpeed / 24.0, 0.3, 1.0))
            impactSoundCooldown = 0.22
        end
    end

    -- Keep inside outer map bounds cleanly
    local S = CONFIG.arenaHalfSize
    car.x = clamp(car.x, -S, S)
    car.z = clamp(car.z, -S, S)

    if not car.flooded then
        local targetRoll = clamp(lateralVel * 0.95 + car.steerSmooth * speedNorm * 2.5, -8.5, 8.5)
        car.roll = lerp(car.roll, targetRoll, 9.0 * dt)
    end

    -- Sync Jolt Kinematic Car Hull & visual model
    physics.moveKinematic("player_car", car.x, car.y + 0.65, car.z, car.pitch, car.yaw, car.roll, dt)
    checkPhysicsCrateSmashes(dt)
    updateDeliveryMission(dt, timeNow)

    world.setPosition("player_car", car.x, car.y, car.z)
    world.setRotation("player_car", car.pitch, car.yaw, car.roll)

    if car.isBoosting then
        local flicker = 1.65 + 0.55 * math.sin(timeNow * 48.0)
        world.setVisible("car_flame", true)
        world.setPosition("car_flame", car.x - fwdX * 2.7, car.y + 0.42, car.z - fwdZ * 2.7)
        world.setRotation("car_flame", car.pitch, car.yaw + 180.0, 0.0)
        world.setScale("car_flame", 0.85, 0.6, flicker)
        -- HDR glowing Nitro sparks!
        world.spawnParticles(car.x - fwdX * 2.6, car.y + 0.35, car.z - fwdZ * 2.6, -fwdX * 6.0, 1.4, -fwdZ * 6.0, 0.18, 0.88, 1.0, 2, 0.16, 0.35, true, 6.0)
    else
        world.setVisible("car_flame", false)
    end

    -- Real-Time Dynamic Car Headlight Beam & Tail/Nitro Underglow Point Lights!
    local headPower = car.flooded and 0.4 or 2.8
    world.setPointLight(
        "car_headlight",
        car.x + fwdX * 5.8, car.y + 1.35, car.z + fwdZ * 5.8,
        1.0, 0.94, 0.82, 24.0, headPower
    )
    if car.isBoosting then
        world.setPointLight("car_tail", car.x - fwdX * 2.8, car.y + 0.8, car.z - fwdZ * 2.8, 0.15, 0.86, 1.0, 15.0, 2.6)
    elseif throttle < 0.0 and not car.flooded then
        world.setPointLight("car_tail", car.x - fwdX * 2.6, car.y + 0.8, car.z - fwdZ * 2.6, 1.0, 0.12, 0.10, 12.0, 2.0)
    else
        world.setPointLight("car_tail", car.x - fwdX * 2.6, car.y + 0.8, car.z - fwdZ * 2.6, 0.95, 0.15, 0.12, 8.0, 0.7)
    end

    -- 6-Gear Transmission & V12 Engine Synth (cuts out cleanly when flooded!)
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
    local desiredFov = preset.fov + clamp(speed * 0.30, 0.0, 14.0) + (car.isBoosting and 6.0 or 0.0)

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
            "SAGE v0.8 | %s | G%d %d km/h | Score: %d | Esc = Menu | F1 = Editor",
            biome, car.gear, kmh, score
        ))
    end
end

-- -----------------------------------------------------------------------------
-- Main Menu, Pause Menu & Compact Driving HUD (Hidden Cleanly During Gameplay!)
-- -----------------------------------------------------------------------------
function onRenderUI()
    local vpW, vpH = ui.getViewportSize()

    -- 1. MAIN MENU (hides completely the moment you click PLAY!)
    if gameState == "MENU" then
        if ui.beginCenteredWindow("SAGE ENGINE v0.8 — DYNAMIC HDR OPEN WORLD", 440, 390) then
            ui.setFontScale(1.35)
            ui.textCentered("SAGE STUNT & DELIVERY")
            ui.setFontScale(1.0)
            ui.textCentered("Water Physics  |  Dynamic Lights  |  3D Particles  |  HDR Bloom")
            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("PLAY GAME", 420, 44) then
                respawnCarAtBiome(selectedBiomeIdx)
                applySelectedVehicle()
                audio.playChime()
                gameState = "PLAYING"
            end

            ui.spacing()
            ui.separator()
            ui.spacing()

            ui.text("Vehicle: " .. vehicles[selectedVehicleIdx].name)
            if ui.button("Switch Vehicle", 205, 30) then
                selectedVehicleIdx = (selectedVehicleIdx % #vehicles) + 1
                applySelectedVehicle()
            end

            ui.spacing()
            ui.text("Starting Biome: " .. biomeSpawns[selectedBiomeIdx].name)
            if ui.button("Downtown", 99, 28) then selectedBiomeIdx = 1; respawnCarAtBiome(1) end
            ui.sameLine()
            if ui.button("Highlands", 99, 28) then selectedBiomeIdx = 2; respawnCarAtBiome(2) end
            ui.sameLine()
            if ui.button("Palm Beach", 99, 28) then selectedBiomeIdx = 3; respawnCarAtBiome(3) end
            ui.sameLine()
            if ui.button("Red Desert", 99, 28) then selectedBiomeIdx = 4; respawnCarAtBiome(4) end

            ui.spacing()
            ui.text("Sky & Lighting: " .. timePresets[timePresetIdx].name)
            if ui.button("Sunset", 99, 28) then timePresetIdx = 1; world.setTimeOfDay(17.4) end
            ui.sameLine()
            if ui.button("Noon", 99, 28) then timePresetIdx = 2; world.setTimeOfDay(12.5) end
            ui.sameLine()
            if ui.button("Night", 99, 28) then timePresetIdx = 3; world.setTimeOfDay(22.5) end
            ui.sameLine()
            if ui.button("Sunrise", 99, 28) then timePresetIdx = 4; world.setTimeOfDay(6.4) end

            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("Quit to Desktop", 420, 32) then
                sage.quit()
            end
        end
        ui.endWindow()
        return
    end

    -- 2. PAUSE MENU (Esc while driving)
    if gameState == "PAUSED" then
        if ui.beginCenteredWindow("PAUSED", 340, 290) then
            ui.setFontScale(1.25)
            ui.textCentered("GAME PAUSED")
            ui.setFontScale(1.0)
            ui.spacing()
            ui.separator()
            ui.spacing()

            if ui.button("Resume Driving (Esc)", 320, 38) then
                gameState = "PLAYING"
            end
            ui.spacing()
            if ui.button("Teleport to Next Biome (" .. biomeSpawns[(selectedBiomeIdx % #biomeSpawns) + 1].name .. ")", 320, 32) then
                selectedBiomeIdx = (selectedBiomeIdx % #biomeSpawns) + 1
                respawnCarAtBiome(selectedBiomeIdx)
                gameState = "PLAYING"
            end
            ui.spacing()
            if ui.button("Switch Vehicle", 320, 32) then
                selectedVehicleIdx = (selectedVehicleIdx % #vehicles) + 1
                applySelectedVehicle()
            end
            ui.spacing()
            if ui.button("Return to Main Menu", 320, 32) then
                gameState = "MENU"
            end
            ui.spacing()
            if ui.button("Quit to Desktop", 320, 32) then
                sage.quit()
            end
        end
        ui.endWindow()
        return
    end

    -- 3. ENGINE FLOODED ALERT BANNER (when vehicle sinks in deep water!)
    if car.flooded then
        if ui.beginCenteredWindow("WATER HAZARD", 380, 108) then
            ui.setFontScale(1.25)
            ui.textCentered("ENGINE FLOODED!")
            ui.setFontScale(1.0)
            ui.spacing()
            ui.textCentered(string.format("Rescuing vehicle to dry shore in %.1fs... (or press R)", math.max(car.floodTimer, 0.0)))
        end
        ui.endWindow()
    end

    -- 4. COMPACT DRIVING HUD (All menus hidden while playing! Press H to hide HUD too)
    if showHud then
        local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
        local kmh = math.floor(speed * 3.6)
        local spot = deliverySpots[activeDeliveryIdx]
        local biome = getCurrentBiomeName(car.x, car.z)
        local statusTag = car.flooded and " [FLOODED]" or (car.inWater and " [SHALLOW WATER]" or "")
        local hudW, hudH = 296, 178
        if ui.beginWindow("Cockpit HUD (H = Hide, Esc = Menu)", vpW - hudW - 14, vpH - hudH - 14, hudW, hudH, true) then
            ui.text("Zone: " .. biome .. statusTag)
            ui.text(string.format("Speed: %d km/h   |   Gear: G%d", kmh, car.gear))
            ui.progressBar(car.rpm, string.format("V12 RPM %d%%", math.floor(car.rpm * 100)))
            ui.progressBar(car.boostEnergy / 100.0, string.format("NITRO %d%%", math.floor(car.boostEnergy)))
            ui.separator()
            ui.text("Next: " .. spot.label)
            ui.text(string.format("Score: %d | Deliveries: %d | Jumps: %d", score, deliveriesDone, stuntJumps))
        end
        ui.endWindow()
    end
end

function onShutdown()
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
    sage.log("v0.8 Dynamic HDR Multi-Biome Sandbox shutting down cleanly.")
end
