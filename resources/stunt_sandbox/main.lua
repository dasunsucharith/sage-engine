-- =============================================================================
-- SAGE Engine v0.4 Showcase: 3D Aventador SVJ Stunt & Delivery Sandbox
-- =============================================================================
-- Powered by: Jolt Physics 5.7 + miniaudio 0.11 + Dear ImGui 1.92 + Lua 5.4
--
-- CONTROLS:
--   W / S (or Up / Down)   : Accelerate / Brake & Reverse (6-speed auto!)
--   A / D (or Left / Right): Speed-sensitive Steering
--   Space                  : Handbrake Drift (GTA-style lateral slide)
--   Left Shift             : Nitro Boost (V12 roar + wider 3D FOV)
--   Right-Mouse Drag       : Orbit 3D Chase Camera around car
--   C                      : Cycle Camera (Far Chase -> Close Action -> Hood Cam)
--   X                      : Physics Shockwave (blasts nearby crates sky-high!)
--   B                      : Drop a fresh 6-crate Jolt Physics pyramid ahead
--   T                      : Cycle Time of Day (Sunset / Noon / Night / Sunrise)
--   Tab                    : Toggle Free-Fly 3D Inspector Camera
--   F1                     : Toggle Dear ImGui Live Editor HUD
--   F5 / Ctrl+S            : Live Hot-Reload (JSON scene + Lua scripts)
--   R                      : Reset Aventador SVJ to city center
-- =============================================================================

local CONFIG = {
    carModel      = "assets/models/aventador_svj.glb",
    crateModel    = "assets/models/crate.glb",
    coneModel     = "assets/models/traffic_cone.glb",
    ballModel     = "assets/models/stunt_ball.glb",
    droneModel    = "assets/models/hover_drone.glb",
    carTint       = { 1.0, 1.0, 1.0 },
    maxSpeed      = 32.0,   -- Base top speed (~115 km/h in city scale)
    boostSpeed    = 46.0,   -- Nitro top speed (~165 km/h)
    acceleration  = 27.0,
    brakeForce    = 38.0,
    reverseAccel  = 18.0,
    lowSpeedTurn  = 160.0,  -- Agile low-speed turning (deg/s)
    highSpeedTurn = 82.0,   -- Stable high-speed steering (deg/s)
    normalGrip    = 9.5,
    driftGrip     = 2.2,    -- Smooth controllable GTA-style drifts
    gravity       = 26.0,
    arenaHalfSize = 42.5,
}

-- -----------------------------------------------------------------------------
-- Vehicle, Camera & Mission State
-- -----------------------------------------------------------------------------
local car = {
    x = 0.0, y = 0.0, z = -4.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    yaw = 0.0,
    pitch = 0.0,
    roll = 0.0,
    steerSmooth = 0.0,
    grounded = true,
    airTime = 0.0,
    boostEnergy = 100.0,
    boostTimer = 0.0,
    isDrifting = false,
    isBoosting = false,
    gear = 1,
    rpm = 0.12,
}

local camPresets = {
    { name = "Far Chase",    dist = 10.2, height = 4.2, lookAhead = 5.0, fov = 60.0 },
    { name = "Close Action", dist =  7.2, height = 2.9, lookAhead = 6.5, fov = 66.0 },
    { name = "Hood Cam",     dist = -0.7, height = 1.48, lookAhead = 18.0, fov = 72.0 },
}
local camPresetIdx = 1
local cam = {
    x = 0.0, y = 5.0, z = -14.0,
    tx = 0.0, ty = 1.4, tz = 0.0,
    fov = 60.0,
    orbitYaw = 0.0,
    orbitPitch = 0.0,
}

local buildings = {
    { id = "col_bldg_ne", x =  14.5, z =  14.5, w = 11.8, h = 11.8 },
    { id = "col_bldg_nw", x = -14.5, z =  14.5, w = 11.8, h = 11.8 },
    { id = "col_bldg_sw", x = -14.5, z = -14.5, w = 11.8, h = 11.8 },
    { id = "col_bldg_se", x =  14.5, z = -14.5, w = 11.8, h = 11.8 },
}

