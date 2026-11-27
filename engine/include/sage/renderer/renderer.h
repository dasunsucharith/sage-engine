#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace sage {

class World;

struct ClearColor {
    float r = 0.06f;
    float g = 0.08f;
    float b = 0.12f;
    float a = 1.0f;
};

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

    virtual RendererInfo info() const = 0;
    virtual std::size_t loadedModelCount() const = 0;
    virtual std::size_t grassInstanceCount() const { return 0; }
    virtual std::size_t lastCulledCount() const { return 0; }
};

// Creates the default OpenGL 3.3 Core 3D renderer backend.
std::unique_ptr<Renderer> createOpenGLRenderer();

} // namespace sage
