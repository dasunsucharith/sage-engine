#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "sage/profiling/profiler.h"

namespace sage {

class World;

struct ClearColor {
    float r = 0.06f;
    float g = 0.08f;
    float b = 0.12f;
    float a = 1.0f;
};

// Time spent in one render pass, e.g. "shadows", "opaque", "post".
using GpuPassTiming = TimingSample;

struct RendererInfo {
    std::string backendName;
    std::string gpuVendor;
    std::string gpuRenderer;
    std::string apiVersion;
};

// Backend-agnostic 3D graphics renderer interface.
// Starts with OpenGL 3.3 Core + glTF 2.0 (.glb) support and stays decoupled
// so Vulkan / DX12 backends can drop in cleanly later.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool initialize(int viewportWidth, int viewportHeight) = 0;
    virtual void shutdown() = 0;

    virtual void setAssetRoot(const std::filesystem::path& assetRoot) = 0;
    virtual void preloadModels(const World& world) = 0;
    virtual void onResize(int width, int height) = 0;
    virtual void beginFrame(const ClearColor& clearColor) = 0;
    virtual void renderWorld(const World& world, float timeSeconds) = 0;
    virtual void endFrame() = 0;
    virtual bool captureScreenshot(const std::filesystem::path& outputPath) const {
        (void)outputPath;
        return false;
    }

    virtual RendererInfo info() const = 0;
    virtual std::size_t loadedModelCount() const = 0;
    virtual std::size_t grassInstanceCount() const { return 0; }
    virtual std::size_t lastCulledCount() const { return 0; }
    virtual std::size_t lastDrawCallCount() const { return 0; }
    virtual std::size_t lastInstancedBatchCount() const { return 0; }
    virtual std::size_t lastInstancedPropCount() const { return 0; }
    virtual std::size_t lastSavedDrawCalls() const { return 0; }

    // Per-pass GPU times of the most recently COMPLETED frame (typically 2-3 frames old,
    // because reading GPU timers immediately would stall the CPU). Empty when unsupported.
    virtual std::vector<GpuPassTiming> lastGpuPassTimings() const { return {}; }
    // Increments each time lastGpuPassTimings() holds a newly completed frame.
    virtual std::uint64_t gpuTimingFrameId() const { return 0; }
    // CPU time the renderer spent issuing each pass in the LAST renderWorld() call
    // (state changes, uniform uploads, draw-call submission). Available immediately.
    virtual std::vector<TimingSample> lastCpuPassTimings() const { return {}; }
};

// Creates the default OpenGL 3.3 Core 3D renderer backend.
std::unique_ptr<Renderer> createOpenGLRenderer();

} // namespace sage