local ramps = {
    { x =   0.0, z =  14.5, yaw =   0.0, width = 5.0, length = 7.5, height = 2.2 },
    { x =   0.0, z = -14.5, yaw = 180.0, width = 5.0, length = 7.5, height = 2.2 },
    { x =  14.5, z =   0.0, yaw =  90.0, width = 5.0, length = 7.5, height = 2.2 },
    { x = -14.5, z =   0.0, yaw = -90.0, width = 5.0, length = 7.5, height = 2.2 },
}

local deliverySpots = {
    { x =  28.0, z =  28.0, label = "North-East Plaza" },
    { x = -28.0, z =  28.0, label = "Westside Docks" },
    { x = -28.0, z = -28.0, label = "Downtown Depot" },
    { x =  28.0, z = -28.0, label = "Harbor Warehouse" },
    { x =   0.0, z =  32.0, label = "North Overpass" },
    { x =  32.0, z =   0.0, label = "East Boulevard" },
    { x = -32.0, z =   0.0, label = "Sunset Strip" },
    { x =   0.0, z = -32.0, label = "South Pier" },
}

local timePresets = {
    { hour = 17.5, name = "Golden Sunset" },
    { hour = 12.5, name = "Bright Noon" },
    { hour = 22.5, name = "Neon Night" },
    { hour =  6.4, name = "Early Sunrise" },
}
local timePresetIdx = 1

-- Debounce latches for one-shot hotkeys inside 60 Hz fixed tick
local keyLatch = { T = false, C = false, X = false, B = false, R = false }

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
-- Math Helpers
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

-- -----------------------------------------------------------------------------
-- Jolt Physics Setup & Dynamic Crate Pyramids
-- -----------------------------------------------------------------------------
local function spawnCrate(x, y, z)
    nextCrateSeq = nextCrateSeq + 1
    local id = "phys_crate_" .. nextCrateSeq
    world.spawnModel(id, CONFIG.crateModel, x, y, z)
    physics.addDynamicBox(id, x, y, z, 0.64, 0.64, 0.64, 18.0, 0.32, 0.62)
    table.insert(crates, { id = id, smashed = false })
end

-- Builds a real 3-tier Jolt Physics stacked pyramid (3 + 2 + 1 = 6 tumbling crates!)
local function spawnCratePyramid(cx, cz, yawDeg)
    local rad = degToRad(yawDeg or 0.0)
    local rightX, rightZ = math.cos(rad), -math.sin(rad)
    local step = 1.32

    -- Bottom row (3 crates)
    for i = -1, 1 do
        spawnCrate(cx + rightX * (i * step), 0.66, cz + rightZ * (i * step))
    end
    -- Middle row (2 crates)
    for _, off in ipairs({ -0.5, 0.5 }) do
        spawnCrate(cx + rightX * (off * step), 1.96, cz + rightZ * (off * step))
    end
    -- Top crate
    spawnCrate(cx, 3.26, cz)
end

