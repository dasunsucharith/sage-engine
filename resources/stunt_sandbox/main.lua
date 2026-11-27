-- =============================================================================
-- SAGE Engine Showcase Game: 3D Mini Stunt & Delivery Sandbox (v0.3)
-- =============================================================================
-- LIVE HOT-RELOAD DEMO:
--   Edit scenes/stunt_city.json OR any setting below and press Ctrl+S!
--   Replace assets/models/sports_car.glb with your own Blender .glb anytime!
--
-- CONTROLS:
--   W / S (or Up / Down)   : Accelerate / Brake & Reverse
--   A / D (or Left / Right): Steer
--   Space                  : Handbrake Drift
--   Left Shift             : Nitro Boost (widens 3D FOV!)
--   T                      : Cycle Time of Day (Sunset -> Noon -> Night -> Sunrise)
--   Tab                    : Toggle 3D Free-Fly Inspector Camera
--   R                      : Reset car to city center
--   F5                     : Manual Hot-Reload (JSON scene + Lua scripts)
--   Esc                    : Quit
-- =============================================================================

local CONFIG = {
    -- Your custom Blender Lamborghini Aventador SVJ!
    carModel      = "assets/models/aventador_svj.glb",
    crateModel    = "assets/models/crate.glb",
    carTint       = { 1.0, 1.0, 1.0 }, -- {1,1,1} keeps original .glb PBR paint colors!
    maxSpeed      = 27.0,              -- Base top speed (m/s)
    boostSpeed    = 40.0,              -- Nitro top speed (m/s)
    acceleration  = 24.0,
    reverseAccel  = 16.0,
    turnSpeed     = 135.0,             -- Degrees/sec
    normalGrip    = 8.5,
    driftGrip     = 2.0,               -- Lower = wider GTA-style power slides!
    gravity       = 26.0,              -- 3D stunt jump gravity (m/s^2)
    chaseDistance = 10.5,              -- 3D chase camera distance behind car
    chaseHeight   = 4.6,               -- 3D chase camera height
    arenaHalfSize = 42.5,
}

-- -----------------------------------------------------------------------------
-- 3D Game State
-- -----------------------------------------------------------------------------
local car = {
    x = 0.0, y = 0.0, z = -4.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    yaw = 0.0,        -- Degrees around +Y (0 = facing +Z)
    pitch = 0.0,      -- Jump pitch tilt (negative = nose up)
    roll = 0.0,       -- Drift body roll
    grounded = true,
    airTime = 0.0,
    boostEnergy = 100.0,
    boostTimer = 0.0,
    isDrifting = false,
    isBoosting = false,
}

local cam = {
    x = 0.0, y = 6.0, z = -15.0,
    tx = 0.0, ty = 1.5, tz = 0.0,
    fov = 60.0,
}

-- Matches the 4 city blocks in scenes/stunt_city.json
local buildings = {
    { x =  14.5, z =  14.5, w = 11.8, h = 11.8 },
    { x = -14.5, z =  14.5, w = 11.8, h = 11.8 },
    { x = -14.5, z = -14.5, w = 11.8, h = 11.8 },
    { x =  14.5, z = -14.5, w = 11.8, h = 11.8 },
}

-- Matches the 4 placed assets/models/stunt_ramp.glb jump ramps in scenes/stunt_city.json
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
local tKeyWasDown = false

local crates = {}
local activeDeliveryIdx = 1
local deliveriesDone = 0
local score = 0
local stuntJumps = 0
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

