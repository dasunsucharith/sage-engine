#include "sage/engine.h"

#include "sage/version.h"
#include "sage/world/scene_loader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace sage {

namespace fs = std::filesystem;

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
} // namespace

Engine::Engine(const EngineConfig& config)
    : m_config(config) {}

Engine::~Engine() {
    shutdown();
}

fs::path Engine::resolveProjectPath(const fs::path& relativePath) const {
    if (fs::exists(relativePath)) {
        return fs::weakly_canonical(relativePath);
    }
    const fs::path parentCandidate = fs::path("..") / relativePath;
    if (fs::exists(parentCandidate)) {
        return fs::weakly_canonical(parentCandidate);
    }
    return relativePath;
}

void Engine::syncFreeFlyFromWorldCamera() {
    const CameraState& cam = m_world.camera();
    m_flyPos = cam.position;
    const float dx = cam.target.x - cam.position.x;
    const float dy = cam.target.y - cam.position.y;
    const float dz = cam.target.z - cam.position.z;
    const float horiz = std::sqrt(dx * dx + dz * dz);
    m_flyYawDeg = std::atan2(dx, dz) * kRadToDeg;
    m_flyPitchDeg = std::clamp(std::atan2(dy, std::max(horiz, 0.001f)) * kRadToDeg, -85.0f, 85.0f);
}

void Engine::updateFreeFlyCamera(float dt) {
    if (!m_editor.wantsCaptureMouse()) {
        if (m_input.isMouseButtonDown(MouseButton::Right) || m_input.isMouseButtonDown(MouseButton::Left)) {
            constexpr float kMouseSensitivity = 0.22f;
            m_flyYawDeg -= m_input.mouseDeltaX() * kMouseSensitivity;
            m_flyPitchDeg = std::clamp(m_flyPitchDeg - m_input.mouseDeltaY() * kMouseSensitivity, -85.0f, 85.0f);
        }
        if (m_input.mouseWheelDelta() != 0.0f) {
            m_flySpeed = std::clamp(m_flySpeed * (1.0f + m_input.mouseWheelDelta() * 0.15f), 4.0f, 120.0f);
        }
    }

    const float yawRad = m_flyYawDeg * kDegToRad;
    const float pitchRad = m_flyPitchDeg * kDegToRad;
    const float cp = std::cos(pitchRad);

    const Vec3 forward{std::sin(yawRad) * cp, std::sin(pitchRad), std::cos(yawRad) * cp};
    const Vec3 right{-std::cos(yawRad), 0.0f, std::sin(yawRad)};

    if (!m_editor.wantsCaptureKeyboard()) {
        float speed = m_flySpeed;
        if (m_input.isKeyDown(Key::LeftShift) || m_input.isKeyDown(Key::RightShift)) {
            speed *= 2.5f;
        }
        const float step = speed * dt;
        if (m_input.isKeyDown(Key::W) || m_input.isKeyDown(Key::Up)) {
            m_flyPos.x += forward.x * step;
            m_flyPos.y += forward.y * step;
            m_flyPos.z += forward.z * step;
        }
        if (m_input.isKeyDown(Key::S) || m_input.isKeyDown(Key::Down)) {
            m_flyPos.x -= forward.x * step;
            m_flyPos.y -= forward.y * step;
            m_flyPos.z -= forward.z * step;
        }
        if (m_input.isKeyDown(Key::D) || m_input.isKeyDown(Key::Right)) {
            m_flyPos.x += right.x * step;
            m_flyPos.z += right.z * step;
        }
        if (m_input.isKeyDown(Key::A) || m_input.isKeyDown(Key::Left)) {
            m_flyPos.x -= right.x * step;
            m_flyPos.z -= right.z * step;
        }
        if (m_input.isKeyDown(Key::E) || m_input.isKeyDown(Key::Space)) {
            m_flyPos.y += step;
        }
        if (m_input.isKeyDown(Key::Q) || m_input.isKeyDown(Key::LeftControl)) {
            m_flyPos.y -= step;
        }
    }

    m_world.camera().position = m_flyPos;
    m_world.camera().target = {
        m_flyPos.x + forward.x * 10.0f,
        m_flyPos.y + forward.y * 10.0f,
        m_flyPos.z + forward.z * 10.0f,
    };
}

