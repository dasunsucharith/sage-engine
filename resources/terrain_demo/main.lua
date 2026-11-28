-- terrain_demo: a slow orbiting camera for previewing data-driven terrain scenes.
-- Run:  .\build\sage.exe --scene scenes/terrain_demo.json --resource terrain_demo
-- Press Tab for the free-fly camera, F1 for the editor. Edit the scene's "terrain"
-- block and save: the scene hot-reloads and the landscape rebuilds instantly.

local orbit = { radius = 300.0, height = 95.0, speed = 0.05, targetY = 14.0 }

function onStart()
    sage.setTitle("SAGE Data-Driven Terrain Demo (Tab = free-fly, F1 = editor)")
    sage.log("Terrain demo: edit the \"terrain\" block of the scene JSON and save to rebuild live.")
end

function onTick(dt)
    local a = sage.getTime() * orbit.speed + 2.3
    camera.setPosition(math.cos(a) * orbit.radius, orbit.height, math.sin(a) * orbit.radius)
    camera.setTarget(0.0, orbit.targetY, 0.0)
end

function onRenderUI()
    local x, z = 0.0, 0.0
    local h, _, _, _, biome = world.sampleTerrain(x, z)
    if ui.beginWindow("Terrain", 16, 16, 260, 70) then
        ui.text(string.format("Height at island centre: %.1f m", h))
        ui.text("Layer index: " .. tostring(biome))
    end
    ui.endWindow() -- Dear ImGui needs End() even when Begin() returns false
end