local function setupJoltPhysicsWorld()
    -- 1. Static ground plane collider (top face at y = 0.0)
    physics.addStaticBox("col_ground", 0.0, -1.0, 0.0, 120.0, 1.0, 120.0)

    -- 2. Static perimeter walls (so flying crates bounce back into the arena!)
    physics.addStaticBox("col_wall_n",   0.0, 4.0,  44.5, 46.0, 5.0, 1.0)
    physics.addStaticBox("col_wall_s",   0.0, 4.0, -44.5, 46.0, 5.0, 1.0)
    physics.addStaticBox("col_wall_e",  44.5, 4.0,   0.0,  1.0, 5.0, 46.0)
    physics.addStaticBox("col_wall_w", -44.5, 4.0,   0.0,  1.0, 5.0, 46.0)

    -- 3. Static skyscraper colliders
    for _, b in ipairs(buildings) do
        physics.addStaticBox(b.id, b.x, 12.0, b.z, b.w * 0.5, 12.0, b.h * 0.5)
    end

    -- 3b. Real Jolt Convex-Hull 3D Stunt Ramp colliders (so balls & crates roll up and launch off ramps!)
    for i, r in ipairs(ramps) do
        physics.addStaticRamp("col_ramp_" .. i, r.x, 0.0, r.z, r.width, r.height, r.length, r.yaw)
    end

    -- 4. Kinematic Jolt hull for the Lamborghini Aventador SVJ (transfers true collision momentum!)
    physics.addKinematicBox("player_car", car.x, car.y + 0.65, car.z, 1.05, 0.62, 2.25)

    -- 5. Four 3-tier tumbling Jolt Physics crate pyramids (24 rigid bodies!)
    crates = {}
    spawnCratePyramid(  0.0,  26.0,   0.0)
    spawnCratePyramid(  0.0, -26.0,   0.0)
    spawnCratePyramid( 26.0,   0.0,  90.0)
    spawnCratePyramid(-26.0,   0.0,  90.0)

    -- 6. Three Giant Rocket-League-Style Jolt Physics Stunt Balls (spheres!)
    nextBallSeq = 0
    for _, bp in ipairs({ { 0.0, 3.0, 6.0 }, { 22.0, 3.0, 22.0 }, { -22.0, 3.0, -22.0 } }) do
        nextBallSeq = nextBallSeq + 1
        local id = "stunt_ball_" .. nextBallSeq
        world.spawnModel(id, CONFIG.ballModel, bp[1], bp[2], bp[3])
        physics.addDynamicSphere(id, bp[1], bp[2], bp[3], 1.25, 32.0, 0.72, 0.35)
    end

    -- 7. Eight Knockable Jolt Physics Capsule Traffic Cones lining the stunt avenues!
    local coneSpots = {
        { -3.8,  8.0 }, {  3.8,  8.0 }, { -3.8, -8.0 }, {  3.8, -8.0 },
        {  8.0,  3.8 }, {  8.0, -3.8 }, { -8.0,  3.8 }, { -8.0, -3.8 },
    }
    for i, cp in ipairs(coneSpots) do
        local id = "cone_" .. i
        world.spawnModel(id, CONFIG.coneModel, cp[1], 0.50, cp[2])
        physics.addDynamicCapsule(id, cp[1], 0.50, cp[2], 0.24, 0.26, 6.0, 0.28, 0.65)
    end
end

local function spawnDynamicVisuals()
    -- Dashed yellow road markings
    local dashId = 0
    for d = -38, 38, 6 do
        if math.abs(d) > 7 and math.abs(math.abs(d) - 14.5) > 4.5 then
            dashId = dashId + 1
            local idNS = "dash_ns_" .. dashId
            world.spawnPlane(idNS, 0.0, 0.02, d)
            world.setScale(idNS, 0.32, 1.0, 2.6)
            world.setColor(idNS, 0.96, 0.78, 0.16)

            local idEW = "dash_ew_" .. dashId
            world.spawnPlane(idEW, d, 0.02, 0.0)
            world.setScale(idEW, 2.6, 1.0, 0.32)
            world.setColor(idEW, 0.96, 0.78, 0.16)
        end
    end

    -- Player Lamborghini Aventador SVJ (.glb)
    world.spawnModel("player_car", CONFIG.carModel, car.x, car.y, car.z)
    world.setColor("player_car", CONFIG.carTint[1], CONFIG.carTint[2], CONFIG.carTint[3])

    -- Nitro exhaust flame
    world.spawnTriangle("car_flame", car.x, car.y + 0.42, car.z)
    world.setScale("car_flame", 0.95, 0.65, 2.4)
    world.setColor("car_flame", 0.15, 0.88, 1.0)
    world.setEmissive("car_flame", true)
    world.setVisible("car_flame", false)

    -- Floating 3D nav arrow above car
    world.spawnTriangle("nav_arrow", car.x, car.y + 2.6, car.z)
    world.setScale("nav_arrow", 0.95, 0.6, 1.4)
    world.setColor("nav_arrow", 1.0, 0.86, 0.18)
    world.setEmissive("nav_arrow", true)

    -- Glowing 3D Delivery Checkpoint Beacon Ring
    local spot = deliverySpots[activeDeliveryIdx]
    world.spawnRing("delivery_ring", spot.x, 0.05, spot.z)
    world.setScale("delivery_ring", 6.5, 4.4, 6.5)
    world.setColor("delivery_ring", 1.0, 0.82, 0.14)
    world.setEmissive("delivery_ring", true)

    -- Two Animated Blender Hover Drones patrolling overhead!
    world.spawnModel("drone_1", CONFIG.droneModel, 12.0, 7.5, 0.0)
    world.playAnimation("drone_1", "", true, 1.2)
    world.spawnModel("drone_2", CONFIG.droneModel, -12.0, 9.5, 0.0)
    world.playAnimation("drone_2", "", true, 1.5)