bool Engine::checkSceneHotReload() {
    if (m_resolvedScenePath.empty() || !fs::exists(m_resolvedScenePath)) {
        return false;
    }
    std::error_code ec;
    const auto currentWriteTime = fs::last_write_time(m_resolvedScenePath, ec);
    if (ec) {
        return false;
    }
    if (!m_hasSceneWriteTime) {
        m_sceneWriteTime = currentWriteTime;
        m_hasSceneWriteTime = true;
        return false;
    }
    if (currentWriteTime != m_sceneWriteTime) {
        m_sceneWriteTime = currentWriteTime;
        std::cout << "\n[SAGE][HotReload] Scene change detected in: " << m_resolvedScenePath.string() << "\n";
        m_scriptTitle.clear();
        m_lua.reloadAll();
        return true;
    }
    return false;
}

bool Engine::initialize() {
    std::cout << "========================================\n";
    std::cout << "  SAGE Engine v" << kEngineVersion << " (" << buildConfigName() << " build)\n";
    std::cout << "  App: " << m_config.appName << "\n";
    std::cout << "  Window: " << m_config.windowWidth << "x" << m_config.windowHeight
              << " (VSync: " << (m_config.vsync ? "ON" : "OFF") << ")\n";
    std::cout << "========================================\n";

    WindowConfig windowConfig;
    windowConfig.title = m_config.appName;
    windowConfig.width = m_config.windowWidth;
    windowConfig.height = m_config.windowHeight;
    windowConfig.vsync = m_config.vsync;
    windowConfig.resizable = true;

    if (!m_window.initialize(windowConfig)) {
        std::cerr << "[SAGE] Failed to initialize platform window.\n";
        return false;
    }

    const fs::path resolvedResources = resolveProjectPath(m_config.resourcesPath);
    m_projectRoot = resolvedResources.parent_path();
    m_resolvedScenePath = resolveProjectPath(m_config.scenePath);

    m_renderer = createOpenGLRenderer();
    if (!m_renderer || !m_renderer->initialize(m_window.width(), m_window.height())) {
        std::cerr << "[SAGE] Failed to initialize 3D GPU renderer.\n";
        shutdown();
        return false;
    }
    m_renderer->setAssetRoot(m_projectRoot);

    const RendererInfo gpu = m_renderer->info();
    std::cout << "[SAGE][GPU] Backend : " << gpu.backendName << "\n";
    std::cout << "[SAGE][GPU] Vendor  : " << gpu.gpuVendor << "\n";
    std::cout << "[SAGE][GPU] Renderer: " << gpu.gpuRenderer << "\n";
    std::cout << "[SAGE][GPU] Version : " << gpu.apiVersion << "\n";

    if (!m_physics.initialize()) {
        std::cerr << "[SAGE] Failed to initialize Jolt Physics.\n";
        shutdown();
        return false;
    }

    m_audio.initialize(m_config.silentAudio);
    m_audio.setAssetRoot(m_projectRoot);

    if (m_editor.initialize(m_window)) {
        m_window.setRawEventCallback([this](const void* sdlEvent) {
            m_editor.processEvent(sdlEvent);
        });
    }

    m_startTime = Window::getTimeSeconds();

    if (fs::exists(m_resolvedScenePath)) {
        std::error_code ec;
        m_sceneWriteTime = fs::last_write_time(m_resolvedScenePath, ec);
        m_hasSceneWriteTime = !ec;
    }

    std::cout << "[SAGE][Lua] Scanning resources in: " << resolvedResources.string() << "\n";

    LuaHostCallbacks callbacks;
    callbacks.setWindowTitle = [this](const std::string& title) {
        m_scriptTitle = title;
    };
    // sage.getTime() / world.sampleWater() see SIMULATION time so every fixed tick
    // (including catch-up ticks within one frame) advances gameplay consistently.
    callbacks.getTimeSeconds = [this]() -> double {
        return m_simTime;
    };
    callbacks.getRealTimeSeconds = [this]() -> double {
        return Window::getTimeSeconds() - m_startTime;
    };
    callbacks.beforeResourcesStart = [this]() {
        if (!m_resolvedScenePath.empty() && fs::exists(m_resolvedScenePath)) {
            SceneLoader::loadFromFile(m_resolvedScenePath, m_world, &m_physics);
        }
    };
    callbacks.requestQuit = [this]() {
        m_window.requestClose();
    };
    callbacks.isUiCapturingKeyboard = [this]() -> bool {
        return m_editor.wantsCaptureKeyboard();
    };
    callbacks.isUiCapturingMouse = [this]() -> bool {
        return m_editor.wantsCaptureMouse();
    };

    m_lua.setResourceFilter(m_config.onlyResource);
    m_lua.initialize(resolvedResources, m_world, m_physics, m_audio, m_input, std::move(callbacks));
    if (m_renderer) {
        // Pre-warm all scene & Lua .glb models, 3D terrain & instanced grass BEFORE Frame 1 (eliminates v1.3 226ms hitch!)
        m_renderer->preloadModels(m_world);
    }
    return true;
}

