-- SAGE Engine Starter Lua Resource (Target API — wired up in Milestone v0.2!)
-- Designed with FiveM-style clarity so humans and AI can script effortlessly.

local angle = 0.0

function onStart()
    sage.log("Starter sandbox Lua resource loaded!")
end

function onTick(dt)
    -- Slowly spin the demo cube every frame
    angle = angle + (45.0 * dt)
    -- Coming in v0.2/v0.3:
    -- world.setRotation("demo_cube", 0.0, angle, 0.0)
end

function onShutdown()
    sage.log("Starter sandbox shutting down.")
end