end

-- -----------------------------------------------------------------------------
-- Ramp Surface Sampling & Building Collisions
-- -----------------------------------------------------------------------------
local function sampleGroundHeightAndSlope(x, z)
    local groundY = 0.0
    local onRamp = nil

    for _, r in ipairs(ramps) do
        local rad = degToRad(r.yaw)
        local dx = x - r.x
        local dz = z - r.z
        local localZ = dx * math.sin(rad) + dz * math.cos(rad)
        local localX = dx * math.cos(rad) - dz * math.sin(rad)

        if math.abs(localX) <= (r.width * 0.5 + 0.65) and math.abs(localZ) <= (r.length * 0.5 + 0.25) then
            local t = clamp((localZ + r.length * 0.5) / r.length, 0.0, 1.0)
            local h = t * r.height
            if h > groundY then
                groundY = h
                onRamp = r
            end
        end
    end
    return groundY, onRamp
end

local function resolveCarBuildingCollisions()
    local carRadius = 1.65
    local S = CONFIG.arenaHalfSize - carRadius
    local hitWall = false

    if car.x > S then car.x = S; car.vx = -car.vx * 0.35; hitWall = true end
    if car.x < -S then car.x = -S; car.vx = -car.vx * 0.35; hitWall = true end
    if car.z > S then car.z = S; car.vz = -car.vz * 0.35; hitWall = true end
    if car.z < -S then car.z = -S; car.vz = -car.vz * 0.35; hitWall = true end

    for _, b in ipairs(buildings) do
        local halfW = b.w * 0.5 + carRadius
        local halfH = b.h * 0.5 + carRadius
        local dx = car.x - b.x
        local dz = car.z - b.z
        if math.abs(dx) < halfW and math.abs(dz) < halfH then
            local penX = halfW - math.abs(dx)
            local penZ = halfH - math.abs(dz)
            if penX < penZ then
                car.x = car.x + (dx >= 0 and penX or -penX)
                car.vx = -car.vx * 0.35
            else
                car.z = car.z + (dz >= 0 and penZ or -penZ)
                car.vz = -car.vz * 0.35
            end
            hitWall = true
        end
    end

    local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    if hitWall and speed > 4.0 and impactSoundCooldown <= 0.0 then
        audio.playImpact(clamp(speed / 24.0, 0.3, 1.0))
        impactSoundCooldown = 0.22
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

    local ringRadius = 6.2 + 0.7 * math.sin(timeNow * 4.5)
    world.setScale("delivery_ring", ringRadius, 4.5, ringRadius)
    world.setRotation("delivery_ring", 0.0, timeNow * 55.0, 0.0)

    local dx = spot.x - car.x
    local dz = spot.z - car.z
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist > 0.05 then
        local bob = 0.22 * math.sin(timeNow * 5.0)
        world.setPosition("nav_arrow", car.x, car.y + 2.55 + bob, car.z)
        local angleDeg = math.atan(dx, dz) * (180.0 / math.pi)
        world.setRotation("nav_arrow", 0.0, angleDeg, 0.0)
    end

    if dist < 4.4 then
        deliveriesDone = deliveriesDone + 1
        local speedBonus = math.floor(math.max(0.0, 20.0 - deliveryTimer) * 10.0)
        local earned = 100 + speedBonus
        score = score + earned
        car.boostEnergy = 100.0
        audio.playChime()

        sage.log(string.format(
            "DELIVERY #%d at %s! +%d pts (Speed Bonus: +%d) | Score: %d",
            deliveriesDone, spot.label, earned, speedBonus, score
        ))

        local nextIdx = activeDeliveryIdx
        while nextIdx == activeDeliveryIdx do
            nextIdx = math.random(1, #deliverySpots)
        end
        activeDeliveryIdx = nextIdx
        deliveryTimer = 0.0

        local nextSpot = deliverySpots[activeDeliveryIdx]
        world.setPosition("delivery_ring", nextSpot.x, 0.05, nextSpot.z)
        sage.log("Next drop-off -> " .. nextSpot.label)
    end
end

-- -----------------------------------------------------------------------------
-- FiveM-Style Lifecycle Callbacks
-- -----------------------------------------------------------------------------
function onStart()
    math.randomseed(42)
    nextCrateSeq = 0
    spawnDynamicVisuals()
    setupJoltPhysicsWorld()

    camera.setPosition(cam.x, cam.y, cam.z)
    camera.setTarget(cam.tx, cam.ty, cam.tz)
    camera.setFov(cam.fov)

    sage.log("=================================================================")
    sage.log("  SAGE v0.4 Showcase: Aventador SVJ + Jolt Physics + V12 Audio")
    sage.log("  WASD: Drive | Space: Drift | Shift: Nitro | C: Camera Mode")
    sage.log("  Right-Mouse Drag: Orbit Camera | X: Shockwave | B: Drop Crates")
    sage.log("  F1: Dear ImGui Live Editor | Tab: Free-Fly | T: Time of Day")
    sage.log("  First drop-off -> " .. deliverySpots[activeDeliveryIdx].label)
    sage.log("=================================================================")
end

function onTick(dt)
    local timeNow = sage.getTime()

    -- One-shot hotkeys
    if keyJustPressed("T") then
        timePresetIdx = (timePresetIdx % #timePresets) + 1
        local preset = timePresets[timePresetIdx]
        world.setTimeOfDay(preset.hour)
        sage.log("Sky Time of Day -> " .. preset.name)
    end

    if keyJustPressed("C") then
        camPresetIdx = (camPresetIdx % #camPresets) + 1
        cam.orbitYaw = 0.0
        cam.orbitPitch = 0.0
        sage.log("Camera View -> " .. camPresets[camPresetIdx].name)
    end

    if keyJustPressed("X") then
        local hitCount = physics.applyExplosion(car.x, car.y + 0.5, car.z, 24.0, 380.0)
        audio.playExplosion()
        sage.log("SHOCKWAVE! Blasted " .. hitCount .. " Jolt Physics objects!")
    end

    if keyJustPressed("B") then
        local rad = degToRad(car.yaw)
        local spawnX = clamp(car.x + math.sin(rad) * 14.0, -38.0, 38.0)
        local spawnZ = clamp(car.z + math.cos(rad) * 14.0, -38.0, 38.0)
        spawnCratePyramid(spawnX, spawnZ, car.yaw)
        audio.playChime()
        sage.log("Dropped fresh 6-crate Jolt Physics pyramid ahead!")
    end

    if keyJustPressed("R") then
        car.x, car.y, car.z = 0.0, 0.0, -4.0
        car.vx, car.vy, car.vz = 0.0, 0.0, 0.0
        car.yaw, car.pitch, car.roll = 0.0, 0.0, 0.0
        car.grounded = true
        car.airTime = 0.0
        car.boostEnergy = 100.0
        cam.orbitYaw, cam.orbitPitch = 0.0, 0.0
    end

    -- Driving inputs
    local throttle = 0.0
    if input.isKeyDown("W") or input.isKeyDown("Up") then throttle = throttle + 1.0 end
    if input.isKeyDown("S") or input.isKeyDown("Down") then throttle = throttle - 1.0 end

    local rawSteer = 0.0
    if input.isKeyDown("A") or input.isKeyDown("Left") then rawSteer = rawSteer + 1.0 end
    if input.isKeyDown("D") or input.isKeyDown("Right") then rawSteer = rawSteer - 1.0 end
    -- Smooth analog-style steering response (crisp yet never jerky!)
    car.steerSmooth = lerp(car.steerSmooth, rawSteer, 13.0 * dt)

    car.isDrifting = input.isKeyDown("Space")

    local shiftHeld = input.isKeyDown("LeftShift") or input.isKeyDown("RightShift")
    if car.boostTimer > 0.0 then car.boostTimer = car.boostTimer - dt end
    car.isBoosting = (shiftHeld and car.boostEnergy > 1.0) or (car.boostTimer > 0.0)
    if shiftHeld and car.boostEnergy > 0.0 then
        car.boostEnergy = math.max(0.0, car.boostEnergy - 36.0 * dt)
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
        -- Progressive speed-sensitive steering (agile slow, rock-steady fast!)
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
            forwardVel = forwardVel * math.exp(-1.7 * dt)
        end

        if car.isBoosting and throttle >= 0.0 then
            forwardVel = math.max(forwardVel, topSpeed * 0.72)
        end
        forwardVel = clamp(forwardVel, -12.0, topSpeed)

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

    -- 3D Stunt Ramp elevation & vertical jump physics
    local groundY, ramp = sampleGroundHeightAndSlope(car.x, car.z)
    if car.grounded then
        if ramp and groundY > 0.05 then
            local climbRate = (groundY - prevY) / math.max(dt, 0.001)
            car.y = groundY
            car.vy = math.max(climbRate, 0.0)
            car.pitch = lerp(car.pitch, -17.5, 14.0 * dt)
            if groundY > ramp.height * 0.78 and math.abs(forwardVel) > 10.0 then
                car.vy = math.max(car.vy, math.abs(forwardVel) * 0.44 + 4.8)
                car.boostTimer = math.max(car.boostTimer, 0.55)
                car.grounded = false
            end
        else
            if car.y > groundY + 0.16 then
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
        car.vy = car.vy - CONFIG.gravity * dt
        car.y = car.y + car.vy * dt
        car.pitch = lerp(car.pitch, clamp(-car.vy * 2.1, -26.0, 22.0), 6.0 * dt)

        if car.y <= groundY then
            car.y = groundY
            car.vy = 0.0
            car.grounded = true
            if car.airTime >= 0.42 then
                stuntJumps = stuntJumps + 1
                local airBonus = math.floor(car.airTime * 150.0)
                score = score + airBonus
                car.boostEnergy = math.min(100.0, car.boostEnergy + 45.0)
                audio.playImpact(0.75)
                audio.playChime()
                -- Landing shockwave jostles nearby crates!
                physics.applyExplosion(car.x, car.y + 0.3, car.z, 10.0, 110.0)
                sage.log(string.format(
                    "STUNT JUMP #%d! Air time: %.2fs (+%d pts) | Score: %d",
                    stuntJumps, car.airTime, airBonus, score
                ))
            end
            car.airTime = 0.0
        end
    end

    -- Suspension body roll
    local targetRoll = clamp(lateralVel * 0.95 + car.steerSmooth * speedNorm * 2.5, -8.5, 8.5)
    car.roll = lerp(car.roll, targetRoll, 9.0 * dt)

    resolveCarBuildingCollisions()

    -- Drive Jolt Kinematic Hull so the Aventador SVJ physically smashes & scatters crate stacks!
    physics.moveKinematic("player_car", car.x, car.y + 0.65, car.z, car.pitch, car.yaw, car.roll, dt)
    checkPhysicsCrateSmashes(dt)
    updateDeliveryMission(dt, timeNow)

    -- Sync visual .glb Aventador & nitro exhaust flame
    world.setPosition("player_car", car.x, car.y, car.z)
    world.setRotation("player_car", car.pitch, car.yaw, car.roll)

    -- Orbit the 2 animated Blender hover drones smoothly around the central plaza!
    local d1a = timeNow * 0.45
    world.setPosition("drone_1", math.cos(d1a) * 14.0, 7.2, math.sin(d1a) * 14.0)
    world.setRotation("drone_1", 0.0, math.deg(-d1a), 0.0)

    local d2a = -timeNow * 0.35 + 1.8
    world.setPosition("drone_2", math.cos(d2a) * 24.0, 10.5, math.sin(d2a) * 24.0)
    world.setRotation("drone_2", 0.0, math.deg(-d2a + math.pi), 0.0)

    if car.isBoosting then
        local flicker = 1.65 + 0.55 * math.sin(timeNow * 48.0)
        world.setVisible("car_flame", true)
        world.setPosition("car_flame", car.x - fwdX * 2.7, car.y + 0.42, car.z - fwdZ * 2.7)
        world.setRotation("car_flame", car.pitch, car.yaw + 180.0, 0.0)
        world.setScale("car_flame", 0.85, 0.6, flicker)
    else
        world.setVisible("car_flame", false)
    end

    -- 6-Gear Transmission & miniaudio V12 Engine Synth!
    local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    local gearSpan = CONFIG.boostSpeed / 6.0
    car.gear = clamp(math.floor(speed / gearSpan) + 1, 1, 6)
    local gearProgress = (speed - (car.gear - 1) * gearSpan) / gearSpan
    local targetRpm = clamp(0.14 + gearProgress * 0.72 + (car.isBoosting and 0.18 or 0.0), 0.08, 1.0)
    if not car.grounded and throttle > 0.0 then
        targetRpm = 0.96
    end
    car.rpm = lerp(car.rpm, targetRpm, 10.0 * dt)

    local driftIntensity = (car.grounded and math.abs(lateralVel) > 2.4)
        and clamp((math.abs(lateralVel) - 2.4) / 8.0, 0.0, 1.0)
        or 0.0
    audio.setVehicleState(
        car.rpm,
        throttle > 0.0 and 1.0 or 0.15,
        driftIntensity,
        car.isBoosting and 1.0 or 0.0,
        not car.grounded
    )

    -- 3D Chase Camera with GTA V-style Right-Mouse Orbit & 3 View Modes (C)
    local preset = camPresets[camPresetIdx]
    if input.isMouseDown("Right") then
        local mdx, mdy = input.getMouseDelta()
        cam.orbitYaw = cam.orbitYaw - mdx * 0.35
        cam.orbitPitch = clamp(cam.orbitPitch + mdy * 0.22, -12.0, 35.0)
    elseif speed > 2.5 then
        -- Gently recenter orbit behind car while driving
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
    local desiredFov = preset.fov + clamp(speed * 0.32, 0.0, 14.0) + (car.isBoosting and 6.0 or 0.0)

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

    -- Update HUD window title 4x/sec
    titleUpdateAccum = titleUpdateAccum + dt
    if titleUpdateAccum >= 0.25 then
        titleUpdateAccum = 0.0
        local kmh = math.floor(speed * 3.6)
        local tag = ""
        if not car.grounded then
            tag = " | [AIR TIME!]"
        elseif car.isBoosting then
            tag = " | [NITRO!]"
        elseif driftIntensity > 0.25 then
            tag = " | [DRIFT!]"
        end
        sage.setTitle(string.format(
            "SAGE Aventador SVJ | G%d %d km/h | Nitro: %d%% | Deliveries: %d | Score: %d%s",
            car.gear, kmh, math.floor(car.boostEnergy), deliveriesDone, score, tag
        ))
    end
end

function onRenderUI()
    local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    local kmh = math.floor(speed * 3.6)
    local spot = deliverySpots[activeDeliveryIdx]

    if ui.beginWindow("Aventador SVJ Cockpit HUD", 985, 515, 280, 190, true) then
        ui.text(string.format("Speed: %d km/h  |  Gear: G%d", kmh, car.gear))
        ui.progressBar(car.rpm, string.format("V12 RPM %d%%", math.floor(car.rpm * 100)))
        ui.progressBar(car.boostEnergy / 100.0, string.format("NITRO %d%%", math.floor(car.boostEnergy)))
        ui.separator()
        ui.text("Drop-off: " .. spot.label)
        ui.text(string.format("Score: %d | Deliveries: %d | Jumps: %d", score, deliveriesDone, stuntJumps))
        if ui.button("Drop Stunt Ball") then
            nextBallSeq = nextBallSeq + 1
            local id = "stunt_ball_" .. nextBallSeq
            local rad = degToRad(car.yaw)
            local bx = clamp(car.x + math.sin(rad) * 10.0, -38.0, 38.0)
            local bz = clamp(car.z + math.cos(rad) * 10.0, -38.0, 38.0)
            world.spawnModel(id, CONFIG.ballModel, bx, 5.0, bz)
            physics.addDynamicSphere(id, bx, 5.0, bz, 1.25, 32.0, 0.72, 0.35)
            audio.playChime()
        end
    end
    ui.endWindow()
end

function onShutdown()
    audio.setVehicleState(0.0, 0.0, 0.0, 0.0, false)
    sage.log("v0.4 Stunt & Delivery Sandbox shutting down cleanly.")
end