void Engine::shutdown() {
    m_lua.shutdown();
    m_window.setRawEventCallback(nullptr);
    m_editor.shutdown();
    m_audio.shutdown();
    m_physics.shutdown();
    m_world.clear();
    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }
    m_window.shutdown();
}

void Engine::fixedTick(double dt) {
    if (!m_freeFlyMode) {
        m_lua.tick(dt);
    }
    // Physics samples the animated water at simulation time (never wall-clock), so the
    // same inputs always produce the same buoyancy result regardless of frame pacing.
    m_physics.step(static_cast<float>(dt), m_world, static_cast<float>(m_simTime));
    m_world.updateAnimations(static_cast<float>(dt));
    m_simTime += dt;
}

void Engine::render(double elapsedSeconds) {
    if (m_config.overrideCamera) {
        m_world.camera().position = m_config.overrideCamPos;
        m_world.camera().target = m_config.overrideCamTarget;
    }
    const ClearColor clearColor{0.05f, 0.07f, 0.11f, 1.0f};
    m_renderer->beginFrame(clearColor);
    m_renderer->renderWorld(m_world, static_cast<float>(elapsedSeconds));
    m_renderer->endFrame();

    if (!m_config.screenshotPath.empty() && m_config.maxFrames > 0) {
        // Captured on the final frame right before ImGui overlay so 3D world is 100% unobstructed!
    }

    EditorStats stats;
    stats.fps = m_smoothedFps;
    stats.frameMs = m_lastFrameMs;
    stats.gpuRenderer = m_renderer->info().gpuRenderer;
    stats.assetsDir = (m_projectRoot / "assets").string();
    stats.loadedModels = m_renderer->loadedModelCount();
    stats.grassClumps = m_renderer->grassInstanceCount();
    stats.culledEntities = m_renderer->lastCulledCount();
    stats.drawCalls = m_renderer->lastDrawCallCount();
    stats.instancedBatches = m_renderer->lastInstancedBatchCount();
    stats.instancedProps = m_renderer->lastInstancedPropCount();
    stats.savedDrawCalls = m_renderer->lastSavedDrawCalls();
    stats.luaResources = m_lua.activeResourceCount();
    stats.freeFlyMode = m_freeFlyMode;

    EditorActions actions;
    actions.onHotReload = [this]() {
        m_scriptTitle.clear();
        m_lua.reloadAll();
        if (m_renderer) {
            m_renderer->preloadModels(m_world);
        }
    };
    actions.onToggleFreeFly = [this]() {
        m_freeFlyMode = !m_freeFlyMode;
        if (m_freeFlyMode) {
            syncFreeFlyFromWorldCamera();
        }
    };
    actions.onTriggerShockwave = [this]() {
        // Engine code must not know game entity names: blast at whatever the camera looks at.
        m_physics.applyExplosion(m_world.camera().target, 24.0f, 360.0f);
        m_audio.playExplosion();
    };
    actions.onSaveScene = [this]() -> bool {
        const fs::path exportPath = m_projectRoot / "scenes" / "exported_scene.json";
        return SceneLoader::saveToFile(exportPath, m_world, std::string("SAGE v") + kEngineVersion + " Exported World");
    };
    actions.onRenderCustomUi = [this]() {
        m_lua.renderUI();
    };

    m_editor.render(m_world, m_physics, m_audio, stats, actions);
    m_window.swapBuffers();
}