-- -----------------------------------------------------------------------------
-- Spawn Dynamic 3D Game Entities (.glb Car, .glb Crates, Road Markings, Beacon)
-- -----------------------------------------------------------------------------
local function spawnDynamicWorld()
    -- Dashed yellow road lines along both main avenues
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

    -- Player 3D Sports Car (.glb)
    world.spawnModel("player_car", CONFIG.carModel, car.x, car.y, car.z)
    world.setColor("player_car", CONFIG.carTint[1], CONFIG.carTint[2], CONFIG.carTint[3])

    -- Glowing cyan nitro exhaust flame behind car
    world.spawnTriangle("car_flame", car.x, car.y + 0.45, car.z)
    world.setScale("car_flame", 0.95, 0.7, 2.2)
    world.setColor("car_flame", 0.15, 0.88, 1.0)
    world.setEmissive("car_flame", true)
    world.setVisible("car_flame", false)

    -- Floating golden 3D navigation chevron above car
    world.spawnTriangle("nav_arrow", car.x, car.y + 2.7, car.z)
    world.setScale("nav_arrow", 1.0, 0.65, 1.45)
    world.setColor("nav_arrow", 1.0, 0.86, 0.18)
    world.setEmissive("nav_arrow", true)

    -- Glowing 3D Delivery Checkpoint Beacon Ring
    local spot = deliverySpots[activeDeliveryIdx]
    world.spawnRing("delivery_ring", spot.x, 0.05, spot.z)
    world.setScale("delivery_ring", 6.5, 4.2, 6.5)
    world.setColor("delivery_ring", 1.0, 0.82, 0.14)
    world.setEmissive("delivery_ring", true)

    -- 16 Smashable 3D .glb Crates around the city!
    crates = {}
    local crateCoords = {
        {  5.0,  28.0 }, {  7.0,  28.0 }, {  6.0,  26.0 },
        { -5.0, -28.0 }, { -7.0, -28.0 }, { -6.0, -26.0 },
        { 28.0,  -6.0 }, { 28.0,  -8.0 }, { 26.0,  -7.0 },
        {-28.0,   6.0 }, {-28.0,   8.0 }, {-26.0,   7.0 },
        {  4.5,   4.5 }, { -4.5,   4.5 }, {  4.5,  -4.5 }, { -4.5, -4.5 },
    }
    for i, pos in ipairs(crateCoords) do
        local c = {
            id = "crate_" .. i,
            x = pos[1], y = 0.65, z = pos[2],
            vx = 0.0, vy = 0.0, vz = 0.0,
            yaw = (i * 29) % 90,
            angVel = 0.0,
            smashed = false,
        }
        world.spawnModel(c.id, CONFIG.crateModel, c.x, c.y, c.z)
        world.setRotation(c.id, 0.0, c.yaw, 0.0)
        table.insert(crates, c)
    end
end

-- -----------------------------------------------------------------------------
-- 3D Ramp Surface Height & Jump Launch
-- -----------------------------------------------------------------------------
local function sampleGroundHeightAndSlope(x, z)
    local groundY = 0.0
    local onRamp = nil

    for _, r in ipairs(ramps) do
        local rad = degToRad(r.yaw)
        local dx = x - r.x
        local dz = z - r.z
        -- Transform car position into ramp local space (+Z = up the ramp)
        local localZ =  dx * math.sin(rad) + dz * math.cos(rad)
        local localX =  dx * math.cos(rad) - dz * math.sin(rad)

        if math.abs(localX) <= (r.width * 0.5 + 0.6) and math.abs(localZ) <= (r.length * 0.5 + 0.2) then
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

-- -----------------------------------------------------------------------------
-- Collisions, Crates & Delivery Mission
-- -----------------------------------------------------------------------------
local function resolveBuildingCollisions()
    local carRadius = 1.6
    local S = CONFIG.arenaHalfSize - carRadius

    if car.x > S then car.x = S; car.vx = -car.vx * 0.4 end
    if car.x < -S then car.x = -S; car.vx = -car.vx * 0.4 end
    if car.z > S then car.z = S; car.vz = -car.vz * 0.4 end
    if car.z < -S then car.z = -S; car.vz = -car.vz * 0.4 end

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
        end
    end
end

