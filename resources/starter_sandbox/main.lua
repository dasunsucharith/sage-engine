-- SAGE Engine Starter Lua Resource Template (v0.2)
-- Enable this in manifest.json ("enabled": true) to test a minimal spinning box!

local angle = 0.0

function onStart()
    world.spawnBox("demo_cube", 0.0, 0.0, 0.0)
    world.setScale("demo_cube", 4.0, 1.0, 4.0)
    world.setColor("demo_cube", 0.9, 0.25, 0.2)
    camera.setTarget(0.0, 0.0)
    camera.setZoom(20.0)
    sage.log("Starter sandbox Lua resource loaded!")
end

function onTick(dt)
    angle = angle + (45.0 * dt)
    world.setRotation("demo_cube", 0.0, angle, 0.0)
end

function onShutdown()
    sage.log("Starter sandbox shutting down.")
end
