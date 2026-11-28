#pragma once

#include "sage/audio/audio_engine.h"
#include "sage/editor/editor_ui.h"
#include "sage/physics/physics_world.h"
#include "sage/platform/input.h"
#include "sage/platform/window.h"
#include "sage/profiling/profiler.h"
#include "sage/renderer/renderer.h"
#include "sage/scripting/lua_engine.h"
#include "sage/world/world.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sage {

struct EngineConfig {
    std::string appName = "SAGE Engine";
    int windowWidth = 1280;
    int windowHeight = 720;
    bool vsync = true;
    double fixedTimeStep = 1.0 / 60.0;
    int maxFrames = 0;                 // 0 = run until window closes (non-zero for --smoke-test / --benchmark)
    int warmupFrames = 0;              // Leading frames excluded from steady-state frame-time stats
    bool benchmarkMode = false;        // true for --benchmark (VSync off, long measured window)
    bool renderInterpolation = true;
    bool lightTileCulling = true;      // Tiled point-light culling; --no-light-tiles disables (A/B testing)   // Blend the last two 60 Hz ticks when drawing (smooth on 120/144 Hz); --no-interp disables
    bool silentAudio = false;          // true during --smoke-test
    std::filesystem::path resourcesPath = "resources";
    std::filesystem::path scenePath = "scenes/stunt_city.json";
    std::string onlyResource;          // Optional: load only this single Lua resource (e.g., --resource survival_game)
    std::filesystem::path screenshotPath; // Optional: save a PNG of the 3D rendered frame on exit
    bool overrideCamera = false;
    Vec3 overrideCamPos{};
    Vec3 overrideCamTarget{};
};

class Engine {
public:
    explicit Engine(const EngineConfig& config);
    ~Engine();

    int run();

private:
    bool initialize();
    void shutdown();
    void fixedTick(double dt);
    void updateFreeFlyCamera(float dt);
    void syncFreeFlyFromWorldCamera();
    void render(double elapsedSeconds);
    void renderWorldInterpolated(const ClearColor& clearColor, double elapsedSeconds);
    void updateWindowTitle(double fps);
    bool checkSceneHotReload();
    void writeSessionTelemetry(int totalFrames, double totalSeconds) const;
    void recordGpuPassTimings();

    std::filesystem::path resolveProjectPath(const std::filesystem::path& relativePath) const;

    EngineConfig m_config;
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_resolvedScenePath;
    std::filesystem::file_time_type m_sceneWriteTime{};
    bool m_hasSceneWriteTime = false;

    Window m_window;
    InputState m_input;
    World m_world;
    PhysicsWorld m_physics;
    AudioEngine m_audio;
    EditorUI m_editor;
    LuaEngine m_lua;
    std::unique_ptr<Renderer> m_renderer;

    bool m_freeFlyMode = false;
    Vec3 m_flyPos{0.0f, 12.0f, -24.0f};
    float m_flyYawDeg = 0.0f;
    float m_flyPitchDeg = -20.0f;
    float m_flySpeed = 24.0f;

    float m_smoothedFps = 60.0f;
    float m_lastFrameMs = 16.6f;
    float m_peakFrameMs = 0.0f;
    double m_frameMsAccum = 0.0;
    std::vector<float> m_frameTimesMs;  // Raw (unclamped) per-frame CPU wall times for percentile telemetry
    // Named per-frame timings collected after warm-up in --benchmark / --smoke-test runs.
    // Every frame records a value for every known name (0 if it didn't run that frame),
    // so means are true per-frame costs and columns stay aligned.
    struct SampleSeries {
        std::vector<std::string> names;          // First-seen order
        std::vector<std::vector<float>> samples; // samples[i] belongs to names[i]
        std::size_t frames = 0;
        void add(const std::vector<TimingSample>& frame);
    };
    SampleSeries m_gpuPassSeries;        // GPU ms per render pass
    SampleSeries m_cpuSectionSeries;     // CPU ms per engine-loop section (exclusive)
    SampleSeries m_cpuRenderPassSeries;  // CPU ms the renderer spent issuing each pass
    std::vector<float> m_gpuTotalSamples;
    CpuProfiler m_cpuProfiler;
    std::uint64_t m_lastGpuTimingId = 0;

    // Simulation clock: advances by exactly fixedTimeStep per fixedTick, so physics,
    // water waves and Lua gameplay all see the same deterministic time (never wall-clock).
    double m_simTime = 0.0;
    double m_renderTime = 0.0;         // m_simTime + leftover accumulator (smooth time for shaders)
    float m_interpAlpha = 1.0f;        // accumulator / fixedTimeStep, blend factor for render interpolation

    std::string m_scriptTitle;
    double m_startTime = 0.0;
    bool m_running = false;
};

} // namespace sage