local function updateCrates(dt)
    local S = CONFIG.arenaHalfSize - 1.0
    for _, c in ipairs(crates) do
        local dx = c.x - car.x
        local dz = c.z - car.z
        local dy = math.abs(c.y - (car.y + 0.6))
        local distSq = dx * dx + dz * dz
        if distSq < 5.0 and dy < 2.0 and distSq > 0.0001 then
            local dist = math.sqrt(distSq)
            local nx = dx / dist
            local nz = dz / dist
            local carSpeed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
            local impulse = math.max(12.0, carSpeed * 1.45)
            c.vx = nx * impulse + car.vx * 0.45
            c.vz = nz * impulse + car.vz * 0.45
            c.vy = math.max(c.vy, 5.5 + carSpeed * 0.22)
            c.angVel = (math.random() > 0.5 and 1 or -1) * (200.0 + carSpeed * 14.0)

            if not c.smashed then
                c.smashed = true
                score = score + 25
                world.setColor(c.id, 1.0, 0.55, 0.35)
                sage.log("CRATE SMASH! +25 pts | Score: " .. score)
            end
        end

        c.vy = c.vy - CONFIG.gravity * dt
        c.x = clamp(c.x + c.vx * dt, -S, S)
        c.z = clamp(c.z + c.vz * dt, -S, S)
        c.y = c.y + c.vy * dt
        if c.y < 0.65 then
            c.y = 0.65
            c.vy = -c.vy * 0.35
            if math.abs(c.vy) < 0.8 then c.vy = 0.0 end
        end

        c.yaw = c.yaw + c.angVel * dt
        local drag = math.exp(-3.2 * dt)
        c.vx = c.vx * drag
        c.vz = c.vz * drag
        c.angVel = c.angVel * drag

        world.setPosition(c.id, c.x, c.y, c.z)
        world.setRotation(c.id, 0.0, c.yaw, 0.0)
    end
end

local function updateDeliveryMission(dt, timeNow)
    deliveryTimer = deliveryTimer + dt
    local spot = deliverySpots[activeDeliveryIdx]

    local ringRadius = 6.2 + 0.7 * math.sin(timeNow * 4.5)
    world.setScale("delivery_ring", ringRadius, 4.4, ringRadius)
    world.setRotation("delivery_ring", 0.0, timeNow * 50.0, 0.0)

    -- Point floating 3D nav arrow above car toward active drop-off
    local dx = spot.x - car.x
    local dz = spot.z - car.z
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist > 0.05 then
        local bob = 0.25 * math.sin(timeNow * 5.0)
        world.setPosition("nav_arrow", car.x, car.y + 2.75 + bob, car.z)
        local angleDeg = math.atan(dx, dz) * (180.0 / math.pi)
        world.setRotation("nav_arrow", 0.0, angleDeg, 0.0)
    end

    if dist < 4.2 then
        deliveriesDone = deliveriesDone + 1
        local speedBonus = math.floor(math.max(0.0, 20.0 - deliveryTimer) * 10.0)
        local earned = 100 + speedBonus
        score = score + earned
        car.boostEnergy = 100.0
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
    spawnDynamicWorld()

    camera.setPosition(cam.x, cam.y, cam.z)
    camera.setTarget(cam.tx, cam.ty, cam.tz)
    camera.setFov(cam.fov)

    sage.log("==============================================================")
    sage.log("  SAGE 3D Showcase: Stunt & Delivery Sandbox (v0.3)")
    sage.log("  WASD/Arrows: Drive | Space: Drift | Shift: Nitro Boost")
    sage.log("  Hit the 4 neon 3D ramps for Stunt Jumps!")
    sage.log("  T: Cycle Time of Day | Tab: Free-Fly 3D Camera | F5: Reload")
    sage.log("  First drop-off -> " .. deliverySpots[activeDeliveryIdx].label)
    sage.log("==============================================================")
end