namespace {

// Steady-state statistics over frames AFTER the warm-up window. Percentiles matter
// more than averages for games: p99 captures the hitches players actually feel.
nlohmann::json computeFrameStats(const std::vector<float>& frameTimesMs, int warmupFrames, bool vsync) {
    const std::size_t skip = static_cast<std::size_t>(std::max(0, warmupFrames));
    if (frameTimesMs.size() <= skip) {
        return {{"measuredFrames", 0}, {"warmupFrames", skip}};
    }
    std::vector<float> samples(frameTimesMs.begin() + static_cast<std::ptrdiff_t>(skip), frameTimesMs.end());
    std::sort(samples.begin(), samples.end());

    double sum = 0.0;
    for (float s : samples) {
        sum += s;
    }
    const auto percentile = [&samples](double p) -> double {
        // Nearest-rank percentile on the sorted samples.
        const double rank = p / 100.0 * static_cast<double>(samples.size() - 1);
        return samples[static_cast<std::size_t>(std::lround(rank))];
    };
    const double mean = sum / static_cast<double>(samples.size());

    nlohmann::json stats;
    stats["warmupFrames"] = skip;
    stats["measuredFrames"] = samples.size();
    stats["meanFrameMs"] = mean;
    stats["p50FrameMs"] = percentile(50.0);
    stats["p95FrameMs"] = percentile(95.0);
    stats["p99FrameMs"] = percentile(99.0);
    stats["maxFrameMs"] = samples.back();
    stats["meanFps"] = mean > 0.001 ? 1000.0 / mean : 0.0;
    if (samples.size() < 300) {
        stats["warning"] = "Fewer than 300 measured frames: too few for reliable comparisons. Use --benchmark.";
    }
    if (vsync) {
        stats["vsyncWarning"] = "VSync was ON: frame times are capped by the display refresh rate.";
    }
    return stats;
}

} // namespace

void Engine::writeSessionTelemetry(int totalFrames, double totalSeconds) const {
    if (m_projectRoot.empty() || totalFrames <= 0) {
        return;
    }
    const fs::path logsDir = m_projectRoot / "logs";
    std::error_code ec;
    fs::create_directories(logsDir, ec);
    const fs::path outPath = logsDir / "last_session_telemetry.json";

    const double avgMs = m_frameMsAccum / static_cast<double>(totalFrames);
    const double avgFps = avgMs > 0.01 ? (1000.0 / avgMs) : 0.0;
    const RendererInfo gpu = m_renderer ? m_renderer->info() : RendererInfo{};
    const SkySettings& sky = m_world.sky();

    nlohmann::json j;
    j["engineVersion"] = kEngineVersion;
    j["buildConfig"] = buildConfigName();
    j["runMode"] = m_config.benchmarkMode ? "benchmark" : (m_config.maxFrames > 0 ? "smoke-test" : "interactive");
    j["smokeTest"] = (m_config.maxFrames > 0 && !m_config.benchmarkMode);
    j["vsync"] = m_config.vsync;
    j["gpu"] = {
        {"renderer", gpu.gpuRenderer},
        {"vendor", gpu.gpuVendor},
        {"backend", gpu.backendName}
    };
    j["performance"] = {
        {"totalFrames", totalFrames},
        {"sessionSeconds", totalSeconds},
        {"avgFrameMs", avgMs},
        {"peakFrameMs", m_peakFrameMs},
        {"avgFps", avgFps},
        {"lastDrawCalls", m_renderer ? m_renderer->lastDrawCallCount() : 0},
        {"instancedBatches", m_renderer ? m_renderer->lastInstancedBatchCount() : 0},
        {"instancedProps", m_renderer ? m_renderer->lastInstancedPropCount() : 0},
        {"savedDrawCalls", m_renderer ? m_renderer->lastSavedDrawCalls() : 0}
    };
    j["performance"]["note"] =
        "avgFrameMs/peakFrameMs include cold-start frames. Compare versions ONLY with steadyState from a "
        "Release --benchmark run (VSync off); smoke-test numbers are dominated by the first-frame hitch.";
    j["steadyState"] = computeFrameStats(m_frameTimesMs, m_config.warmupFrames, m_config.vsync);
    if (m_config.benchmarkMode && j["steadyState"].value("measuredFrames", 0) > 0) {
        const auto& s = j["steadyState"];
        std::cout << std::fixed << std::setprecision(2)
                  << "[SAGE][Benchmark] " << s["measuredFrames"].get<std::size_t>() << " frames after "
                  << s["warmupFrames"].get<std::size_t>() << " warm-up | mean " << s["meanFrameMs"].get<double>()
                  << " ms | p50 " << s["p50FrameMs"].get<double>() << " | p95 " << s["p95FrameMs"].get<double>()
                  << " | p99 " << s["p99FrameMs"].get<double>() << " | max " << s["maxFrameMs"].get<double>()
                  << " ms (" << buildConfigName() << ", VSync " << (m_config.vsync ? "ON" : "OFF") << ")\n";
    }
    j["world"] = {
        {"entities", m_world.entityCount()},
        {"loadedModels", m_renderer ? m_renderer->loadedModelCount() : 0},
        {"grassClumps", m_renderer ? m_renderer->grassInstanceCount() : 0},
        {"lastCulledEntities", m_renderer ? m_renderer->lastCulledCount() : 0},
        {"activeRagdolls", m_world.activeRagdollCount()},
        {"joltBodies", m_physics.bodyCount()},
        {"dynamicBodies", m_physics.dynamicBodyCount()},
        {"pointLights", m_world.pointLights().size()},
        {"luaResources", m_lua.activeResourceCount()},
        {"luaReloads", m_lua.reloadCount()}
    };
    j["features"] = {
        {"terrain3D", m_world.terrain().enabled},
        {"instancedProps", sky.instancedPropsEnabled},
        {"grassInstanced", sky.grassEnabled},
        {"ssr", sky.ssrEnabled},
        {"godRays", sky.godRaysEnabled},
        {"cloudShadows", sky.cloudShadowsEnabled},
        {"fxaa", sky.fxaaEnabled},
        {"ssao", sky.ssaoEnabled},
        {"bloom", sky.bloomEnabled},
        {"shadows3Cascade", sky.shadowsEnabled}
    };

    std::ofstream out(outPath, std::ios::trunc);
    if (out.is_open()) {
        out << j.dump(2) << "\n";
        std::cout << "[SAGE][AI-Telemetry] Saved session metrics -> " << outPath.string() << "\n";
    }
}

