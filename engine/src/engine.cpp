#include "sage/engine.h"

#include "sage/world/scene_loader.h"

#include <algorithm>
#include <cmath>
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
    std::cout << "  SAGE Engine v0.5.0 (Shadows + glTF Anim + Jolt + Audio + UI)\n";
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
    callbacks.getTimeSeconds = [this]() -> double {
        return Window::getTimeSeconds() - m_startTime;
    };
    callbacks.beforeResourcesStart = [this]() {
        if (!m_resolvedScenePath.empty() && fs::exists(m_resolvedScenePath)) {
            SceneLoader::loadFromFile(m_resolvedScenePath, m_world);
        }
    };
    callbacks.isUiCapturingKeyboard = [this]() -> bool {
        return m_editor.wantsCaptureKeyboard();
    };
    callbacks.isUiCapturingMouse = [this]() -> bool {
        return m_editor.wantsCaptureMouse();
    };

    m_lua.initialize(resolvedResources, m_world, m_physics, m_audio, m_input, std::move(callbacks));
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
    m_physics.step(static_cast<float>(dt), m_world);
    m_world.updateAnimations(static_cast<float>(dt));
}

void Engine::render(double elapsedSeconds) {
    const ClearColor clearColor{0.05f, 0.07f, 0.11f, 1.0f};
    m_renderer->beginFrame(clearColor);
    m_renderer->renderWorld(m_world, static_cast<float>(elapsedSeconds));
    m_renderer->endFrame();

    EditorStats stats;
    stats.fps = m_smoothedFps;
    stats.frameMs = m_lastFrameMs;
    stats.gpuRenderer = m_renderer->info().gpuRenderer;
    stats.loadedModels = m_renderer->loadedModelCount();
    stats.luaResources = m_lua.activeResourceCount();
    stats.freeFlyMode = m_freeFlyMode;

    EditorActions actions;
    actions.onHotReload = [this]() {
        m_scriptTitle.clear();
        m_lua.reloadAll();
    };
    actions.onToggleFreeFly = [this]() {
        m_freeFlyMode = !m_freeFlyMode;
        if (m_freeFlyMode) {
            syncFreeFlyFromWorldCamera();
        }
    };
    actions.onTriggerShockwave = [this]() {
        const Vec3 carPos = m_world.hasEntity("player_car")
            ? m_world.getPosition("player_car")
            : m_world.camera().target;
        m_physics.applyExplosion(carPos, 24.0f, 360.0f);
        m_audio.playExplosion();
    };
    actions.onRenderCustomUi = [this]() {
        m_lua.renderUI();
    };

    m_editor.render(m_world, m_physics, m_audio, stats, actions);
    m_window.swapBuffers();
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
        const double frameDelta = std::min(currentTime - previousTime, kMaxFrameDelta);
        previousTime = currentTime;
        m_lastFrameMs = static_cast<float>(frameDelta * 1000.0);

        m_window.pollEvents(m_input);

        if (m_window.wasResized()) {
            m_renderer->onResize(m_window.width(), m_window.height());
            m_window.clearResizedFlag();
        }

        if (m_input.wasKeyPressed(Key::Escape)) {
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

        render(currentTime - m_startTime);

        ++totalFrames;
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
            std::cout << "[SAGE] v0.4 Smoke test completed (" << totalFrames
                      << " frames, " << m_lua.activeResourceCount()
                      << " Lua resource(s), " << m_world.entityCount()
                      << " entities, " << m_renderer->loadedModelCount()
                      << " .glb model(s), " << m_physics.bodyCount()
                      << " Jolt bodies).\n";
            break;
        }
    }

    m_running = false;
    shutdown();
    std::cout << "[SAGE] Engine shut down cleanly.\n";
    return 0;
}

} // namespace sage