function onTick(dt)
    local timeNow = sage.getTime()

    -- Cycle time of day on T key press
    local tDown = input.isKeyDown("T")
    if tDown and not tKeyWasDown then
        timePresetIdx = (timePresetIdx % #timePresets) + 1
        local preset = timePresets[timePresetIdx]
        world.setTimeOfDay(preset.hour)
        sage.log("Sky Time of Day -> " .. preset.name .. " (" .. preset.hour .. ":00)")
    end
    tKeyWasDown = tDown

    -- Reset car on R
    if input.isKeyDown("R") then
        car.x, car.y, car.z = 0.0, 0.0, -4.0
        car.vx, car.vy, car.vz = 0.0, 0.0, 0.0
        car.yaw, car.pitch, car.roll = 0.0, 0.0, 0.0
        car.grounded = true
        car.airTime = 0.0
        car.boostEnergy = 100.0
    end

    -- Driving inputs
    local throttle = 0.0
    if input.isKeyDown("W") or input.isKeyDown("Up") then throttle = throttle + 1.0 end
    if input.isKeyDown("S") or input.isKeyDown("Down") then throttle = throttle - 1.0 end

    local steer = 0.0
    if input.isKeyDown("A") or input.isKeyDown("Left") then steer = steer + 1.0 end
    if input.isKeyDown("D") or input.isKeyDown("Right") then steer = steer - 1.0 end

    car.isDrifting = input.isKeyDown("Space")

    local shiftHeld = input.isKeyDown("LeftShift") or input.isKeyDown("RightShift")
    if car.boostTimer > 0.0 then car.boostTimer = car.boostTimer - dt end
    car.isBoosting = (shiftHeld and car.boostEnergy > 1.0) or (car.boostTimer > 0.0)
    if shiftHeld and car.boostEnergy > 0.0 then
        car.boostEnergy = math.max(0.0, car.boostEnergy - 38.0 * dt)
    else
        car.boostEnergy = math.min(100.0, car.boostEnergy + 18.0 * dt)
    end

    -- Right-handed 3D basis (+Y up, +Z forward at yaw = 0, +X left)
    local rad = degToRad(car.yaw)
    local fwdX = math.sin(rad)
    local fwdZ = math.cos(rad)
    local leftX = math.cos(rad)
    local leftZ = -math.sin(rad)

    local forwardVel = car.vx * fwdX + car.vz * fwdZ
    local lateralVel = car.vx * leftX + car.vz * leftZ

    if car.grounded then
        local speedFactor = clamp(math.abs(forwardVel) / 6.0, 0.0, 1.0)
        local reverseSign = forwardVel >= -0.2 and 1.0 or -1.0
        local driftBoost = car.isDrifting and 1.25 or 1.0
        car.yaw = car.yaw + steer * CONFIG.turnSpeed * driftBoost * speedFactor * reverseSign * dt

        rad = degToRad(car.yaw)
        fwdX, fwdZ = math.sin(rad), math.cos(rad)
        leftX, leftZ = math.cos(rad), -math.sin(rad)

        local topSpeed = car.isBoosting and CONFIG.boostSpeed or CONFIG.maxSpeed
        if throttle > 0.0 then
            local accel = car.isBoosting and (CONFIG.acceleration * 1.85) or CONFIG.acceleration
            forwardVel = forwardVel + accel * dt
        elseif throttle < 0.0 then
            if forwardVel > 1.0 then
                forwardVel = forwardVel - CONFIG.acceleration * 1.65 * dt
            else
                forwardVel = forwardVel - CONFIG.reverseAccel * dt
            end
        else
            forwardVel = forwardVel * math.exp(-2.0 * dt)
        end

        if car.isBoosting and throttle >= 0.0 then
            forwardVel = math.max(forwardVel, topSpeed * 0.72)
        end
        forwardVel = clamp(forwardVel, -10.0, topSpeed)

        local grip = car.isDrifting and CONFIG.driftGrip or CONFIG.normalGrip
        lateralVel = lateralVel * math.exp(-grip * dt)

        car.vx = fwdX * forwardVel + leftX * lateralVel
        car.vz = fwdZ * forwardVel + leftZ * lateralVel
    else
        -- Slight mid-air steering control
        car.yaw = car.yaw + steer * 35.0 * dt
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
            car.pitch = lerp(car.pitch, -17.0, 14.0 * dt)
            -- Extra arcade launch kick near ramp lip!
            if groundY > ramp.height * 0.80 and math.abs(forwardVel) > 10.0 then
                car.vy = math.max(car.vy, math.abs(forwardVel) * 0.42 + 4.5)
                car.boostTimer = math.max(car.boostTimer, 0.5)
                car.grounded = false
            end
        else
            if car.y > groundY + 0.15 then
                car.grounded = false
            else
                car.y = groundY
                car.vy = 0.0
                car.pitch = lerp(car.pitch, 0.0, 10.0 * dt)
            end
        end
    end

    if not car.grounded then
        car.airTime = car.airTime + dt
        car.vy = car.vy - CONFIG.gravity * dt
        car.y = car.y + car.vy * dt
        local targetAirPitch = clamp(-car.vy * 2.2, -26.0, 22.0)
        car.pitch = lerp(car.pitch, targetAirPitch, 6.0 * dt)

        if car.y <= groundY then
            car.y = groundY
            car.vy = 0.0
            car.grounded = true
            if car.airTime >= 0.45 then
                stuntJumps = stuntJumps + 1
                local airBonus = math.floor(car.airTime * 150.0)
                score = score + airBonus
                car.boostEnergy = math.min(100.0, car.boostEnergy + 40.0)
                sage.log(string.format(
                    "STUNT JUMP #%d! Air time: %.2fs (+%d pts) | Score: %d",
                    stuntJumps, car.airTime, airBonus, score
                ))
            end
            car.airTime = 0.0
        end
    end

    -- Subtle suspension body roll while drifting/turning
    local targetRoll = clamp(lateralVel * 1.1, -9.0, 9.0)
    car.roll = lerp(car.roll, targetRoll, 9.0 * dt)

    resolveBuildingCollisions()
    updateCrates(dt)
    updateDeliveryMission(dt, timeNow)

    -- Sync 3D .glb car & nitro exhaust flame
    world.setPosition("player_car", car.x, car.y, car.z)
    world.setRotation("player_car", car.pitch, car.yaw, car.roll)

    if car.isBoosting then
        local flicker = 1.6 + 0.55 * math.sin(timeNow * 45.0)
        world.setVisible("car_flame", true)
        world.setPosition("car_flame", car.x - fwdX * 2.75, car.y + 0.45, car.z - fwdZ * 2.75)
        world.setRotation("car_flame", car.pitch, car.yaw + 180.0, 0.0)
        world.setScale("car_flame", 0.85, 0.65, flicker)
    else
        world.setVisible("car_flame", false)
    end

    -- GTA/RAGE-style smooth 3D chase camera behind car
    local speed = math.sqrt(car.vx * car.vx + car.vz * car.vz)
    local desiredCamX = car.x - fwdX * CONFIG.chaseDistance
    local desiredCamY = car.y + CONFIG.chaseHeight + clamp(speed * 0.04, 0.0, 1.4)
    local desiredCamZ = car.z - fwdZ * CONFIG.chaseDistance
    local desiredTargetX = car.x + fwdX * 4.0
    local desiredTargetY = car.y + 1.4
    local desiredTargetZ = car.z + fwdZ * 4.0
    local desiredFov = 60.0 + clamp(speed * 0.35, 0.0, 14.0) + (car.isBoosting and 6.0 or 0.0)

    cam.x = lerp(cam.x, desiredCamX, 5.5 * dt)
    cam.y = lerp(cam.y, desiredCamY, 5.5 * dt)
    cam.z = lerp(cam.z, desiredCamZ, 5.5 * dt)
    cam.tx = lerp(cam.tx, desiredTargetX, 10.0 * dt)
    cam.ty = lerp(cam.ty, desiredTargetY, 10.0 * dt)
    cam.tz = lerp(cam.tz, desiredTargetZ, 10.0 * dt)
    cam.fov = lerp(cam.fov, desiredFov, 5.0 * dt)

    camera.setPosition(cam.x, cam.y, cam.z)
    camera.setTarget(cam.tx, cam.ty, cam.tz)
    camera.setFov(cam.fov)

    -- Update window title HUD 4x/sec
    titleUpdateAccum = titleUpdateAccum + dt
    if titleUpdateAccum >= 0.25 then
        titleUpdateAccum = 0.0
        local kmh = math.floor(speed * 3.6)
        local tag = ""
        if not car.grounded then
            tag = " | [AIR TIME!]"
        elseif car.isBoosting then
            tag = " | [NITRO!]"
        elseif car.isDrifting and math.abs(lateralVel) > 2.5 then
            tag = " | [DRIFT!]"
        end
        sage.setTitle(string.format(
            "SAGE 3D Stunt & Delivery | %d km/h | Nitro: %d%% | Deliveries: %d | Score: %d%s",
            kmh, math.floor(car.boostEnergy), deliveriesDone, score, tag
        ))
    end
end

function onShutdown()
    sage.log("3D Stunt & Delivery Sandbox shutting down cleanly.")
end