void Engine::updateWindowTitle(double fps) {
    std::ostringstream oss;
    if (m_freeFlyMode) {
        oss << "SAGE 3D Free-Fly Camera (Drag Mouse + WASD/Q/E, Tab = Drive) | "
            << std::fixed << std::setprecision(0) << fps << " FPS";
    } else if (!m_scriptTitle.empty()) {
        oss << m_scriptTitle << " | " << std::fixed << std::setprecision(0) << fps << " FPS";
    } else {
        oss << m_config.appName
            << " | " << std::fixed << std::setprecision(0) << fps << " FPS"
            << " | F1 Editor | F5 Reload";
    }
    m_window.setTitle(oss.str());
}

int Engine::run() {
    if (!initialize()) {
        return 1;
    }

    m_running = true;

    double previousTime = Window::getTimeSeconds();
    double accumulator = 0.0;

    double fpsTimer = 0.0;
    double hotReloadTimer = 0.0;
    int fpsFrames = 0;
    int totalFrames = 0;

    constexpr double kMaxFrameDelta = 0.25;
    constexpr double kHotReloadInterval = 0.50;

    while (m_running && !m_window.shouldClose()) {
        const double currentTime = Window::getTimeSeconds();
        const double rawFrameDelta = currentTime - previousTime;
        // Simulation catch-up is clamped (avoids the "spiral of death" after a long hitch)...
        const double frameDelta = std::min(rawFrameDelta, kMaxFrameDelta);
        previousTime = currentTime;
        // ...but telemetry records the REAL frame time, otherwise every hitch reads as exactly 250 ms.
        m_lastFrameMs = static_cast<float>(rawFrameDelta * 1000.0);
        m_frameMsAccum += static_cast<double>(m_lastFrameMs);
        if (totalFrames > 1 && m_lastFrameMs > m_peakFrameMs) {
            m_peakFrameMs = m_lastFrameMs;
        }
        if (totalFrames > 0) {
            // Frame 0's delta is the gap between initialize() and the first loop pass, not a frame.
            m_frameTimesMs.push_back(m_lastFrameMs);
        }

        m_window.pollEvents(m_input);

        if (m_window.wasResized()) {
            m_renderer->onResize(m_window.width(), m_window.height());
            m_window.clearResizedFlag();
        }

        // Only handle Escape at engine level if no Lua resource is active (otherwise Lua manages Pause Menu!)
        if (m_input.wasKeyPressed(Key::Escape) && m_lua.activeResourceCount() == 0) {
            m_window.requestClose();
            break;
        }

        if (m_input.wasKeyPressed(Key::F1)) {
            m_editor.toggleVisible();
        }

        if (m_input.wasKeyPressed(Key::Tab)) {
            m_freeFlyMode = !m_freeFlyMode;
            if (m_freeFlyMode) {
                syncFreeFlyFromWorldCamera();
                std::cout << "[SAGE][Camera] Free-Fly 3D Camera ON\n";
            } else {
                std::cout << "[SAGE][Camera] Drive Camera ON\n";
            }
        }

        if (m_input.wasKeyPressed(Key::F5)) {
            std::cout << "\n[SAGE][HotReload] Manual reload triggered (F5)...\n";
            m_scriptTitle.clear();
            m_lua.reloadAll();
            if (m_renderer) {
                m_renderer->preloadModels(m_world);
            }
        }

        hotReloadTimer += frameDelta;
        if (hotReloadTimer >= kHotReloadInterval) {
            hotReloadTimer = 0.0;
            if (!checkSceneHotReload() && m_lua.checkHotReload()) {
                m_scriptTitle.clear();
            }
        }

        if (m_freeFlyMode) {
            updateFreeFlyCamera(static_cast<float>(frameDelta));
        }

        accumulator += frameDelta;
        while (accumulator >= m_config.fixedTimeStep) {
            fixedTick(m_config.fixedTimeStep);
            accumulator -= m_config.fixedTimeStep;
        }

        // Shader time = simulation time + the not-yet-simulated remainder, so GPU water waves
        // stay in lock-step with the CPU buoyancy samples while still animating smoothly.
        m_renderTime = m_simTime + accumulator;
        render(m_renderTime);

        ++totalFrames;
        if (!m_config.screenshotPath.empty() && m_config.maxFrames > 0 && totalFrames == m_config.maxFrames) {
            // Re-render pure 3D world cleanly (no MENU overlay) & read back exact backbuffer PNG!
            m_renderer->beginFrame({0.05f, 0.07f, 0.11f, 1.0f});
            m_renderer->renderWorld(m_world, static_cast<float>(m_renderTime));
            m_renderer->endFrame();
            m_renderer->captureScreenshot(m_config.screenshotPath);
        }
        ++fpsFrames;
        fpsTimer += frameDelta;
        if (fpsTimer >= 0.25) {
            const double fps = static_cast<double>(fpsFrames) / fpsTimer;
            m_smoothedFps = static_cast<float>(fps);
            updateWindowTitle(fps);
            fpsTimer = 0.0;
            fpsFrames = 0;
        }

        if (m_config.maxFrames > 0 && totalFrames >= m_config.maxFrames) {
            std::cout << "[SAGE] v" << kEngineVersion
                      << (m_config.benchmarkMode ? " Benchmark" : " Smoke test")
                      << " completed (" << totalFrames
                      << " frames, " << m_lua.activeResourceCount()
                      << " Lua resource(s), " << m_world.entityCount()
                      << " entities, " << m_renderer->loadedModelCount()
                      << " .glb model(s), " << m_renderer->lastInstancedBatchCount()
                      << " instanced batches (" << m_renderer->lastSavedDrawCalls()
                      << " draw calls saved), " << m_renderer->grassInstanceCount()
                      << " grass clumps, " << m_renderer->lastCulledCount()
                      << " culled, " << m_physics.bodyCount()
                      << " Jolt bodies, " << m_world.pointLights().size()
                      << " dynamic light(s)).\n";
            break;
        }
    }

    writeSessionTelemetry(totalFrames, Window::getTimeSeconds() - m_startTime);
    m_running = false;
    shutdown();
    std::cout << "[SAGE] Engine shut down cleanly.\n";
    return 0;
}

} // namespace sage
