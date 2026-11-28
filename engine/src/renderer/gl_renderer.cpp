#include "sage/renderer/renderer.h"

#include "renderer/gl_functions.h"
#include "renderer/gl_shaders.h"
#include "renderer/gl_animation.h"
#include "renderer/gl_model.h"
#include "renderer/gl_profiler.h"
#include "sage/world/world.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sage {

namespace {

using namespace render; // Mesh/model, animation and profiling modules (gl_model/gl_animation/gl_profiler)

constexpr int kShadowMapSize = 2048;
constexpr int kShadowMapSizeFar = 1024;
constexpr int kMaxSkinBones = 64;

// Shader sources live in engine/shaders/*.vert|*.frag (loaded at startup and hot-reloaded
// on save; see gl_shaders.cpp). Each program below lists the files it is built from.
struct ProgramSpec {
    GLuint* program;
    const char* label;
    const char* vertFile;
    const char* fragFile;
};

class OpenGLRenderer final : public Renderer {
public:
    ~OpenGLRenderer() override { shutdown(); }

    bool initialize(int viewportWidth, int viewportHeight) override {
        if (!gl::loadFunctions()) return false;

        m_info.backendName = "OpenGL 3.3 Core (v1.6.0: Material-Merged Instancing + Pose Cache + Time-Sliced 3-CSM + Soft-Shore Water + Shadowed Grass + 8x AF + GGX PBR)";
        m_info.gpuVendor = safeGLString(GL_VENDOR);
        m_info.gpuRenderer = safeGLString(GL_RENDERER);
        m_info.apiVersion = safeGLString(GL_VERSION);

        if (!m_shaders.findShaderDirectory(m_assetRoot)) {
            return false;
        }
        if (!buildAllPrograms()) {
            std::cerr << "[SAGE][Shader] Could not build the renderer's shader programs (see errors above).\n";
            shutdown();
            return false;
        }

        gl::genVertexArrays(1, &m_fullscreenVao);

        // Tiered 3-Split Cascaded Shadow Map FBOs (Near 24m @ 2048px, Mid 92m @ 1024px, Far 255m @ 1024px!)
        auto createShadowCascade = [](GLuint& fboOut, GLuint& texOut, int mapSize) {
            gl::genFramebuffers(1, &fboOut);
            glGenTextures(1, &texOut);
            glBindTexture(GL_TEXTURE_2D, texOut);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mapSize, mapSize, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            constexpr float kBorder[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, kBorder);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            gl::bindFramebuffer(GL_FRAMEBUFFER, fboOut);
            gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texOut, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            if (gl::checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                gl::deleteFramebuffers(1, &fboOut);
                glDeleteTextures(1, &texOut);
                fboOut = texOut = 0;
            }
            gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        };
        createShadowCascade(m_shadowFbo, m_shadowTex, kShadowMapSize);
        createShadowCascade(m_shadowFboMid, m_shadowTexMid, kShadowMapSizeFar);
        createShadowCascade(m_shadowFboFar, m_shadowTexFar, kShadowMapSizeFar);

        // Shared dynamic GPU instance buffer for zero-overhead batched static prop rendering (v1.5.0)!
        gl::genBuffers(1, &m_propInstVbo);
        const PropInstanceData defaultInst{};
        gl::bindBuffer(GL_ARRAY_BUFFER, m_propInstVbo);
        gl::bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(PropInstanceData)), &defaultInst, GL_DYNAMIC_DRAW);
        gl::bindBuffer(GL_ARRAY_BUFFER, 0);

        fetchUniformLocations();

        m_boxMesh = buildUnitCubeMesh();
        m_boxMesh.attachInstanceBuffer(m_propInstVbo);
        m_planeMesh = buildUnitPlaneMesh();
        m_waterMesh = buildWaterGridMesh(160);
        m_sphereMesh = buildUnitSphereMesh();
        m_arrowMesh = buildUnitArrowMesh();
        m_ringMesh = buildUnitRingMesh();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        onResize(viewportWidth, viewportHeight);
        m_gpuProfiler.initialize();
        m_initialized = true;
        return true;
    }

    // ------------------------------------------------------------------
    // Shader programs (sources in engine/shaders, hot-reloaded on save)
    // ------------------------------------------------------------------
    std::vector<ProgramSpec> programSpecs() {
        return {
            {&m_shadowProgram, "ShadowPass", "shadow.vert", "shadow.frag"},
            {&m_skyProgram, "SkyPass", "sky.vert", "sky.frag"},
            {&m_litProgram, "Lit3DPass", "lit.vert", "lit.frag"},
            {&m_grassProgram, "InstancedGrassPass", "grass.vert", "grass.frag"},
            {&m_waterProgram, "OceanWaterPass", "water.vert", "water.frag"},
            {&m_bloomExtractProgram, "BloomExtract", "fullscreen.vert", "bloom_extract.frag"},
            {&m_bloomBlurProgram, "BloomBlur", "fullscreen.vert", "bloom_blur.frag"},
            {&m_compositeProgram, "HDRComposite", "fullscreen.vert", "composite.frag"},
        };
    }

    // Builds every program into temporaries and only swaps them in if ALL succeed, so a
    // typo in one shader never leaves the renderer half-updated (or black).
    bool buildAllPrograms() {
        const std::vector<ProgramSpec> specs = programSpecs();
        std::vector<GLuint> built;
        built.reserve(specs.size());
        for (const ProgramSpec& spec : specs) {
            const GLuint program = m_shaders.buildProgram(spec.label, spec.vertFile, spec.fragFile);
            if (!program) {
                for (const GLuint p : built) gl::deleteProgram(p);
                return false;
            }
            built.push_back(program);
        }
        for (std::size_t i = 0; i < specs.size(); ++i) {
            if (*specs[i].program) gl::deleteProgram(*specs[i].program);
            *specs[i].program = built[i];
        }
        return true;
    }

    // Polls shader files (every 0.5 s) and rebuilds all programs when one is saved.
    void checkShaderHotReload() {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_lastShaderPoll < std::chrono::milliseconds(500)) return;
        m_lastShaderPoll = now;
        std::string changedFile;
        if (!m_shaders.pollForChanges(changedFile)) return;
        std::cout << "[SAGE][Shader] Change detected in " << changedFile << ", recompiling shaders...\n";
        const auto t0 = std::chrono::steady_clock::now();
        if (buildAllPrograms()) {
            fetchUniformLocations();
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::cout << "[SAGE][Shader] Shaders reloaded (" << static_cast<int>(ms) << " ms).\n";
        } else {
            std::cerr << "[SAGE][Shader] Fix the error above and save again: still using the previous working shaders.\n";
        }
    }

    // Looks up every uniform the renderer sets. Called after every (re)build, because
    // relinking a program can change its uniform locations.
    void fetchUniformLocations() {
        m_shadowModel = gl::getUniformLocation(m_shadowProgram, "uModel");
        m_shadowLightVP = gl::getUniformLocation(m_shadowProgram, "uLightViewProj");
        m_shadowHasSkin = gl::getUniformLocation(m_shadowProgram, "uHasSkin");
        m_shadowUseInstancing = gl::getUniformLocation(m_shadowProgram, "uUseInstancing");
        m_shadowBoneMatrices = gl::getUniformLocation(m_shadowProgram, "uBoneMatrices");

        m_skyInvViewProj = gl::getUniformLocation(m_skyProgram, "uInvViewProj");
        m_skyCameraPos = gl::getUniformLocation(m_skyProgram, "uCameraPos");
        m_skySunDir = gl::getUniformLocation(m_skyProgram, "uSunDir");
        m_skySunColor = gl::getUniformLocation(m_skyProgram, "uSunColor");
        m_skyZenith = gl::getUniformLocation(m_skyProgram, "uSkyZenith");
        m_skyHorizon = gl::getUniformLocation(m_skyProgram, "uSkyHorizon");
        m_skyGround = gl::getUniformLocation(m_skyProgram, "uGroundAmbient");
        m_skyTime = gl::getUniformLocation(m_skyProgram, "uTime");

        m_uModel = gl::getUniformLocation(m_litProgram, "uModel");
        m_uViewProj = gl::getUniformLocation(m_litProgram, "uViewProj");
        m_uHasSkin = gl::getUniformLocation(m_litProgram, "uHasSkin");
        m_uUseInstancing = gl::getUniformLocation(m_litProgram, "uUseInstancing");
        m_uBoneMatrices = gl::getUniformLocation(m_litProgram, "uBoneMatrices");
        m_uBaseColor = gl::getUniformLocation(m_litProgram, "uBaseColor");
        m_uEmissive = gl::getUniformLocation(m_litProgram, "uEmissive");
        m_uMetallic = gl::getUniformLocation(m_litProgram, "uMetallic");
        m_uRoughness = gl::getUniformLocation(m_litProgram, "uRoughness");
        m_uHasTexture = gl::getUniformLocation(m_litProgram, "uHasTexture");
        m_uAlbedoTex = gl::getUniformLocation(m_litProgram, "uAlbedoTex");
        m_uHasNormalTex = gl::getUniformLocation(m_litProgram, "uHasNormalTex");
        m_uNormalTex = gl::getUniformLocation(m_litProgram, "uNormalTex");
        m_uHasMetRoughTex = gl::getUniformLocation(m_litProgram, "uHasMetRoughTex");
        m_uMetRoughTex = gl::getUniformLocation(m_litProgram, "uMetRoughTex");
        m_uPulse = gl::getUniformLocation(m_litProgram, "uPulse");
        m_uTime = gl::getUniformLocation(m_litProgram, "uTime");
        m_uIsGround = gl::getUniformLocation(m_litProgram, "uIsGround");
        m_uTerrainMode = gl::getUniformLocation(m_litProgram, "uTerrainMode");
        m_uTLayers = gl::getUniformLocation(m_litProgram, "uTLayers");
        m_uTLayerCount = gl::getUniformLocation(m_litProgram, "uTLayerCount");
        m_uTCliffColor = gl::getUniformLocation(m_litProgram, "uTCliffColor");
        m_uTSlopeSnow = gl::getUniformLocation(m_litProgram, "uTSlopeSnow");
        m_uTSnowColor = gl::getUniformLocation(m_litProgram, "uTSnowColor");
        m_uWetness = gl::getUniformLocation(m_litProgram, "uWetness");
        m_uCloudShadowsEnabled = gl::getUniformLocation(m_litProgram, "uCloudShadowsEnabled");
        m_uShadowsEnabled = gl::getUniformLocation(m_litProgram, "uShadowsEnabled");
        m_uLightViewProj = gl::getUniformLocation(m_litProgram, "uLightViewProj");
        m_uLightViewProjMid = gl::getUniformLocation(m_litProgram, "uLightViewProjMid");
        m_uLightViewProjFar = gl::getUniformLocation(m_litProgram, "uLightViewProjFar");
        m_uShadowMap = gl::getUniformLocation(m_litProgram, "uShadowMap");
        m_uShadowMapMid = gl::getUniformLocation(m_litProgram, "uShadowMapMid");
        m_uShadowMapFar = gl::getUniformLocation(m_litProgram, "uShadowMapFar");
        m_uShadowTexelSize = gl::getUniformLocation(m_litProgram, "uShadowTexelSize");
        m_uCameraPos = gl::getUniformLocation(m_litProgram, "uCameraPos");
        m_uSunDir = gl::getUniformLocation(m_litProgram, "uSunDir");
        m_uSunColor = gl::getUniformLocation(m_litProgram, "uSunColor");
        m_uSkyZenith = gl::getUniformLocation(m_litProgram, "uSkyZenith");
        m_uSkyHorizon = gl::getUniformLocation(m_litProgram, "uSkyHorizon");
        m_uGroundAmbient = gl::getUniformLocation(m_litProgram, "uGroundAmbient");
        m_uAmbientIntensity = gl::getUniformLocation(m_litProgram, "uAmbientIntensity");
        m_uFogStart = gl::getUniformLocation(m_litProgram, "uFogStart");
        m_uFogEnd = gl::getUniformLocation(m_litProgram, "uFogEnd");
        m_uPointLightCount = gl::getUniformLocation(m_litProgram, "uPointLightCount");
        m_uLightTileMask = gl::getUniformLocation(m_litProgram, "uLightTileMask");
        m_uLightTilesEnabled = gl::getUniformLocation(m_litProgram, "uLightTilesEnabled");
        m_uLightTileSize = gl::getUniformLocation(m_litProgram, "uLightTileSize");
        m_uLightTilesY = gl::getUniformLocation(m_litProgram, "uLightTilesY");
        m_uLightSlices = gl::getUniformLocation(m_litProgram, "uLightSlices");
        m_uCameraForward = gl::getUniformLocation(m_litProgram, "uCameraForward");
        m_uClusterNear = gl::getUniformLocation(m_litProgram, "uClusterNear");
        m_uClusterLogScale = gl::getUniformLocation(m_litProgram, "uClusterLogScale");
        m_uPointLightPosRadius = gl::getUniformLocation(m_litProgram, "uPointLightPosRadius");
        m_uPointLightColorInt = gl::getUniformLocation(m_litProgram, "uPointLightColorInt");

        m_waterModel = gl::getUniformLocation(m_waterProgram, "uModel");
        m_waterViewProj = gl::getUniformLocation(m_waterProgram, "uViewProj");
        m_waterTime = gl::getUniformLocation(m_waterProgram, "uTime");
        m_waterShallow = gl::getUniformLocation(m_waterProgram, "uShallowColor");
        m_waterCamPos = gl::getUniformLocation(m_waterProgram, "uCameraPos");
        m_waterSunDir = gl::getUniformLocation(m_waterProgram, "uSunDir");
        m_waterSunColor = gl::getUniformLocation(m_waterProgram, "uSunColor");
        m_waterSkyZenith = gl::getUniformLocation(m_waterProgram, "uSkyZenith");
        m_waterSkyHorizon = gl::getUniformLocation(m_waterProgram, "uSkyHorizon");
        m_waterFogStart = gl::getUniformLocation(m_waterProgram, "uFogStart");
        m_waterFogEnd = gl::getUniformLocation(m_waterProgram, "uFogEnd");
        m_waterPointLightCount = gl::getUniformLocation(m_waterProgram, "uWaterPointLightCount");
        m_waterPointLightPosRadius = gl::getUniformLocation(m_waterProgram, "uWaterPointLightPosRadius");
        m_waterPointLightColorInt = gl::getUniformLocation(m_waterProgram, "uWaterPointLightColorInt");
        m_waterSceneDepthTex = gl::getUniformLocation(m_waterProgram, "uWaterSceneDepthTex");
        m_waterNearPlane = gl::getUniformLocation(m_waterProgram, "uWaterNearPlane");
        m_waterFarPlane = gl::getUniformLocation(m_waterProgram, "uWaterFarPlane");

        m_bloomExtractTex = gl::getUniformLocation(m_bloomExtractProgram, "uHdrTex");
        m_bloomBlurImage = gl::getUniformLocation(m_bloomBlurProgram, "uImage");
        m_bloomBlurDir = gl::getUniformLocation(m_bloomBlurProgram, "uDirection");
        m_compHdrTex = gl::getUniformLocation(m_compositeProgram, "uHdrTex");
        m_compBloomTex = gl::getUniformLocation(m_compositeProgram, "uBloomTex");
        m_compDepthTex = gl::getUniformLocation(m_compositeProgram, "uDepthTex");
        m_compBloomEnabled = gl::getUniformLocation(m_compositeProgram, "uBloomEnabled");
        m_compBloomIntensity = gl::getUniformLocation(m_compositeProgram, "uBloomIntensity");
        m_compSsaoEnabled = gl::getUniformLocation(m_compositeProgram, "uSsaoEnabled");
        m_compFxaaEnabled = gl::getUniformLocation(m_compositeProgram, "uFxaaEnabled");
        m_compGodRaysEnabled = gl::getUniformLocation(m_compositeProgram, "uGodRaysEnabled");
        m_compGodRayIntensity = gl::getUniformLocation(m_compositeProgram, "uGodRayIntensity");
        m_compSunScreenPos = gl::getUniformLocation(m_compositeProgram, "uSunScreenPos");
        m_compSunColor = gl::getUniformLocation(m_compositeProgram, "uSunColorComp");
        m_compSsrEnabled = gl::getUniformLocation(m_compositeProgram, "uSsrEnabled");
        m_compSsrIntensity = gl::getUniformLocation(m_compositeProgram, "uSsrIntensity");
        m_compViewProj = gl::getUniformLocation(m_compositeProgram, "uViewProjComp");
        m_compInvViewProj = gl::getUniformLocation(m_compositeProgram, "uInvViewProjComp");
        m_compCameraPos = gl::getUniformLocation(m_compositeProgram, "uCameraPosComp");
        m_compExposure = gl::getUniformLocation(m_compositeProgram, "uExposure");

        m_grassViewProj = gl::getUniformLocation(m_grassProgram, "uViewProj");
        m_grassCamPos = gl::getUniformLocation(m_grassProgram, "uCameraPos");
        m_grassInteractPos = gl::getUniformLocation(m_grassProgram, "uInteractPos");
        m_grassTime = gl::getUniformLocation(m_grassProgram, "uTime");
        m_grassSunDir = gl::getUniformLocation(m_grassProgram, "uSunDir");
        m_grassSunColor = gl::getUniformLocation(m_grassProgram, "uSunColor");
        m_grassSkyZenith = gl::getUniformLocation(m_grassProgram, "uSkyZenith");
        m_grassSkyHorizon = gl::getUniformLocation(m_grassProgram, "uSkyHorizon");
        m_grassGroundAmbient = gl::getUniformLocation(m_grassProgram, "uGroundAmbient");
        m_grassAmbientInt = gl::getUniformLocation(m_grassProgram, "uAmbientIntensity");
        m_grassFogStart = gl::getUniformLocation(m_grassProgram, "uFogStart");
        m_grassFogEnd = gl::getUniformLocation(m_grassProgram, "uFogEnd");
        m_grassCloudShadows = gl::getUniformLocation(m_grassProgram, "uCloudShadowsEnabled");
        m_grassShadowsEnabled = gl::getUniformLocation(m_grassProgram, "uShadowsEnabled");
        m_grassLightVP = gl::getUniformLocation(m_grassProgram, "uLightViewProj");
        m_grassLightVPMid = gl::getUniformLocation(m_grassProgram, "uLightViewProjMid");
        m_grassShadowMap = gl::getUniformLocation(m_grassProgram, "uShadowMap");
        m_grassShadowMapMid = gl::getUniformLocation(m_grassProgram, "uShadowMapMid");
        m_compTexelSize = gl::getUniformLocation(m_compositeProgram, "uTexelSize");
        m_compNearPlane = gl::getUniformLocation(m_compositeProgram, "uNearPlane");
        m_compFarPlane = gl::getUniformLocation(m_compositeProgram, "uFarPlane");

    }

    void destroyPostBuffers() {
        if (m_waterDepthCopyTex) { glDeleteTextures(1, &m_waterDepthCopyTex); m_waterDepthCopyTex = 0; }
        if (m_hdrDepthTex) { glDeleteTextures(1, &m_hdrDepthTex); m_hdrDepthTex = 0; }
        if (m_hdrColorTex) { glDeleteTextures(1, &m_hdrColorTex); m_hdrColorTex = 0; }
        if (m_hdrFbo) { gl::deleteFramebuffers(1, &m_hdrFbo); m_hdrFbo = 0; }
        for (int i = 0; i < 2; ++i) {
            if (m_bloomTex[i]) { glDeleteTextures(1, &m_bloomTex[i]); m_bloomTex[i] = 0; }
            if (m_bloomFbo[i]) { gl::deleteFramebuffers(1, &m_bloomFbo[i]); m_bloomFbo[i] = 0; }
        }
    }

    void recreatePostBuffers(int w, int h) {
        destroyPostBuffers();

        // Main HDR RGBA16F framebuffer + sampleable Depth24 texture (for real-time Screen-Space AO & Depth Water!)
        gl::genFramebuffers(1, &m_hdrFbo);
        gl::bindFramebuffer(GL_FRAMEBUFFER, m_hdrFbo);

        glGenTextures(1, &m_hdrColorTex);
        glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrColorTex, 0);

        glGenTextures(1, &m_hdrDepthTex);
        glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_hdrDepthTex, 0);

        // GPU copy of opaque scene depth for soft shoreline water fade & foam without FBO feedback loop!
        glGenTextures(1, &m_waterDepthCopyTex);
        glBindTexture(GL_TEXTURE_2D, m_waterDepthCopyTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const bool hdrOk = (gl::checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        // Half-resolution ping-pong Bloom buffers
        m_bloomWidth = std::max(w / 2, 1);
        m_bloomHeight = std::max(h / 2, 1);
        bool bloomOk = hdrOk;
        for (int i = 0; i < 2 && bloomOk; ++i) {
            gl::genFramebuffers(1, &m_bloomFbo[i]);
            gl::bindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[i]);

            glGenTextures(1, &m_bloomTex[i]);
            glBindTexture(GL_TEXTURE_2D, m_bloomTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_bloomWidth, m_bloomHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTex[i], 0);
            if (gl::checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                bloomOk = false;
            }
        }

        gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (!bloomOk) {
            destroyPostBuffers();
        }
    }

    void shutdown() override {
        m_gpuProfiler.shutdown();
        if (m_lightTileTex) { glDeleteTextures(1, &m_lightTileTex); m_lightTileTex = 0; }
        for (auto& [path, model] : m_modelCache) model.destroy();
        m_modelCache.clear();
        m_failedModels.clear();

        m_boxMesh.destroy();
        m_planeMesh.destroy();
        m_waterMesh.destroy();
        m_sphereMesh.destroy();
        m_arrowMesh.destroy();
        m_ringMesh.destroy();
        m_terrainMesh.destroy();
        m_terrainMeshBuilt = false;
        destroyGrassBuffers();

        destroyPostBuffers();
        if (m_propInstVbo) { gl::deleteBuffers(1, &m_propInstVbo); m_propInstVbo = 0; }
        if (m_shadowTexFar) { glDeleteTextures(1, &m_shadowTexFar); m_shadowTexFar = 0; }
        if (m_shadowFboFar) { gl::deleteFramebuffers(1, &m_shadowFboFar); m_shadowFboFar = 0; }
        if (m_shadowTexMid) { glDeleteTextures(1, &m_shadowTexMid); m_shadowTexMid = 0; }
        if (m_shadowFboMid) { gl::deleteFramebuffers(1, &m_shadowFboMid); m_shadowFboMid = 0; }
        if (m_shadowTex) { glDeleteTextures(1, &m_shadowTex); m_shadowTex = 0; }
        if (m_shadowFbo) { gl::deleteFramebuffers(1, &m_shadowFbo); m_shadowFbo = 0; }
        if (m_fullscreenVao) { gl::deleteVertexArrays(1, &m_fullscreenVao); m_fullscreenVao = 0; }

        auto delProg = [](GLuint& p) { if (p) { gl::deleteProgram(p); p = 0; } };
        delProg(m_shadowProgram);
        delProg(m_skyProgram);
        delProg(m_litProgram);
        delProg(m_grassProgram);
        delProg(m_waterProgram);
        delProg(m_bloomExtractProgram);
        delProg(m_bloomBlurProgram);
        delProg(m_compositeProgram);
        m_pipelineWarmed = false;
        m_initialized = false;
    }

    void setAssetRoot(const std::filesystem::path& assetRoot) override { m_assetRoot = assetRoot; }

    void preloadModels(const World& world) override {
        if (!m_initialized) return;
        std::size_t preloaded = 0;
        for (const Entity& e : world.entities()) {
            if (e.shape == EntityShape::Model && !e.modelPath.empty()) {
                const bool alreadyCached = m_modelCache.contains(e.modelPath);
                if (getOrLoadModel(e.modelPath) && !alreadyCached) {
                    ++preloaded;
                }
            }
        }
        if (world.terrain().enabled) {
            ensureTerrainMesh(world);
        }
        if (world.sky().grassEnabled) {
            ensureGrassField(world);
        }

        // 1x1 GPU Shader & FBO Pipeline Warmup (eliminates NVIDIA Frame-1 JIT shader compile stall!)
        if (!m_pipelineWarmed && m_hdrFbo != 0) {
            const glm::mat4 idMat(1.0f);
            glViewport(0, 0, 1, 1);
            if (m_shadowFbo != 0) {
                gl::bindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
                gl::useProgram(m_shadowProgram);
                gl::uniformMatrix4fv(m_shadowLightVP, 1, GL_FALSE, glm::value_ptr(idMat));
                gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(idMat));
                gl::uniform1i(m_shadowHasSkin, 0);
                gl::uniform1i(m_shadowUseInstancing, 0);
                gl::bindVertexArray(m_boxMesh.vao);
                glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
                gl::uniform1i(m_shadowUseInstancing, 1);
                gl::drawElementsInstanced(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr, 1);
                gl::uniform1i(m_shadowUseInstancing, 0);
            }
            gl::bindFramebuffer(GL_FRAMEBUFFER, m_hdrFbo);
            gl::useProgram(m_skyProgram);
            gl::bindVertexArray(m_fullscreenVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            gl::useProgram(m_litProgram);
            gl::uniformMatrix4fv(m_uViewProj, 1, GL_FALSE, glm::value_ptr(idMat));
            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(idMat));
            gl::uniform1i(m_uHasSkin, 0);
            gl::uniform1i(m_uUseInstancing, 0);
            gl::uniform1i(m_uShadowsEnabled, 0);
            gl::bindVertexArray(m_boxMesh.vao);
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
            gl::uniform1i(m_uUseInstancing, 1);
            gl::drawElementsInstanced(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr, 1);
            gl::uniform1i(m_uUseInstancing, 0);

            if (m_grassVao != 0) {
                gl::useProgram(m_grassProgram);
                gl::uniformMatrix4fv(m_grassViewProj, 1, GL_FALSE, glm::value_ptr(idMat));
                gl::uniform1i(m_grassShadowsEnabled, 0);
                gl::bindVertexArray(m_grassVao);
                gl::drawArraysInstanced(GL_TRIANGLES, 0, 3, 1);
            }
            gl::useProgram(m_waterProgram);
            gl::uniformMatrix4fv(m_waterViewProj, 1, GL_FALSE, glm::value_ptr(idMat));
            gl::uniformMatrix4fv(m_waterModel, 1, GL_FALSE, glm::value_ptr(idMat));
            gl::uniform1i(m_waterSceneDepthTex, 7);
            gl::uniform1f(m_waterNearPlane, 0.1f);
            gl::uniform1f(m_waterFarPlane, 1000.0f);
            gl::bindVertexArray(m_waterMesh.vao);
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

            if (m_bloomFbo[0] != 0) {
                gl::bindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
                gl::bindVertexArray(m_fullscreenVao);
                gl::useProgram(m_bloomExtractProgram);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                gl::useProgram(m_bloomBlurProgram);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
            gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
            gl::bindVertexArray(m_fullscreenVao);
            gl::useProgram(m_compositeProgram);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            gl::bindVertexArray(0);
            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
            glFinish();
            m_pipelineWarmed = true;
        }

        if (preloaded > 0) {
            std::cout << "[SAGE][Preload] Pre-warmed " << preloaded
                      << " .glb model(s) + 3D Terrain + " << m_grassCount
                      << " grass clumps + GPU shaders before Frame 1!\n";
            std::cout << "[SAGE][Shadows] Merged depth-only shadow meshes use "
                      << (m_shadowMergedBytes / 1024) << " KB of extra GPU memory\n";
        }
    }

    void onResize(int width, int height) override {
        m_viewportWidth = width > 0 ? width : 1;
        m_viewportHeight = height > 0 ? height : 1;
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        if (m_initialized || m_hdrProgramReady()) {
            recreatePostBuffers(m_viewportWidth, m_viewportHeight);
        }
    }

    void beginFrame(const ClearColor& clearColor) override {
        checkShaderHotReload();
        m_clearColor = clearColor;
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void renderWorld(const World& world, float timeSeconds) override {
        if (!m_initialized) return;
        GpuProfilerFrameScope gpuFrame(m_gpuProfiler);
        m_gpuProfiler.begin("setup");
        m_frameWorld = &world; // Component lookups (look-at, ragdoll) while posing models

        const CameraState& cam = world.camera();
        const SkySettings& sky = world.sky();
        const float aspect = static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight);

        const glm::vec3 camPos(cam.position.x, cam.position.y, cam.position.z);
        glm::vec3 camTarget(cam.target.x, cam.target.y, cam.target.z);
        if (glm::length(camTarget - camPos) < 0.001f) {
            camTarget = camPos + glm::vec3(0.0f, 0.0f, 1.0f);
        }

        const glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 proj = glm::perspective(
            glm::radians(std::clamp(cam.fovDegrees, 20.0f, 120.0f)),
            aspect, cam.nearPlane, cam.farPlane);
        const glm::mat4 viewProj = proj * view;
        const glm::mat4 invViewProj = glm::inverse(viewProj);

        if (world.terrain().enabled) {
            ensureTerrainMesh(world);
        }
        if (sky.grassEnabled) {
            ensureGrassField(world);
        }

        // Extract 6 normalized camera frustum planes (Gribb-Hartmann) for fast AAA off-screen culling!
        glm::vec4 frustumPlanes[6];
        {
            const glm::vec4 r0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
            const glm::vec4 r1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
            const glm::vec4 r2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
            const glm::vec4 r3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);
            frustumPlanes[0] = r3 + r0; // Left
            frustumPlanes[1] = r3 - r0; // Right
            frustumPlanes[2] = r3 + r1; // Bottom
            frustumPlanes[3] = r3 - r1; // Top
            frustumPlanes[4] = r3 + r2; // Near
            frustumPlanes[5] = r3 - r2; // Far
            for (int i = 0; i < 6; ++i) {
                const float len = glm::length(glm::vec3(frustumPlanes[i]));
                if (len > 1e-5f) frustumPlanes[i] /= len;
            }
        }
        auto isEntityInFrustum = [&](const Entity& e) -> bool {
            // Never cull huge ground/water planes
            if (e.shape == EntityShape::Plane || e.shape == EntityShape::Water) return true;
            const float maxScale = std::max({std::abs(e.scale.x), std::abs(e.scale.y), std::abs(e.scale.z), 0.1f});
            glm::vec3 center(e.position.x, e.position.y, e.position.z);
            float boundRadius = maxScale * 1.4f + 1.5f;
            if (e.shape == EntityShape::Model) {
                if (const auto it = m_modelCache.find(e.modelPath); it != m_modelCache.end()) {
                    const GpuModel& gm = it->second;
                    center += glm::vec3(gm.boundCenter.x * e.scale.x, gm.boundCenter.y * e.scale.y, gm.boundCenter.z * e.scale.z);
                    boundRadius = gm.boundRadius * maxScale + 1.5f;
                } else {
                    boundRadius = maxScale * 20.0f + 8.0f;
                }
            }
            for (int i = 0; i < 6; ++i) {
                if (glm::dot(glm::vec3(frustumPlanes[i]), center) + frustumPlanes[i].w < -boundRadius) {
                    return false;
                }
            }
            return true;
        };

        m_lastDrawCalls = 0;
        m_lastInstancedBatches = 0;
        m_lastInstancedProps = 0;
        m_lastSavedDrawCalls = 0;
        ++m_frameCounter;
        if (m_framePoseCache.size() > 256) {
            m_framePoseCache.clear();
        }

        // --- Pass 0: Texel-Snapped Time-Sliced 3-Split 2048x2048 Cascaded Shadow Maps (24m / 92m / 255m!) ---
        const bool useShadows = sky.shadowsEnabled && (m_shadowFbo != 0) && (m_shadowFboMid != 0) && (m_shadowFboFar != 0);
        glm::mat4 lightViewProj(1.0f);
        glm::mat4 lightViewProjMid(1.0f);
        glm::mat4 lightViewProjFar(1.0f);
        if (useShadows) {
            const glm::vec3 sunDir = glm::normalize(glm::vec3(sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z));
            const glm::vec3 focusPoint(camTarget.x, camTarget.y, camTarget.z);
            const glm::vec3 upAxis = std::abs(sunDir.y) > 0.98f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

            auto renderShadowCascade = [&](GLuint fbo, int mapSize, int cascadeIdx, float radius, float backDist, float farDist) -> glm::mat4 {
                glm::mat4 lightView = glm::lookAt(focusPoint + sunDir * backDist, focusPoint, upAxis);
                const float texelWorld = (radius * 2.0f) / static_cast<float>(mapSize);
                const glm::vec4 originLS = lightView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                lightView[3][0] += std::floor(originLS.x / texelWorld) * texelWorld - originLS.x;
                lightView[3][1] += std::floor(originLS.y / texelWorld) * texelWorld - originLS.y;

                constexpr float kShadowNear = 5.0f;
                const glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, kShadowNear, farDist);
                const glm::mat4 lvp = lightProj * lightView;

                gl::bindFramebuffer(GL_FRAMEBUFFER, fbo);
                glViewport(0, 0, mapSize, mapSize);
                glClear(GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(2.0f, 4.0f);

                gl::useProgram(m_shadowProgram);
                gl::uniformMatrix4fv(m_shadowLightVP, 1, GL_FALSE, glm::value_ptr(lvp));
                gl::uniform1i(m_shadowHasSkin, 0);
                gl::uniform1i(m_shadowUseInstancing, 0);

                // 3D Heightmap Terrain casts mountain & hill shadows across valleys!
                if (world.terrain().enabled && m_terrainMesh.indexCount > 0) {
                    const glm::mat4 idMat(1.0f);
                    gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(idMat));
                    gl::bindVertexArray(m_terrainMesh.vao);
                    glDrawElements(GL_TRIANGLES, m_terrainMesh.indexCount, GL_UNSIGNED_INT, nullptr);
                    ++m_lastDrawCalls;
                }

                m_instBatchesScratch.clear();
                const float minWorldRadius = (cascadeIdx == 2) ? 3.2f : ((cascadeIdx == 1) ? 1.1f : 0.0f);
                for (const Entity& e : world.entities()) {
                    if (!e.visible || e.shape == EntityShape::Plane || e.shape == EntityShape::Water || e.shape == EntityShape::Ring) {
                        continue;
                    }
                    const float dx = e.position.x - focusPoint.x;
                    const float dz = e.position.z - focusPoint.z;
                    const float distSq = dx * dx + dz * dz;
                    // Cull against this cascade's actual light-space box (an orthographic box
                    // aligned with the sun), using a rotation-safe bounding sphere. The old XZ
                    // circle around the focus point missed casters lying along the sun direction,
                    // so their shadows vanished from the mid/far cascades.
                    {
                        const float maxS = std::max({std::abs(e.scale.x), std::abs(e.scale.y), std::abs(e.scale.z), 0.1f});
                        float casterRadius = maxS * 0.9f + 0.5f;
                        if (e.shape == EntityShape::Model) {
                            if (const auto it = m_modelCache.find(e.modelPath); it != m_modelCache.end()) {
                                casterRadius = (glm::length(it->second.boundCenter) + it->second.boundRadius) * maxS;
                            }
                        }
                        const glm::vec4 ls = lightView * glm::vec4(e.position.x, e.position.y, e.position.z, 1.0f);
                        const bool inBox = std::abs(ls.x) <= radius + casterRadius &&
                                           std::abs(ls.y) <= radius + casterRadius &&
                                           -ls.z >= kShadowNear - casterRadius &&
                                           -ls.z <= farDist + casterRadius;
                        if (!inBox) {
                            continue;
                        }
                    }
                    const float maxScale = std::max({std::abs(e.scale.x), std::abs(e.scale.y), std::abs(e.scale.z), 0.1f});
                    if (e.shape == EntityShape::Model) {
                        const GpuModel* gpuModel = getOrLoadModel(e.modelPath);
                        if (!gpuModel) continue;
                        const float worldRadius = gpuModel->boundRadius * maxScale;
                        if (worldRadius < minWorldRadius) continue;
                        // Skip distant skinned rigs & tiny animated birds in Mid/Far cascades for massive GPU savings!
                        if (gpuModel->hasSkins && (cascadeIdx == 2 || (cascadeIdx == 1 && distSq > 48.0f * 48.0f))) {
                            continue;
                        }
                        const glm::mat4 entityMat = buildEntityMatrix(e);
                        if (gpuModel->hasAnimations || gpuModel->hasSkins) {
                            const auto& nodeWorld = getOrEvaluatePose(*gpuModel, e);
                            int uploadedSkin = -1;
                            for (std::size_t ni = 0; ni < gpuModel->nodes.size(); ++ni) {
                                const ModelNode& node = gpuModel->nodes[ni];
                                if (node.subMeshIndices.empty()) continue;
                                if (node.skinIdx >= 0 && static_cast<std::size_t>(node.skinIdx) < gpuModel->skins.size()) {
                                    if (uploadedSkin != node.skinIdx) {
                                        uploadSkinPalette(gpuModel->skins[static_cast<std::size_t>(node.skinIdx)], nodeWorld, m_shadowBoneMatrices);
                                        uploadedSkin = node.skinIdx;
                                    }
                                    gl::uniform1i(m_shadowHasSkin, 1);
                                    gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                                } else {
                                    gl::uniform1i(m_shadowHasSkin, 0);
                                    const glm::mat4 fullMat = entityMat * nodeWorld[ni];
                                    gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(fullMat));
                                }
                                if (ni < gpuModel->shadowNodeMeshes.size() && gpuModel->shadowNodeMeshes[ni].indexCount > 0) {
                                    // All of this node's sub-meshes merged into one depth-only draw.
                                    const GpuSubMesh& merged = gpuModel->shadowNodeMeshes[ni];
                                    gl::bindVertexArray(merged.vao);
                                    glDrawElements(GL_TRIANGLES, merged.indexCount, GL_UNSIGNED_INT, nullptr);
                                    ++m_lastDrawCalls;
                                } else {
                                    for (const std::size_t smIdx : node.subMeshIndices) {
                                        gl::bindVertexArray(gpuModel->subMeshes[smIdx].vao);
                                        glDrawElements(GL_TRIANGLES, gpuModel->subMeshes[smIdx].indexCount, GL_UNSIGNED_INT, nullptr);
                                        ++m_lastDrawCalls;
                                    }
                                }
                            }
                            gl::uniform1i(m_shadowHasSkin, 0);
                        } else if (sky.instancedPropsEnabled && m_propInstVbo != 0) {
                            m_instBatchesScratch[gpuModel].push_back({entityMat, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f)});
                        } else {
                            gl::uniform1i(m_shadowHasSkin, 0);
                            gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                            forEachShadowMesh(*gpuModel, [&](const GpuSubMesh& sm) {
                                gl::bindVertexArray(sm.vao);
                                glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr);
                                ++m_lastDrawCalls;
                            });
                        }
                    } else {
                        if (maxScale < minWorldRadius) continue;
                        const glm::mat4 entityMat = buildEntityMatrix(e);
                        gl::uniform1i(m_shadowHasSkin, 0);
                        const GpuSubMesh* prim = getPrimitiveMesh(e.shape);
                        gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                        gl::bindVertexArray(prim->vao);
                        glDrawElements(GL_TRIANGLES, prim->indexCount, GL_UNSIGNED_INT, nullptr);
                        ++m_lastDrawCalls;
                    }
                }

                // Flush GPU-Instanced Static Prop Batches into Shadow Cascade!
                if (!m_instBatchesScratch.empty()) {
                    gl::uniform1i(m_shadowHasSkin, 0);
                    for (const auto& [gpuModel, instances] : m_instBatchesScratch) {
                        const std::size_t count = instances.size();
                        if (count >= 2) {
                            gl::bindBuffer(GL_ARRAY_BUFFER, m_propInstVbo);
                            gl::bufferData(
                                GL_ARRAY_BUFFER,
                                static_cast<GLsizeiptr>(count * sizeof(PropInstanceData)),
                                instances.data(),
                                GL_DYNAMIC_DRAW);
                            gl::uniform1i(m_shadowUseInstancing, 1);
                            forEachShadowMesh(*gpuModel, [&](const GpuSubMesh& sm) {
                                gl::bindVertexArray(sm.vao);
                                gl::drawElementsInstanced(
                                    GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr,
                                    static_cast<GLsizei>(count));
                                ++m_lastDrawCalls;
                                m_lastSavedDrawCalls += (count - 1);
                            });
                        } else {
                            gl::uniform1i(m_shadowUseInstancing, 0);
                            gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(instances[0].model));
                            forEachShadowMesh(*gpuModel, [&](const GpuSubMesh& sm) {
                                gl::bindVertexArray(sm.vao);
                                glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr);
                                ++m_lastDrawCalls;
                            });
                        }
                    }
                    gl::uniform1i(m_shadowUseInstancing, 0);
                }

                glDisable(GL_POLYGON_OFFSET_FILL);
                return lvp;
            };

            m_gpuProfiler.begin("shadowNear");
            lightViewProj = renderShadowCascade(m_shadowFbo, kShadowMapSize, 0, 24.0f, 80.0f, 190.0f);
            m_gpuProfiler.begin("shadowMid");
            lightViewProjMid = renderShadowCascade(m_shadowFboMid, kShadowMapSizeFar, 1, 92.0f, 145.0f, 340.0f);
            m_gpuProfiler.begin("shadowFar");
            const bool needFarUpdate =
                !m_farShadowValid ||
                ((m_frameCounter & 1u) == 0u) ||
                glm::dot(sunDir, m_lastFarSunDir) < 0.99995f ||
                glm::length(focusPoint - m_lastFarFocus) > 6.0f;
            if (needFarUpdate) {
                m_cachedFarLvp = renderShadowCascade(m_shadowFboFar, kShadowMapSizeFar, 2, 255.0f, 260.0f, 620.0f);
                m_lastFarSunDir = sunDir;
                m_lastFarFocus = focusPoint;
                m_farShadowValid = true;
            }
            lightViewProjFar = m_cachedFarLvp;
            gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        }

        // Bind HDR target if available (falls back cleanly to default framebuffer)
        const bool useHdr = (m_hdrFbo != 0);
        if (useHdr) {
            gl::bindFramebuffer(GL_FRAMEBUFFER, m_hdrFbo);
            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
            glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        m_gpuProfiler.begin("sky");
        // --- Pass 1: HDR Sky Dome + Drifting Procedural Clouds ---
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        gl::useProgram(m_skyProgram);
        gl::uniformMatrix4fv(m_skyInvViewProj, 1, GL_FALSE, glm::value_ptr(invViewProj));
        gl::uniform3f(m_skyCameraPos, camPos.x, camPos.y, camPos.z);
        gl::uniform3f(m_skySunDir, sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z);
        gl::uniform3f(m_skySunColor, sky.sunColor.r, sky.sunColor.g, sky.sunColor.b);
        gl::uniform3f(m_skyZenith, sky.skyZenithColor.r, sky.skyZenithColor.g, sky.skyZenithColor.b);
        gl::uniform3f(m_skyHorizon, sky.skyHorizonColor.r, sky.skyHorizonColor.g, sky.skyHorizonColor.b);
        gl::uniform3f(m_skyGround, sky.groundAmbientColor.r, sky.groundAmbientColor.g, sky.groundAmbientColor.b);
        gl::uniform1f(m_skyTime, timeSeconds);
        gl::bindVertexArray(m_fullscreenVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        gl::bindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);

        m_gpuProfiler.begin("opaque");
        // --- Pass 2: Opaque 3D Lit Geometry & Animated/Skinned/Instanced glTF Models ---
        gl::useProgram(m_litProgram);
        gl::uniform1i(m_uHasSkin, 0);
        gl::uniform1i(m_uUseInstancing, 0);
        gl::uniformMatrix4fv(m_uViewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
        gl::uniform3f(m_uCameraPos, camPos.x, camPos.y, camPos.z);
        gl::uniform3f(m_uSunDir, sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z);
        gl::uniform3f(m_uSunColor, sky.sunColor.r, sky.sunColor.g, sky.sunColor.b);
        gl::uniform3f(m_uSkyZenith, sky.skyZenithColor.r, sky.skyZenithColor.g, sky.skyZenithColor.b);
        gl::uniform3f(m_uSkyHorizon, sky.skyHorizonColor.r, sky.skyHorizonColor.g, sky.skyHorizonColor.b);
        gl::uniform3f(m_uGroundAmbient, sky.groundAmbientColor.r, sky.groundAmbientColor.g, sky.groundAmbientColor.b);
        gl::uniform1f(m_uAmbientIntensity, sky.ambientIntensity);
        gl::uniform1f(m_uFogStart, sky.fogStart);
        gl::uniform1f(m_uFogEnd, sky.fogEnd);
        gl::uniform1f(m_uTime, timeSeconds);
        gl::uniform1i(m_uTerrainMode, world.terrain().mode);
        if (world.terrain().mode == TerrainSettings::kModeDataDriven) {
            uploadTerrainMaterial(world.terrain().spec);
        }
        gl::uniform1f(m_uWetness, std::clamp(sky.wetness, 0.0f, 1.0f));
        gl::uniform1i(m_uCloudShadowsEnabled, sky.cloudShadowsEnabled ? 1 : 0);
        gl::uniform1i(m_uAlbedoTex, 0);
        gl::uniform1i(m_uNormalTex, 4);
        gl::uniform1i(m_uMetRoughTex, 5);
        gl::uniform1i(m_uShadowsEnabled, useShadows ? 1 : 0);
        gl::uniformMatrix4fv(m_uLightViewProj, 1, GL_FALSE, glm::value_ptr(lightViewProj));
        gl::uniformMatrix4fv(m_uLightViewProjMid, 1, GL_FALSE, glm::value_ptr(lightViewProjMid));
        gl::uniformMatrix4fv(m_uLightViewProjFar, 1, GL_FALSE, glm::value_ptr(lightViewProjFar));
        gl::uniform1i(m_uShadowMap, 1);
        gl::uniform1i(m_uShadowMapMid, 3);
        gl::uniform1i(m_uShadowMapFar, 6);
        gl::uniform1f(m_uShadowTexelSize, 1.0f / static_cast<float>(kShadowMapSize));
        if (useShadows) {
            gl::activeTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_shadowTex);
            gl::activeTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, m_shadowTexMid);
            gl::activeTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, m_shadowTexFar);
        }

        // Select the 16 NEAREST active dynamic point lights to camera target (scales to massive open worlds!)
        constexpr int kMaxPointLights = 16;
        glm::vec4 lightPosRad[kMaxPointLights]{};
        glm::vec4 lightColInt[kMaxPointLights]{};
        const auto& allLights = world.pointLights();
        std::vector<const PointLight*> sortedLights;
        sortedLights.reserve(allLights.size());
        for (const PointLight& pl : allLights) {
            if (pl.intensity > 0.001f) {
                sortedLights.push_back(&pl);
            }
        }
        if (sortedLights.size() > kMaxPointLights) {
            std::partial_sort(
                sortedLights.begin(),
                sortedLights.begin() + kMaxPointLights,
                sortedLights.end(),
                [&](const PointLight* a, const PointLight* b) {
                    const float da = (a->position.x - camTarget.x) * (a->position.x - camTarget.x)
                                   + (a->position.z - camTarget.z) * (a->position.z - camTarget.z);
                    const float db = (b->position.x - camTarget.x) * (b->position.x - camTarget.x)
                                   + (b->position.z - camTarget.z) * (b->position.z - camTarget.z);
                    return da < db;
                });
        }
        const int lightCount = static_cast<int>(std::min<std::size_t>(sortedLights.size(), kMaxPointLights));
        for (int i = 0; i < lightCount; ++i) {
            const PointLight& pl = *sortedLights[static_cast<std::size_t>(i)];
            lightPosRad[i] = glm::vec4(pl.position.x, pl.position.y, pl.position.z, pl.radius);
            lightColInt[i] = glm::vec4(pl.color.r, pl.color.g, pl.color.b, pl.intensity);
        }
        gl::uniform1i(m_uPointLightCount, lightCount);
        if (lightCount > 0) {
            gl::uniform4fv(m_uPointLightPosRadius, lightCount, &lightPosRad[0].x);
            gl::uniform4fv(m_uPointLightColorInt, lightCount, &lightColInt[0].x);
        }
        const bool useLightTiles = m_lightTileCulling && lightCount > 0;
        if (useLightTiles) {
            const float clusterNear = std::max(cam.nearPlane, 0.05f);
            const float clusterFar = std::max(cam.farPlane, clusterNear * 2.0f);
            buildLightTiles(viewProj, view, clusterNear, clusterFar, lightPosRad, lightCount);
            gl::activeTexture(GL_TEXTURE0 + kLightTileTextureUnit);
            glBindTexture(GL_TEXTURE_2D, m_lightTileTex);
            gl::activeTexture(GL_TEXTURE0);
            const glm::vec3 camForward = glm::normalize(camTarget - camPos);
            gl::uniform1i(m_uLightTileMask, kLightTileTextureUnit);
            gl::uniform1i(m_uLightTileSize, kLightTileSize);
            gl::uniform1i(m_uLightTilesY, m_lightTilesY);
            gl::uniform1i(m_uLightSlices, kLightSlices);
            gl::uniform3f(m_uCameraForward, camForward.x, camForward.y, camForward.z);
            gl::uniform1f(m_uClusterNear, clusterNear);
            gl::uniform1f(m_uClusterLogScale, static_cast<float>(kLightSlices) / std::log(clusterFar / clusterNear));
        }
        gl::uniform1i(m_uLightTilesEnabled, useLightTiles ? 1 : 0);

        m_waterEntitiesScratch.clear();

        m_gpuProfiler.begin("opaqueEntities");
        m_lastCulledCount = 0;
        m_instBatchesScratch.clear();

        // Draw order for early-Z: solid entities FRONT-TO-BACK so near objects hide far ones
        // before the far ones are shaded; then ground planes & water in their ORIGINAL order
        // (layered roads stay exactly as authored); terrain comes last of all (see below).
        m_opaqueOrderScratch.clear();
        m_opaqueGroundScratch.clear();
        for (const Entity& e : world.entities()) {
            if (e.shape == EntityShape::Plane || e.shape == EntityShape::Water) {
                m_opaqueGroundScratch.push_back(&e);
            } else {
                const float dx = e.position.x - camPos.x;
                const float dy = e.position.y - camPos.y;
                const float dz = e.position.z - camPos.z;
                m_opaqueOrderScratch.push_back({dx * dx + dy * dy + dz * dz, &e});
            }
        }
        std::stable_sort(m_opaqueOrderScratch.begin(), m_opaqueOrderScratch.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        m_opaqueDrawList.clear();
        for (const auto& item : m_opaqueOrderScratch) m_opaqueDrawList.push_back(item.second);
        m_opaqueDrawList.insert(m_opaqueDrawList.end(), m_opaqueGroundScratch.begin(), m_opaqueGroundScratch.end());

        for (const Entity* entityPtr : m_opaqueDrawList) {
            const Entity& e = *entityPtr;
            if (!e.visible) continue;
            if (e.shape == EntityShape::Water) {
                m_waterEntitiesScratch.push_back(&e);
                continue;
            }
            if (!isEntityInFrustum(e)) {
                ++m_lastCulledCount;
                continue;
            }

            const glm::mat4 entityMat = buildEntityMatrix(e);
            if (e.shape == EntityShape::Model) {
                const GpuModel* gpuModel = getOrLoadModel(e.modelPath);
                if (!gpuModel) continue;
                gl::uniform1i(m_uIsGround, 0);
                if (gpuModel->hasAnimations || gpuModel->hasSkins) {
                    const auto& nodeWorld = getOrEvaluatePose(*gpuModel, e);
                    int uploadedSkin = -1;
                    for (std::size_t ni = 0; ni < gpuModel->nodes.size(); ++ni) {
                        const ModelNode& node = gpuModel->nodes[ni];
                        if (node.subMeshIndices.empty()) continue;
                        if (node.skinIdx >= 0 && static_cast<std::size_t>(node.skinIdx) < gpuModel->skins.size()) {
                            if (uploadedSkin != node.skinIdx) {
                                uploadSkinPalette(gpuModel->skins[static_cast<std::size_t>(node.skinIdx)], nodeWorld, m_uBoneMatrices);
                                uploadedSkin = node.skinIdx;
                            }
                            gl::uniform1i(m_uHasSkin, 1);
                            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                        } else {
                            gl::uniform1i(m_uHasSkin, 0);
                            const glm::mat4 fullMat = entityMat * nodeWorld[ni];
                            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(fullMat));
                        }
                        for (const std::size_t smIdx : node.subMeshIndices) {
                            const GpuSubMesh& sm = gpuModel->subMeshes[smIdx];
                            const glm::vec3 tinted = sm.baseColor * glm::vec3(e.color.r, e.color.g, e.color.b);
                            const glm::vec3 emissive = e.emissive ? tinted * 0.9f : sm.emissive;
                            drawSubMesh(sm, tinted, emissive, sm.metallic, sm.roughness, 0.0f);
                            ++m_lastDrawCalls;
                        }
                    }
                    gl::uniform1i(m_uHasSkin, 0);
                } else if (sky.instancedPropsEnabled && m_propInstVbo != 0) {
                    m_instBatchesScratch[gpuModel].push_back({
                        entityMat,
                        glm::vec4(e.color.r, e.color.g, e.color.b, e.emissive ? 1.0f : 0.0f)
                    });
                } else {
                    gl::uniform1i(m_uHasSkin, 0);
                    gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                    for (const GpuSubMesh& sm : gpuModel->subMeshes) {
                        const glm::vec3 tinted = sm.baseColor * glm::vec3(e.color.r, e.color.g, e.color.b);
                        const glm::vec3 emissive = e.emissive ? tinted * 0.9f : sm.emissive;
                        drawSubMesh(sm, tinted, emissive, sm.metallic, sm.roughness, 0.0f);
                        ++m_lastDrawCalls;
                    }
                }
                continue;
            }

            gl::uniform1i(m_uHasSkin, 0);
            gl::uniform1i(m_uUseInstancing, 0);

            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(entityMat));
            const GpuSubMesh* prim = getPrimitiveMesh(e.shape);
            const bool isGround = (e.shape == EntityShape::Plane && !e.emissive);
            gl::uniform1i(m_uIsGround, isGround ? 1 : 0);
            float pulse = (e.shape == EntityShape::Ring) ? 1.0f : 0.0f;
            float roughness = (e.shape == EntityShape::Plane) ? 0.85f : 0.55f;

            const glm::vec3 baseCol(e.color.r, e.color.g, e.color.b);
            const glm::vec3 emissiveCol = (e.shape == EntityShape::Ring)
                ? baseCol * 0.52f
                : (e.emissive ? baseCol * 0.85f : glm::vec3(0.0f));
            drawSubMesh(*prim, baseCol, emissiveCol, 0.10f, roughness, pulse);
            ++m_lastDrawCalls;
        }

        m_gpuProfiler.begin("opaqueInstanced");
        // Flush Automatic GPU-Instanced Static Prop Batches (Trees, Lamps, Towers, Rocks, Crates in 1 draw call per submesh!)
        if (!m_instBatchesScratch.empty()) {
            gl::uniform1i(m_uHasSkin, 0);
            gl::uniform1i(m_uIsGround, 0);
            for (const auto& [gpuModel, instances] : m_instBatchesScratch) {
                const std::size_t count = instances.size();
                if (count >= 2) {
                    ++m_lastInstancedBatches;
                    m_lastInstancedProps += count;
                    gl::bindBuffer(GL_ARRAY_BUFFER, m_propInstVbo);
                    gl::bufferData(
                        GL_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(count * sizeof(PropInstanceData)),
                        instances.data(),
                        GL_DYNAMIC_DRAW);
                    gl::uniform1i(m_uUseInstancing, 1);
                    for (const GpuSubMesh& sm : gpuModel->subMeshes) {
                        drawSubMeshInstanced(sm, sm.baseColor, sm.emissive, sm.metallic, sm.roughness, 0.0f, static_cast<GLsizei>(count));
                        ++m_lastDrawCalls;
                        m_lastSavedDrawCalls += (count - 1);
                    }
                } else {
                    gl::uniform1i(m_uUseInstancing, 0);
                    gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(instances[0].model));
                    const glm::vec3 tint(instances[0].colorEmissive.r, instances[0].colorEmissive.g, instances[0].colorEmissive.b);
                    const bool isEm = instances[0].colorEmissive.a > 0.5f;
                    for (const GpuSubMesh& sm : gpuModel->subMeshes) {
                        const glm::vec3 tinted = sm.baseColor * tint;
                        const glm::vec3 emissive = isEm ? tinted * 0.9f : sm.emissive;
                        drawSubMesh(sm, tinted, emissive, sm.metallic, sm.roughness, 0.0f);
                        ++m_lastDrawCalls;
                    }
                }
            }
            gl::uniform1i(m_uUseInstancing, 0);
        }

        m_gpuProfiler.begin("opaqueTerrain");
        // Terrain is drawn LAST among opaque geometry on purpose. Its shader is the most
        // expensive (layered procedural noise + bump detail) and it covers most of the screen,
        // so drawing it after buildings, roads and props lets the depth test reject hidden
        // terrain pixels BEFORE they are shaded (early-Z). Opaque + depth-tested = same image.
        if (world.terrain().enabled && m_terrainMesh.indexCount > 0) {
            const glm::mat4 idMat(1.0f);
            gl::uniform1i(m_uHasSkin, 0);
            gl::uniform1i(m_uUseInstancing, 0);
            gl::uniform1i(m_uIsGround, 2);
            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(idMat));
            drawSubMesh(m_terrainMesh, glm::vec3(1.0f), glm::vec3(0.0f), 0.02f, 0.88f, 0.0f);
            ++m_lastDrawCalls;
            gl::uniform1i(m_uIsGround, 0);
        }

        m_gpuProfiler.begin("grass");
        // --- Pass 2B: GPU-Instanced Shadowed Wind-Swaying 4-Blade 3D Grass & Wildflowers (1 Draw Call!) ---
        if (sky.grassEnabled && m_grassVao != 0 && m_grassCount > 0) {
            gl::useProgram(m_grassProgram);
            gl::uniformMatrix4fv(m_grassViewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
            gl::uniform3f(m_grassCamPos, camPos.x, camPos.y, camPos.z);
            gl::uniform3f(m_grassInteractPos, sky.grassInteractPos.x, sky.grassInteractPos.y, sky.grassInteractPos.z);
            gl::uniform1f(m_grassTime, timeSeconds);
            gl::uniform3f(m_grassSunDir, sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z);
            gl::uniform3f(m_grassSunColor, sky.sunColor.r, sky.sunColor.g, sky.sunColor.b);
            gl::uniform3f(m_grassSkyZenith, sky.skyZenithColor.r, sky.skyZenithColor.g, sky.skyZenithColor.b);
            gl::uniform3f(m_grassSkyHorizon, sky.skyHorizonColor.r, sky.skyHorizonColor.g, sky.skyHorizonColor.b);
            gl::uniform3f(m_grassGroundAmbient, sky.groundAmbientColor.r, sky.groundAmbientColor.g, sky.groundAmbientColor.b);
            gl::uniform1f(m_grassAmbientInt, sky.ambientIntensity);
            gl::uniform1f(m_grassFogStart, sky.fogStart);
            gl::uniform1f(m_grassFogEnd, sky.fogEnd);
            gl::uniform1i(m_grassCloudShadows, sky.cloudShadowsEnabled ? 1 : 0);
            gl::uniform1i(m_grassShadowsEnabled, useShadows ? 1 : 0);
            gl::uniformMatrix4fv(m_grassLightVP, 1, GL_FALSE, glm::value_ptr(lightViewProj));
            gl::uniformMatrix4fv(m_grassLightVPMid, 1, GL_FALSE, glm::value_ptr(lightViewProjMid));
            gl::uniform1i(m_grassShadowMap, 1);
            gl::uniform1i(m_grassShadowMapMid, 3);
            if (useShadows) {
                gl::activeTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, m_shadowTex);
                gl::activeTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, m_shadowTexMid);
            }

            gl::bindVertexArray(m_grassVao);
            gl::drawArraysInstanced(GL_TRIANGLES, 0, 24, static_cast<GLsizei>(m_grassCount));
            ++m_lastDrawCalls;
            gl::bindVertexArray(0);
        }

        m_gpuProfiler.begin("water");
        // --- Pass 3: Depth-Buffered 3D Ocean & Lake Water (Soft Shore Fade + Foam + Fresnel!) ---
        if (!m_waterEntitiesScratch.empty()) {
            if (useHdr && m_waterDepthCopyTex != 0) {
                gl::activeTexture(GL_TEXTURE7);
                glBindTexture(GL_TEXTURE_2D, m_waterDepthCopyTex);
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_viewportWidth, m_viewportHeight);
            }
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl::useProgram(m_waterProgram);
            gl::uniformMatrix4fv(m_waterViewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
            gl::uniform1f(m_waterTime, timeSeconds);
            gl::uniform3f(m_waterCamPos, camPos.x, camPos.y, camPos.z);
            gl::uniform3f(m_waterSunDir, sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z);
            gl::uniform3f(m_waterSunColor, sky.sunColor.r, sky.sunColor.g, sky.sunColor.b);
            gl::uniform3f(m_waterSkyZenith, sky.skyZenithColor.r, sky.skyZenithColor.g, sky.skyZenithColor.b);
            gl::uniform3f(m_waterSkyHorizon, sky.skyHorizonColor.r, sky.skyHorizonColor.g, sky.skyHorizonColor.b);
            gl::uniform1f(m_waterFogStart, sky.fogStart);
            gl::uniform1f(m_waterFogEnd, sky.fogEnd);
            gl::uniform1i(m_waterSceneDepthTex, 7);
            gl::uniform1f(m_waterNearPlane, cam.nearPlane);
            gl::uniform1f(m_waterFarPlane, cam.farPlane);
            gl::uniform1i(m_waterPointLightCount, lightCount);
            if (lightCount > 0) {
                gl::uniform4fv(m_waterPointLightPosRadius, lightCount, &lightPosRad[0].x);
                gl::uniform4fv(m_waterPointLightColorInt, lightCount, &lightColInt[0].x);
            }

            gl::bindVertexArray(m_waterMesh.vao);
            for (const Entity* w : m_waterEntitiesScratch) {
                const glm::mat4 modelMat = buildEntityMatrix(*w);
                gl::uniformMatrix4fv(m_waterModel, 1, GL_FALSE, glm::value_ptr(modelMat));
                gl::uniform3f(m_waterShallow, w->color.r, w->color.g, w->color.b);
                glDrawElements(GL_TRIANGLES, m_waterMesh.indexCount, GL_UNSIGNED_INT, nullptr);
                ++m_lastDrawCalls;
            }
            glDisable(GL_BLEND);
        }

        m_gpuProfiler.begin("particles");
        // --- Pass 3B: 3D HDR Particle Effects (Water Splashes, Drift Smoke, Nitro Sparks) ---
        if (!world.particles().empty()) {
            gl::useProgram(m_litProgram);
            gl::uniform1i(m_uHasSkin, 0);
            gl::uniform1i(m_uUseInstancing, 0);
            gl::uniform1i(m_uIsGround, 0);
            gl::uniform1i(m_uHasTexture, 0);
            gl::uniform1i(m_uHasNormalTex, 0);
            gl::uniform1i(m_uHasMetRoughTex, 0);
            gl::bindVertexArray(m_sphereMesh.vao);
            for (const Particle3D& p : world.particles()) {
                const float lifeFrac = 1.0f - std::clamp(p.age / std::max(p.maxLife, 0.01f), 0.0f, 1.0f);
                const float s = p.size * (0.25f + 0.85f * lifeFrac);
                glm::mat4 pMat(1.0f);
                pMat = glm::translate(pMat, glm::vec3(p.position.x, p.position.y, p.position.z));
                pMat = glm::scale(pMat, glm::vec3(s, s, s));
                gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(pMat));
                const glm::vec3 col(p.color.r, p.color.g, p.color.b);
                const glm::vec3 em = p.emissive ? col * (1.8f * lifeFrac) : col * (0.35f * lifeFrac);
                gl::uniform3f(m_uBaseColor, col.r, col.g, col.b);
                gl::uniform3f(m_uEmissive, em.r, em.g, em.b);
                gl::uniform1f(m_uMetallic, 0.0f);
                gl::uniform1f(m_uRoughness, 0.7f);
                gl::uniform1f(m_uPulse, 0.0f);
                glDrawElements(GL_TRIANGLES, m_sphereMesh.indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }

        m_gpuProfiler.begin("rain");
        // --- Pass 3C: GPU-Instanced 3D Falling Rain Streaks (1 Draw Call instead of 140!) ---
        if (sky.rainIntensity > 0.05f && m_propInstVbo != 0) {
            const int dropCount = static_cast<int>(std::clamp(sky.rainIntensity, 0.0f, 1.0f) * 160.0f);
            if (dropCount > 0) {
                m_rainInstScratch.clear();
                m_rainInstScratch.reserve(static_cast<std::size_t>(dropCount));
                constexpr float kBoxSpan = 38.0f;
                constexpr float kHeightSpan = 22.0f;
                const float fallOffset = timeSeconds * 28.0f;
                for (int i = 0; i < dropCount; ++i) {
                    const float fi = static_cast<float>(i);
                    const float hx = std::fmod(std::sin(fi * 12.9898f) * 43758.5453f, 1.0f);
                    const float hy = std::fmod(std::sin(fi * 78.2330f) * 24634.6345f, 1.0f);
                    const float hz = std::fmod(std::sin(fi * 39.4250f) * 19583.2317f, 1.0f);

                    const float rx = camTarget.x + (hx - 0.5f) * kBoxSpan;
                    const float rz = camTarget.z + (hz - 0.5f) * kBoxSpan;
                    float ry = std::fmod(std::abs(hy) * kHeightSpan - fallOffset, kHeightSpan);
                    if (ry < 0.0f) ry += kHeightSpan;
                    ry += std::max(camTarget.y - 3.0f, 0.0f);

                    glm::mat4 rMat(1.0f);
                    rMat = glm::translate(rMat, glm::vec3(rx, ry, rz));
                    rMat = glm::scale(rMat, glm::vec3(0.022f, 0.80f, 0.022f));
                    m_rainInstScratch.push_back({rMat, glm::vec4(0.76f, 0.86f, 0.96f, 1.0f)});
                }
                gl::bindBuffer(GL_ARRAY_BUFFER, m_propInstVbo);
                gl::bufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(m_rainInstScratch.size() * sizeof(PropInstanceData)),
                    m_rainInstScratch.data(),
                    GL_DYNAMIC_DRAW);

                gl::useProgram(m_litProgram);
                gl::uniform1i(m_uHasSkin, 0);
                gl::uniform1i(m_uUseInstancing, 1);
                gl::uniform1i(m_uIsGround, 0);
                gl::uniform1i(m_uHasTexture, 0);
                gl::uniform1i(m_uHasNormalTex, 0);
                gl::uniform1i(m_uHasMetRoughTex, 0);
                gl::uniform3f(m_uBaseColor, 1.0f, 1.0f, 1.0f);
                gl::uniform3f(m_uEmissive, 0.42f, 0.55f, 0.72f);
                gl::uniform1f(m_uMetallic, 0.0f);
                gl::uniform1f(m_uRoughness, 0.3f);
                gl::uniform1f(m_uPulse, 0.0f);
                gl::bindVertexArray(m_boxMesh.vao);
                gl::drawElementsInstanced(
                    GL_TRIANGLES, m_boxMesh.indexCount, GL_UNSIGNED_INT, nullptr,
                    static_cast<GLsizei>(m_rainInstScratch.size()));
                gl::uniform1i(m_uUseInstancing, 0);
                ++m_lastDrawCalls;
                m_lastSavedDrawCalls += static_cast<std::size_t>(std::max(dropCount - 1, 0));
            }
        }

        m_gpuProfiler.begin("post");
        // --- Pass 4 & 5: HDR Bloom Blur + ACES Filmic Tone-Mapping Composite ---
        if (useHdr) {
            glDisable(GL_DEPTH_TEST);
            gl::bindVertexArray(m_fullscreenVao);

            if (sky.bloomEnabled) {
                // 4a. Extract bright HDR highlights to half-res Bloom buffer 0
                gl::bindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
                glViewport(0, 0, m_bloomWidth, m_bloomHeight);
                gl::useProgram(m_bloomExtractProgram);
                gl::activeTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
                gl::uniform1i(m_bloomExtractTex, 0);
                glDrawArrays(GL_TRIANGLES, 0, 3);

                // 4b. 2-iteration separable Gaussian blur (horizontal <-> vertical)
                gl::useProgram(m_bloomBlurProgram);
                gl::uniform1i(m_bloomBlurImage, 0);
                const float invW = 1.0f / static_cast<float>(m_bloomWidth);
                const float invH = 1.0f / static_cast<float>(m_bloomHeight);
                for (int pass = 0; pass < 2; ++pass) {
                    gl::bindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[1]);
                    glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);
                    gl::uniform2f(m_bloomBlurDir, invW, 0.0f);
                    glDrawArrays(GL_TRIANGLES, 0, 3);

                    gl::bindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
                    glBindTexture(GL_TEXTURE_2D, m_bloomTex[1]);
                    gl::uniform2f(m_bloomBlurDir, 0.0f, invH);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                }
            }

            // 5. Final SSAO + ACES Tone-Mapped + Vibrance Composite to OS Window Backbuffer
            gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
            gl::useProgram(m_compositeProgram);
            gl::activeTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
            gl::uniform1i(m_compHdrTex, 0);
            gl::activeTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);
            gl::uniform1i(m_compBloomTex, 2);
            gl::activeTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
            gl::uniform1i(m_compDepthTex, 4);
            gl::uniform1i(m_compBloomEnabled, sky.bloomEnabled ? 1 : 0);
            gl::uniform1f(m_compBloomIntensity, sky.bloomIntensity);
            gl::uniform1i(m_compSsaoEnabled, sky.ssaoEnabled ? 1 : 0);
            gl::uniform1i(m_compFxaaEnabled, sky.fxaaEnabled ? 1 : 0);

            // Project dynamic 3D sun into screen-space UV for Volumetric God-Rays!
            const glm::vec3 sunWorldPos = camPos + glm::normalize(glm::vec3(sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z)) * 450.0f;
            const glm::vec4 sunClip = viewProj * glm::vec4(sunWorldPos, 1.0f);
            const bool sunInFront = (sunClip.w > 0.01f) && (sky.sunDirection.y > 0.05f);
            const glm::vec2 sunUv = sunInFront
                ? (glm::vec2(sunClip.x, sunClip.y) / sunClip.w) * 0.5f + 0.5f
                : glm::vec2(-10.0f, -10.0f);
            const float sunVisFactor = sunInFront
                ? std::clamp(1.0f - glm::length(sunUv - glm::vec2(0.5f)) * 0.65f, 0.0f, 1.0f)
                : 0.0f;
            gl::uniform1i(m_compGodRaysEnabled, (sky.godRaysEnabled && sunVisFactor > 0.02f) ? 1 : 0);
            gl::uniform1f(m_compGodRayIntensity, sky.godRayIntensity * sunVisFactor);
            gl::uniform2f(m_compSunScreenPos, sunUv.x, sunUv.y);
            gl::uniform3f(m_compSunColor, sky.sunColor.r, sky.sunColor.g, sky.sunColor.b);
            gl::uniform1i(m_compSsrEnabled, sky.ssrEnabled ? 1 : 0);
            gl::uniform1f(m_compSsrIntensity, sky.ssrIntensity);
            gl::uniformMatrix4fv(m_compViewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
            gl::uniformMatrix4fv(m_compInvViewProj, 1, GL_FALSE, glm::value_ptr(invViewProj));
            gl::uniform3f(m_compCameraPos, camPos.x, camPos.y, camPos.z);

            gl::uniform1f(m_compExposure, sky.exposure);
            gl::uniform2f(m_compTexelSize, 1.0f / static_cast<float>(m_viewportWidth), 1.0f / static_cast<float>(m_viewportHeight));
            gl::uniform1f(m_compNearPlane, cam.nearPlane);
            gl::uniform1f(m_compFarPlane, cam.farPlane);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glEnable(GL_DEPTH_TEST);
        }

        gl::bindVertexArray(0);
    }

    void endFrame() override {}

    bool captureScreenshot(const std::filesystem::path& outputPath) const override {
        return captureScreenshotScaled(outputPath, 0);
    }

    // Saves the back buffer. `maxWidth` > 0 box-downscales by an integer factor until the
    // image fits; a .jpg/.jpeg extension writes a JPEG (quality 85, ~10x smaller), else PNG.
    bool captureScreenshotScaled(const std::filesystem::path& outputPath, int maxWidth) const override {
        if (!m_initialized || m_viewportWidth <= 0 || m_viewportHeight <= 0) return false;
        int w = m_viewportWidth;
        int h = m_viewportHeight;
        std::vector<unsigned char> raw(static_cast<std::size_t>(w * h * 3));
        std::vector<unsigned char> flipped(static_cast<std::size_t>(w * h * 3));
        gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, raw.data());
        const std::size_t rowBytes = static_cast<std::size_t>(w) * 3;
        for (int y = 0; y < h; ++y) {
            std::copy_n(raw.data() + static_cast<std::size_t>(h - 1 - y) * rowBytes, rowBytes,
                        flipped.data() + static_cast<std::size_t>(y) * rowBytes);
        }
        int factor = 1;
        while (maxWidth > 0 && w / factor > maxWidth && factor < 16) ++factor;
        if (factor > 1) {
            const int sw = w / factor;
            const int sh = h / factor;
            std::vector<unsigned char> small(static_cast<std::size_t>(sw * sh * 3));
            for (int y = 0; y < sh; ++y) {
                for (int x = 0; x < sw; ++x) {
                    for (int c = 0; c < 3; ++c) {
                        int sum = 0;
                        for (int dy = 0; dy < factor; ++dy) {
                            for (int dx = 0; dx < factor; ++dx) {
                                sum += flipped[(static_cast<std::size_t>(y * factor + dy) * w + (x * factor + dx)) * 3 + c];
                            }
                        }
                        small[(static_cast<std::size_t>(y) * sw + x) * 3 + c] = static_cast<unsigned char>(sum / (factor * factor));
                    }
                }
            }
            flipped.swap(small);
            w = sw;
            h = sh;
        }
        std::error_code ec;
        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(outputPath.parent_path(), ec);
        }
        std::string ext = outputPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        const int ok = (ext == ".jpg" || ext == ".jpeg")
            ? stbi_write_jpg(outputPath.string().c_str(), w, h, 3, flipped.data(), 85)
            : stbi_write_png(outputPath.string().c_str(), w, h, 3, flipped.data(), w * 3);
        if (ok) {
            std::cout << "[SAGE][Capture] Saved GPU screenshot -> " << outputPath.string() << " (" << w << "x" << h << ")\n";
        }
        return ok != 0;
    }
    RendererInfo info() const override { return m_info; }
    std::size_t loadedModelCount() const override { return m_modelCache.size(); }
    std::size_t grassInstanceCount() const override { return m_grassCount; }
    std::size_t lastCulledCount() const override { return m_lastCulledCount; }
    std::size_t lastDrawCallCount() const override { return m_lastDrawCalls; }
    std::size_t lastInstancedBatchCount() const override { return m_lastInstancedBatches; }
    std::size_t lastInstancedPropCount() const override { return m_lastInstancedProps; }
    std::size_t lastSavedDrawCalls() const override { return m_lastSavedDrawCalls; }
    std::vector<GpuPassTiming> lastGpuPassTimings() const override { return m_gpuProfiler.last(); }
    std::uint64_t gpuTimingFrameId() const override { return m_gpuProfiler.completedFrames(); }
    std::vector<TimingSample> lastCpuPassTimings() const override { return m_gpuProfiler.lastCpu(); }

private:
    bool m_hdrProgramReady() const { return m_compositeProgram != 0; }

    void destroyGrassBuffers() {
        if (m_grassInstVbo) { gl::deleteBuffers(1, &m_grassInstVbo); m_grassInstVbo = 0; }
        if (m_grassBladeVbo) { gl::deleteBuffers(1, &m_grassBladeVbo); m_grassBladeVbo = 0; }
        if (m_grassVao) { gl::deleteVertexArrays(1, &m_grassVao); m_grassVao = 0; }
        m_grassCount = 0;
        m_grassBuilt = false;
    }

    // Data-driven grass: scatter over the terrain using only the scene's "grass" rules
    // (height band, max slope, avoid flatten/road features). No map-specific coordinates.
    template <typename Rand, typename Inst>
    static void scatterDataDrivenGrass(const World& world, Rand& rand01, std::vector<Inst>& out) {
        const TerrainSettings& cfg = world.terrain();
        const TerrainSpec& spec = cfg.spec;
        if (!spec.grassEnabled || spec.grassCount <= 0) {
            return;
        }
        const float extent = cfg.halfSize * spec.grassArea;
        const std::size_t target = static_cast<std::size_t>(spec.grassCount);
        const int candidates = spec.grassCount * 5;

        const auto insideFeature = [&spec](float x, float z) {
            for (const TerrainFeature& f : spec.features) {
                if (f.type == TerrainFeature::Type::Flatten) {
                    if (std::hypot(x - f.x, z - f.z) < f.radius) return true;
                } else if (f.type == TerrainFeature::Type::Road) {
                    // Point-to-polyline distance (cheap enough at grass-build time).
                    for (std::size_t i = 0; i + 5 < f.points.size(); i += 3) { // x, z, height triplets
                        const float ax = f.points[i], az = f.points[i + 1];
                        const float abx = f.points[i + 3] - ax, abz = f.points[i + 4] - az;
                        const float lenSq = abx * abx + abz * abz;
                        const float t = lenSq > 1e-6f ? std::clamp(((x - ax) * abx + (z - az) * abz) / lenSq, 0.0f, 1.0f) : 0.0f;
                        if (std::hypot(x - (ax + abx * t), z - (az + abz * t)) < f.width * 0.5f + f.blend * 0.5f) return true;
                    }
                }
            }
            return false;
        };

        for (int i = 0; i < candidates && out.size() < target; ++i) {
            const float x = -extent + rand01() * 2.0f * extent;
            const float z = -extent + rand01() * 2.0f * extent;
            const TerrainSample ts = world.sampleTerrain(x, z);
            if (ts.height < spec.grassMinHeight || ts.height > spec.grassMaxHeight) continue;
            if (ts.normal.y < spec.grassMinNormalY) continue;
            if (spec.grassAvoidFeatures && insideFeature(x, z)) continue;
            const float h = 0.30f + rand01() * 0.34f;
            const float yaw = rand01() * 2.0f * kPi;
            const float colShift = rand01();
            const float phase = rand01() * 6.2831f;
            const float flower = (rand01() > 0.93f) ? 1.0f : 0.0f;
            out.push_back({x, ts.height - 0.02f, z, h, yaw, colShift, phase, flower});
        }
    }

    // Uploads the scene's terrain layers/cliff/snow to the lit shader (cheap: 15 vec4s per frame).
    void uploadTerrainMaterial(const TerrainSpec& spec) {
        std::array<float, 12 * 4> packed{};
        const int count = static_cast<int>(std::min<std::size_t>(spec.layers.size(), 6));
        for (int i = 0; i < count; ++i) {
            const TerrainLayer& l = spec.layers[static_cast<std::size_t>(i)];
            float* a = &packed[static_cast<std::size_t>(i * 8)];
            a[0] = l.color.r; a[1] = l.color.g; a[2] = l.color.b; a[3] = l.roughness;
            a[4] = l.startHeight; a[5] = l.blend; a[6] = 0.0f; a[7] = 0.0f;
        }
        gl::uniform4fv(m_uTLayers, 12, packed.data());
        gl::uniform1i(m_uTLayerCount, count);
        const float cliff[4] = {spec.cliffColor.r, spec.cliffColor.g, spec.cliffColor.b, 1.0f};
        gl::uniform4fv(m_uTCliffColor, 1, cliff);
        const float slopeSnow[4] = {spec.cliffSlopeStart, spec.cliffSlopeEnd, spec.snowStartHeight, spec.snowEndHeight};
        gl::uniform4fv(m_uTSlopeSnow, 1, slopeSnow);
        const float snow[4] = {spec.snowColor.r, spec.snowColor.g, spec.snowColor.b, spec.snowEnabled ? 1.0f : 0.0f};
        gl::uniform4fv(m_uTSnowColor, 1, snow);
    }

    void ensureGrassField(const World& world) {
        if (m_grassBuilt && m_grassModeCached == world.terrain().mode &&
            m_grassRevisionCached == world.terrain().revision) return;
        destroyGrassBuffers();
        m_grassModeCached = world.terrain().mode;
        m_grassRevisionCached = world.terrain().revision;

        // Lush 4-blade curved slender meadow grass tuft (8 triangles = 24 vertices per instance!)
        constexpr float kW = 0.044f;
        constexpr float kBladeVerts[24 * 3] = {
            // Blade 1 (0 deg, gentle forward arch)
            -kW,        0.0f,  0.00f,    kW,        0.0f,  0.00f,    kW * 0.48f, 0.56f,  0.04f,
            -kW,        0.0f,  0.00f,    kW * 0.48f, 0.56f,  0.04f,   0.00f,      1.00f,  0.11f,
            // Blade 2 (~55 deg, arching out-right)
             0.03f - kW * 0.55f, 0.0f, -0.02f - kW * 0.80f,
             0.03f + kW * 0.55f, 0.0f, -0.02f + kW * 0.80f,
             0.07f + kW * 0.25f, 0.52f, -0.04f + kW * 0.35f,
             0.03f - kW * 0.55f, 0.0f, -0.02f - kW * 0.80f,
             0.07f + kW * 0.25f, 0.52f, -0.04f + kW * 0.35f,
             0.13f,              0.92f, -0.07f,
            // Blade 3 (~115 deg, arching out-left)
            -0.04f - kW * 0.82f, 0.0f,  0.02f + kW * 0.52f,
            -0.04f + kW * 0.82f, 0.0f,  0.02f - kW * 0.52f,
            -0.08f + kW * 0.36f, 0.54f,  0.05f - kW * 0.22f,
            -0.04f - kW * 0.82f, 0.0f,  0.02f + kW * 0.52f,
            -0.08f + kW * 0.36f, 0.54f,  0.05f - kW * 0.22f,
            -0.14f,              0.88f,  0.08f,
            // Blade 4 (~162 deg, low filler tuft arching back)
            -0.01f - kW * 0.90f, 0.0f, -0.04f - kW * 0.28f,
            -0.01f + kW * 0.90f, 0.0f, -0.04f + kW * 0.28f,
            -0.02f + kW * 0.40f, 0.48f, -0.08f + kW * 0.14f,
            -0.01f - kW * 0.90f, 0.0f, -0.04f - kW * 0.28f,
            -0.02f + kW * 0.40f, 0.48f, -0.08f + kW * 0.14f,
            -0.03f,              0.78f, -0.14f,
        };

        struct GrassInst {
            float px, py, pz, h;
            float yaw, colorShift, windPhase, flower;
        };
        std::vector<GrassInst> instances;
        instances.reserve(56000);

        std::uint32_t rng = 0x5A6E1600u;
        auto rand01 = [&]() -> float {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return static_cast<float>(rng & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
        };

        // Scatter lush shadowed meadow grass tufts across natural biomes!
        if (world.terrain().mode == TerrainSettings::kModeDataDriven) {
            scatterDataDrivenGrass(world, rand01, instances);
        }
        const bool islandMode = (world.terrain().mode == TerrainSettings::kModeEchoIslandPreset);
        const int kCandidates = (world.terrain().mode == TerrainSettings::kModeDataDriven) ? 0 : (islandMode ? 135000 : 85000);
        for (int i = 0; i < kCandidates; ++i) {
            float x = 0.0f;
            float z = 0.0f;
            if (islandMode) {
                x = -255.0f + rand01() * 510.0f;
                z = -255.0f + rand01() * 510.0f;
                const TerrainSample ts = world.sampleTerrain(x, z);
                if (ts.height < 2.85f || ts.height > 23.8f || ts.normal.y < 0.78f) continue;
                const float h = 0.22f + rand01() * 0.26f;
                const float yaw = rand01() * 2.0f * kPi;
                const float colShift = rand01();
                const float phase = rand01() * 6.2831f;
                const float flower = (rand01() > 0.94f) ? 1.0f : 0.0f;
                instances.push_back({x, ts.height - 0.03f, z, h, yaw, colShift, phase, flower});
                if (instances.size() >= 54000) break;
                continue;
            }
            x = -255.0f + rand01() * 510.0f;
            z = -40.0f + rand01() * 295.0f; // Northern lush biomes (NE Highlands + NW Parkland)
            const float ax = std::abs(x);
            const float az = std::abs(z);
            const float cheb = std::max(ax, az);

            // Exclude highways, 3 ring roads, Downtown core & Marina lagoon!
            if (ax < 15.0f || az < 15.0f) continue;
            if (std::abs(cheb - 64.0f) < 13.0f || std::abs(cheb - 148.0f) < 14.0f || std::abs(cheb - 236.0f) < 14.0f) continue;
            if (cheb < 66.0f) continue; // Keep Downtown Plaza crisp
            if (x < 0.0f && cheb < 158.0f) continue; // Keep NW city blocks clean asphalt
            if (z < 12.0f) continue;

            const TerrainSample ts = world.sampleTerrain(x, z);
            if (ts.height > 16.0f || ts.normal.y < 0.72f) continue; // Keep off sheer cliffs & snow peaks

            const float h = 0.26f + rand01() * 0.28f;
            const float yaw = rand01() * 2.0f * kPi;
            const float colShift = rand01();
            const float phase = rand01() * 6.2831f;
            const float flower = (rand01() > 0.94f) ? 1.0f : 0.0f;
            instances.push_back({x, ts.height - 0.02f, z, h, yaw, colShift, phase, flower});
            if (instances.size() >= 38000) break;
        }

        m_grassCount = instances.size();
        if (m_grassCount == 0) {
            m_grassBuilt = true;
            return;
        }

        gl::genVertexArrays(1, &m_grassVao);
        gl::genBuffers(1, &m_grassBladeVbo);
        gl::genBuffers(1, &m_grassInstVbo);
        gl::bindVertexArray(m_grassVao);

        gl::bindBuffer(GL_ARRAY_BUFFER, m_grassBladeVbo);
        gl::bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(kBladeVerts)), kBladeVerts, GL_STATIC_DRAW);
        gl::vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        gl::enableVertexAttribArray(0);

        gl::bindBuffer(GL_ARRAY_BUFFER, m_grassInstVbo);
        gl::bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size() * sizeof(GrassInst)), instances.data(), GL_STATIC_DRAW);
        gl::vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GrassInst), reinterpret_cast<void*>(offsetof(GrassInst, px)));
        gl::enableVertexAttribArray(1);
        gl::vertexAttribDivisor(1, 1);
        gl::vertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GrassInst), reinterpret_cast<void*>(offsetof(GrassInst, yaw)));
        gl::enableVertexAttribArray(2);
        gl::vertexAttribDivisor(2, 1);

        gl::bindVertexArray(0);
        m_grassBuilt = true;
        std::cout << "[SAGE][Foliage] Built GPU-instanced shadowed grass field (" << m_grassCount
                  << " 4-blade tufts = " << (m_grassCount * 4) << " wind-swaying 3D blades, 1 draw call)\n";
    }

    void ensureTerrainMesh(const World& world) {
        const TerrainSettings& cfg = world.terrain();
        if (m_terrainMeshBuilt &&
            m_terrainRevisionCached == cfg.revision &&
            m_terrainModeCached == cfg.mode &&
            std::abs(m_terrainHalfSizeCached - cfg.halfSize) < 0.01f &&
            std::abs(m_terrainMaxHeightCached - cfg.maxHeight) < 0.01f &&
            m_terrainGridCached == cfg.gridResolution) {
            return;
        }
        m_terrainMesh.destroy();

        const int divs = std::clamp(cfg.gridResolution, 32, 240);
        const int stride = divs + 1;
        const float half = cfg.halfSize;
        const float step = (half * 2.0f) / static_cast<float>(divs);
        const bool islandMode = (cfg.mode == TerrainSettings::kModeEchoIslandPreset);
        const bool dataDriven = (cfg.mode == TerrainSettings::kModeDataDriven);
        const bool cityPreset = !islandMode && !dataDriven;
        const float hideBelow = cfg.spec.hideBelowHeight;

        std::vector<Vertex3D> verts;
        std::vector<float> rawH(static_cast<std::size_t>(stride * stride), 0.0f);
        verts.reserve(static_cast<std::size_t>(stride * stride));

        for (int iz = 0; iz <= divs; ++iz) {
            const float z = -half + static_cast<float>(iz) * step;
            const float v = static_cast<float>(iz) / static_cast<float>(divs);
            for (int ix = 0; ix <= divs; ++ix) {
                const float x = -half + static_cast<float>(ix) * step;
                const float u = static_cast<float>(ix) / static_cast<float>(divs);
                const TerrainSample s = world.sampleTerrain(x, z);
                rawH[static_cast<std::size_t>(iz * stride + ix)] = s.height;
                float y = s.height;
                if (cityPreset) {
                    // Sink flat zones 0.22m below city roads so zero Z-fighting ever occurs with flat ground/roads!
                    const float riseT = std::clamp(s.height / 0.45f, 0.0f, 1.0f);
                    const float smoothRise = riseT * riseT * (3.0f - 2.0f * riseT);
                    y = s.height - 0.22f * (1.0f - smoothRise);
                }
                verts.push_back({x, y, z, s.normal.x, s.normal.y, s.normal.z, u * 32.0f, v * 32.0f});
            }
        }

        std::vector<std::uint32_t> idx;
        idx.reserve(static_cast<std::size_t>(divs * divs * 6));
        for (int iz = 0; iz < divs; ++iz) {
            for (int ix = 0; ix < divs; ++ix) {
                const std::size_t i0 = static_cast<std::size_t>(iz * stride + ix);
                const std::size_t i1 = i0 + 1;
                const std::size_t i2 = static_cast<std::size_t>((iz + 1) * stride + ix);
                const std::size_t i3 = i2 + 1;
                if (dataDriven && rawH[i0] < hideBelow && rawH[i1] < hideBelow && rawH[i2] < hideBelow && rawH[i3] < hideBelow) {
                    continue;
                }
                if (cityPreset && rawH[i0] < 0.02f && rawH[i1] < 0.02f && rawH[i2] < 0.02f && rawH[i3] < 0.02f) {
                    continue;
                }
                if (islandMode && rawH[i0] < -5.8f && rawH[i1] < -5.8f && rawH[i2] < -5.8f && rawH[i3] < -5.8f) {
                    continue;
                }
                idx.insert(idx.end(), {
                    static_cast<std::uint32_t>(i0), static_cast<std::uint32_t>(i2), static_cast<std::uint32_t>(i1),
                    static_cast<std::uint32_t>(i1), static_cast<std::uint32_t>(i2), static_cast<std::uint32_t>(i3)
                });
            }
        }

        if (!idx.empty()) {
            m_terrainMesh.upload(verts, idx);
        }
        m_terrainMeshBuilt = true;
        m_terrainModeCached = cfg.mode;
        m_terrainRevisionCached = cfg.revision;
        m_terrainHalfSizeCached = cfg.halfSize;
        m_terrainMaxHeightCached = cfg.maxHeight;
        m_terrainGridCached = cfg.gridResolution;
        std::cout << "[SAGE][Terrain] Built GPU 3D heightmap mesh (" << (idx.size() / 3)
                  << " triangles, mode=" << (dataDriven ? "data-driven" : (islandMode ? "island" : "city"))
                  << ", +/-" << half << "m, maxH=" << cfg.maxHeight << "m)\n";
    }

    const std::vector<glm::mat4>& getOrEvaluatePose(const GpuModel& model, const Entity& e) {
        CachedPose& entry = m_framePoseCache[&e];
        if (entry.frameId != m_frameCounter || entry.worldMats.size() != model.nodes.size()) {
            const LookAtComponent* look = m_frameWorld ? m_frameWorld->lookAt(e.handle) : nullptr;
            const RagdollComponent* rag = m_frameWorld ? m_frameWorld->ragdoll(e.handle) : nullptr;
            evaluateModelNodeMatrices(model, e, look, rag, m_animScratch, entry.worldMats);
            entry.frameId = m_frameCounter;
        }
        return entry.worldMats;
    }

    void uploadSkinPalette(const GpuSkin& skin, const std::vector<glm::mat4>& nodeWorld, GLint uniformLoc) {
        const std::size_t count = std::min<std::size_t>(skin.jointNodeIndices.size(), static_cast<std::size_t>(kMaxSkinBones));
        m_bonePaletteScratch.resize(count);
        for (std::size_t j = 0; j < count; ++j) {
            const int nodeIdx = skin.jointNodeIndices[j];
            if (nodeIdx >= 0 && static_cast<std::size_t>(nodeIdx) < nodeWorld.size()) {
                m_bonePaletteScratch[j] = nodeWorld[static_cast<std::size_t>(nodeIdx)] * skin.inverseBindMatrices[j];
            } else {
                m_bonePaletteScratch[j] = glm::mat4(1.0f);
            }
        }
        if (count > 0) {
            gl::uniformMatrix4fv(uniformLoc, static_cast<GLsizei>(count), GL_FALSE, glm::value_ptr(m_bonePaletteScratch[0]));
        }
    }

    const GpuSubMesh* getPrimitiveMesh(EntityShape shape) const {
        switch (shape) {
            case EntityShape::Plane: return &m_planeMesh;
            case EntityShape::Water: return &m_waterMesh;
            case EntityShape::Sphere: return &m_sphereMesh;
            case EntityShape::Triangle: return &m_arrowMesh;
            case EntityShape::Ring: return &m_ringMesh;
            case EntityShape::Box:
            default: return &m_boxMesh;
        }
    }

    void drawSubMeshInstanced(
        const GpuSubMesh& sm,
        const glm::vec3& baseColor,
        const glm::vec3& emissive,
        float metallic,
        float roughness,
        float pulse,
        GLsizei instanceCount) const {
        gl::uniform3f(m_uBaseColor, baseColor.r, baseColor.g, baseColor.b);
        gl::uniform3f(m_uEmissive, emissive.r, emissive.g, emissive.b);
        gl::uniform1f(m_uMetallic, metallic);
        gl::uniform1f(m_uRoughness, roughness);
        gl::uniform1f(m_uPulse, pulse);

        if (sm.albedoTex != 0) {
            gl::activeTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sm.albedoTex);
            gl::uniform1i(m_uHasTexture, 1);
        } else {
            gl::uniform1i(m_uHasTexture, 0);
        }
        if (sm.normalTex != 0) {
            gl::activeTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, sm.normalTex);
            gl::uniform1i(m_uHasNormalTex, 1);
        } else {
            gl::uniform1i(m_uHasNormalTex, 0);
        }
        if (sm.metRoughTex != 0) {
            gl::activeTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, sm.metRoughTex);
            gl::uniform1i(m_uHasMetRoughTex, 1);
        } else {
            gl::uniform1i(m_uHasMetRoughTex, 0);
        }

        gl::bindVertexArray(sm.vao);
        gl::drawElementsInstanced(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr, instanceCount);
    }

    void drawSubMesh(
        const GpuSubMesh& sm,
        const glm::vec3& baseColor,
        const glm::vec3& emissive,
        float metallic,
        float roughness,
        float pulse) const {
        gl::uniform3f(m_uBaseColor, baseColor.r, baseColor.g, baseColor.b);
        gl::uniform3f(m_uEmissive, emissive.r, emissive.g, emissive.b);
        gl::uniform1f(m_uMetallic, metallic);
        gl::uniform1f(m_uRoughness, roughness);
        gl::uniform1f(m_uPulse, pulse);

        if (sm.albedoTex != 0) {
            gl::activeTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sm.albedoTex);
            gl::uniform1i(m_uHasTexture, 1);
        } else {
            gl::uniform1i(m_uHasTexture, 0);
        }
        if (sm.normalTex != 0) {
            gl::activeTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, sm.normalTex);
            gl::uniform1i(m_uHasNormalTex, 1);
        } else {
            gl::uniform1i(m_uHasNormalTex, 0);
        }
        if (sm.metRoughTex != 0) {
            gl::activeTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, sm.metRoughTex);
            gl::uniform1i(m_uHasMetRoughTex, 1);
        } else {
            gl::uniform1i(m_uHasMetRoughTex, 0);
        }

        gl::bindVertexArray(sm.vao);
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr);
    }

    std::filesystem::path resolveModelPath(const std::string& rawPath) const {
        const std::filesystem::path p(rawPath);
        if (std::filesystem::exists(p)) return p;
        if (!m_assetRoot.empty()) {
            const std::filesystem::path fromRoot = m_assetRoot / p;
            if (std::filesystem::exists(fromRoot)) return fromRoot;
        }
        const std::filesystem::path parentCandidate = std::filesystem::path("..") / p;
        if (std::filesystem::exists(parentCandidate)) return parentCandidate;
        return p;
    }

    const GpuModel* getOrLoadModel(const std::string& rawPath) {
        if (const auto it = m_modelCache.find(rawPath); it != m_modelCache.end()) return &it->second;
        if (m_failedModels.contains(rawPath)) return nullptr;

        const std::filesystem::path fullPath = resolveModelPath(rawPath);
        if (!std::filesystem::exists(fullPath)) {
            std::cerr << "[SAGE][Model] glTF/.glb file not found: \"" << rawPath << "\"\n";
            m_failedModels.insert(rawPath);
            return nullptr;
        }

        cgltf_options options{};
        cgltf_data* data = nullptr;
        const std::string pathStr = fullPath.string();
        if (cgltf_parse_file(&options, pathStr.c_str(), &data) != cgltf_result_success) {
            std::cerr << "[SAGE][Model] Failed to parse glTF/.glb file: " << pathStr << "\n";
            m_failedModels.insert(rawPath);
            return nullptr;
        }
        if (cgltf_load_buffers(&options, data, pathStr.c_str()) != cgltf_result_success) {
            std::cerr << "[SAGE][Model] Failed to load binary buffers for: " << pathStr << "\n";
            cgltf_free(data);
            m_failedModels.insert(rawPath);
            return nullptr;
        }

        GpuModel gpuModel;
        const std::filesystem::path modelDir = fullPath.parent_path();
        gpuModel.hasAnimations = (data->animations_count > 0);
        gpuModel.hasSkins = (data->skins_count > 0);

        glm::vec3 bbMin( 1e9f,  1e9f,  1e9f);
        glm::vec3 bbMax(-1e9f, -1e9f, -1e9f);
        bool hasBounds = false;

        // CPU copy of each sub-mesh's geometry (aligned with gpuModel.subMeshes), kept only
        // while loading so depth-only shadow meshes can be merged afterwards.
        std::vector<std::pair<std::vector<Vertex3D>, std::vector<std::uint32_t>>> cpuGeom;

        auto uploadMeshPrimitives = [&](const cgltf_mesh* mesh, const glm::mat4& bakeTransform, std::vector<std::size_t>* outIndices) {
            const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(bakeTransform)));
            for (cgltf_size pIdx = 0; pIdx < mesh->primitives_count; ++pIdx) {
                const cgltf_primitive& prim = mesh->primitives[pIdx];
                if (prim.type != cgltf_primitive_type_triangles) continue;
                const cgltf_accessor* posAcc = nullptr;
                const cgltf_accessor* normAcc = nullptr;
                const cgltf_accessor* uvAcc = nullptr;
                const cgltf_accessor* jointsAcc = nullptr;
                const cgltf_accessor* weightsAcc = nullptr;
                for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                    const cgltf_attribute& attr = prim.attributes[a];
                    if (attr.type == cgltf_attribute_type_position) posAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_normal) normAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uvAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_joints && attr.index == 0) jointsAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_weights && attr.index == 0) weightsAcc = attr.data;
                }
                if (!posAcc || posAcc->count == 0) continue;

                std::vector<Vertex3D> vertices(posAcc->count);
                for (cgltf_size v = 0; v < posAcc->count; ++v) {
                    float pos[3] = {0, 0, 0}, norm[3] = {0, 1, 0}, uv[2] = {0, 0};
                    cgltf_uint jIdx[4] = {0, 0, 0, 0};
                    float jW[4] = {1.0f, 0.0f, 0.0f, 0.0f};
                    cgltf_accessor_read_float(posAcc, v, pos, 3);
                    if (normAcc) cgltf_accessor_read_float(normAcc, v, norm, 3);
                    if (uvAcc) cgltf_accessor_read_float(uvAcc, v, uv, 2);
                    if (jointsAcc) cgltf_accessor_read_uint(jointsAcc, v, jIdx, 4);
                    if (weightsAcc) {
                        cgltf_accessor_read_float(weightsAcc, v, jW, 4);
                        const float sum = jW[0] + jW[1] + jW[2] + jW[3];
                        if (sum > 0.0001f) {
                            jW[0] /= sum; jW[1] /= sum; jW[2] /= sum; jW[3] /= sum;
                        } else {
                            jW[0] = 1.0f; jW[1] = jW[2] = jW[3] = 0.0f;
                        }
                    }
                    const glm::vec4 wp = bakeTransform * glm::vec4(pos[0], pos[1], pos[2], 1.0f);
                    bbMin = glm::min(bbMin, glm::vec3(wp));
                    bbMax = glm::max(bbMax, glm::vec3(wp));
                    hasBounds = true;
                    glm::vec3 wn = normalMat * glm::vec3(norm[0], norm[1], norm[2]);
                    wn = glm::length(wn) > 0.0001f ? glm::normalize(wn) : glm::vec3(0, 1, 0);
                    vertices[v] = {
                        wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, uv[0], uv[1],
                        static_cast<float>(jIdx[0]), static_cast<float>(jIdx[1]),
                        static_cast<float>(jIdx[2]), static_cast<float>(jIdx[3]),
                        jW[0], jW[1], jW[2], jW[3]
                    };
                }

                std::vector<std::uint32_t> indices;
                if (prim.indices) {
                    indices.resize(prim.indices->count);
                    for (cgltf_size i = 0; i < prim.indices->count; ++i)
                        indices[i] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i));
                } else {
                    indices.resize(vertices.size());
                    for (std::size_t i = 0; i < vertices.size(); ++i) indices[i] = static_cast<std::uint32_t>(i);
                }

                if (!normAcc && indices.size() >= 3) {
                    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
                        auto& v0 = vertices[indices[i]];
                        auto& v1 = vertices[indices[i + 1]];
                        auto& v2 = vertices[indices[i + 2]];
                        glm::vec3 n = glm::cross(
                            glm::vec3(v1.px - v0.px, v1.py - v0.py, v1.pz - v0.pz),
                            glm::vec3(v2.px - v0.px, v2.py - v0.py, v2.pz - v0.pz));
                        if (glm::length(n) > 0.0001f) {
                            n = glm::normalize(n);
                            v0.nx = v1.nx = v2.nx = n.x;
                            v0.ny = v1.ny = v2.ny = n.y;
                            v0.nz = v1.nz = v2.nz = n.z;
                        }
                    }
                }

                cpuGeom.emplace_back(vertices, indices);
                GpuSubMesh sm;
                sm.upload(vertices, indices);
                sm.attachInstanceBuffer(m_propInstVbo);
                if (prim.material) {
                    const cgltf_material* mat = prim.material;
                    if (mat->has_pbr_metallic_roughness) {
                        const auto& pbr = mat->pbr_metallic_roughness;
                        sm.baseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]};
                        sm.metallic = pbr.metallic_factor;
                        sm.roughness = pbr.roughness_factor;
                        if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                            sm.albedoTex = loadGltfImageTexture(pbr.base_color_texture.texture->image, modelDir);
                        }
                        if (pbr.metallic_roughness_texture.texture && pbr.metallic_roughness_texture.texture->image) {
                            sm.metRoughTex = loadGltfImageTexture(pbr.metallic_roughness_texture.texture->image, modelDir);
                        }
                    }
                    if (mat->normal_texture.texture && mat->normal_texture.texture->image) {
                        sm.normalTex = loadGltfImageTexture(mat->normal_texture.texture->image, modelDir);
                    }
                    sm.emissive = {mat->emissive_factor[0], mat->emissive_factor[1], mat->emissive_factor[2]};
                }
                const std::size_t newIdx = gpuModel.subMeshes.size();
                gpuModel.subMeshes.push_back(sm);
                if (outIndices) outIndices->push_back(newIdx);
            }
        };

        if ((gpuModel.hasAnimations || gpuModel.hasSkins) && data->nodes_count > 0) {
            gpuModel.skins.resize(data->skins_count);
            for (cgltf_size s = 0; s < data->skins_count; ++s) {
                const cgltf_skin& srcSkin = data->skins[s];
                GpuSkin& dstSkin = gpuModel.skins[s];
                dstSkin.jointNodeIndices.resize(srcSkin.joints_count, -1);
                dstSkin.inverseBindMatrices.assign(srcSkin.joints_count, glm::mat4(1.0f));
                for (cgltf_size j = 0; j < srcSkin.joints_count; ++j) {
                    if (srcSkin.joints[j]) {
                        dstSkin.jointNodeIndices[j] = static_cast<int>(srcSkin.joints[j] - data->nodes);
                    }
                    if (srcSkin.inverse_bind_matrices && j < srcSkin.inverse_bind_matrices->count) {
                        float m[16] = {};
                        cgltf_accessor_read_float(srcSkin.inverse_bind_matrices, j, m, 16);
                        dstSkin.inverseBindMatrices[j] = glm::make_mat4(m);
                    }
                }
            }

            gpuModel.nodes.resize(data->nodes_count);
            for (cgltf_size n = 0; n < data->nodes_count; ++n) {
                const cgltf_node* src = &data->nodes[n];
                ModelNode& dst = gpuModel.nodes[n];
                dst.parentIdx = src->parent ? static_cast<int>(src->parent - data->nodes) : -1;
                dst.skinIdx = src->skin ? static_cast<int>(src->skin - data->skins) : -1;
                dst.boneRole = classifyBoneRole(src->name);
                if (dst.boneRole == BoneRole::Head) ++gpuModel.headBoneCount;
                else if (dst.boneRole == BoneRole::Neck) ++gpuModel.neckBoneCount;
                else if (dst.boneRole == BoneRole::Chest) ++gpuModel.chestBoneCount;
                if (src->has_matrix) { dst.hasMatrix = true; dst.staticMatrix = glm::make_mat4(src->matrix); }
                if (src->has_translation) dst.translation = {src->translation[0], src->translation[1], src->translation[2]};
                if (src->has_rotation) dst.rotation = glm::quat(src->rotation[3], src->rotation[0], src->rotation[1], src->rotation[2]);
                if (src->has_scale) dst.scale = {src->scale[0], src->scale[1], src->scale[2]};
                if (src->mesh) uploadMeshPrimitives(src->mesh, glm::mat4(1.0f), &dst.subMeshIndices);
            }
            // Precompute rest-pose world rotation per bone for exact entity-space Look-At IK on bipeds & quadrupeds!
            {
                const std::size_t nc = gpuModel.nodes.size();
                std::vector<glm::mat4> restW(nc, glm::mat4(1.0f));
                std::vector<std::uint8_t> done(nc, 0);
                auto evalRest = [&](auto&& self, std::size_t idx) -> const glm::mat4& {
                    if (done[idx]) return restW[idx];
                    const ModelNode& nd = gpuModel.nodes[idx];
                    const glm::mat4 loc = nd.hasMatrix
                        ? nd.staticMatrix
                        : (glm::translate(glm::mat4(1.0f), nd.translation) * glm::mat4_cast(nd.rotation) * glm::scale(glm::mat4(1.0f), nd.scale));
                    restW[idx] = (nd.parentIdx >= 0 && static_cast<std::size_t>(nd.parentIdx) < nc)
                        ? self(self, static_cast<std::size_t>(nd.parentIdx)) * loc
                        : loc;
                    done[idx] = 1;
                    return restW[idx];
                };
                for (std::size_t i = 0; i < nc; ++i) {
                    const glm::mat4& m = evalRest(evalRest, i);
                    glm::vec3 c0(m[0]), c1(m[1]), c2(m[2]);
                    if (glm::length(c0) > 1e-5f) c0 = glm::normalize(c0); else c0 = glm::vec3(1, 0, 0);
                    if (glm::length(c1) > 1e-5f) c1 = glm::normalize(c1); else c1 = glm::vec3(0, 1, 0);
                    if (glm::length(c2) > 1e-5f) c2 = glm::normalize(c2); else c2 = glm::vec3(0, 0, 1);
                    if (glm::dot(glm::cross(c0, c1), c2) < 0.0f) c2 = -c2;
                    gpuModel.nodes[i].restWorldRot = glm::normalize(glm::quat_cast(glm::mat3(c0, c1, c2)));
                }
            }
            for (cgltf_size a = 0; a < data->animations_count; ++a) {
                const cgltf_animation& srcAnim = data->animations[a];
                AnimClip clip;
                clip.name = srcAnim.name ? srcAnim.name : ("Clip_" + std::to_string(a));
                for (cgltf_size c = 0; c < srcAnim.channels_count; ++c) {
                    const cgltf_animation_channel& srcCh = srcAnim.channels[c];
                    if (!srcCh.target_node || !srcCh.sampler || !srcCh.sampler->input || !srcCh.sampler->output) continue;
                    AnimChannel ch;
                    ch.nodeIdx = static_cast<int>(srcCh.target_node - data->nodes);
                    if (srcCh.target_path == cgltf_animation_path_type_translation) ch.path = AnimPath::Translation;
                    else if (srcCh.target_path == cgltf_animation_path_type_rotation) ch.path = AnimPath::Rotation;
                    else if (srcCh.target_path == cgltf_animation_path_type_scale) ch.path = AnimPath::Scale;
                    else continue;
                    const cgltf_accessor* inAcc = srcCh.sampler->input;
                    const cgltf_accessor* outAcc = srcCh.sampler->output;
                    const cgltf_size count = std::min(inAcc->count, outAcc->count);
                    ch.times.resize(count);
                    ch.values.resize(count);
                    const int comps = (ch.path == AnimPath::Rotation) ? 4 : 3;
                    for (cgltf_size k = 0; k < count; ++k) {
                        float tm = 0.0f, vals[4] = {0, 0, 0, 1};
                        cgltf_accessor_read_float(inAcc, k, &tm, 1);
                        cgltf_accessor_read_float(outAcc, k, vals, comps);
                        ch.times[k] = tm;
                        ch.values[k] = {vals[0], vals[1], vals[2], vals[3]};
                        clip.duration = std::max(clip.duration, tm);
                    }
                    clip.channels.push_back(std::move(ch));
                }
                if (!clip.channels.empty()) gpuModel.animations.push_back(std::move(clip));
            }
        } else {
            // Static glTF/.glb Material Submesh Merging (v1.6.0: merges identical-material primitives into 1 draw call!)
            struct StaticMatBucket {
                const cgltf_material* mat = nullptr;
                std::vector<Vertex3D> verts;
                std::vector<std::uint32_t> indices;
            };
            std::vector<StaticMatBucket> buckets;
            std::unordered_map<const cgltf_material*, std::size_t> matToBucket;

            auto collectStaticMesh = [&](const cgltf_mesh* mesh, const glm::mat4& bakeTransform) {
                const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(bakeTransform)));
                for (cgltf_size pIdx = 0; pIdx < mesh->primitives_count; ++pIdx) {
                    const cgltf_primitive& prim = mesh->primitives[pIdx];
                    if (prim.type != cgltf_primitive_type_triangles) continue;
                    const cgltf_accessor* posAcc = nullptr;
                    const cgltf_accessor* normAcc = nullptr;
                    const cgltf_accessor* uvAcc = nullptr;
                    for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                        const cgltf_attribute& attr = prim.attributes[a];
                        if (attr.type == cgltf_attribute_type_position) posAcc = attr.data;
                        else if (attr.type == cgltf_attribute_type_normal) normAcc = attr.data;
                        else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uvAcc = attr.data;
                    }
                    if (!posAcc || posAcc->count == 0) continue;

                    std::size_t bIdx = 0;
                    if (const auto it = matToBucket.find(prim.material); it != matToBucket.end()) {
                        bIdx = it->second;
                    } else {
                        bIdx = buckets.size();
                        matToBucket[prim.material] = bIdx;
                        buckets.push_back({prim.material, {}, {}});
                    }
                    StaticMatBucket& bucket = buckets[bIdx];
                    const auto baseVertex = static_cast<std::uint32_t>(bucket.verts.size());
                    bucket.verts.resize(baseVertex + posAcc->count);

                    for (cgltf_size v = 0; v < posAcc->count; ++v) {
                        float pos[3] = {0, 0, 0}, norm[3] = {0, 1, 0}, uv[2] = {0, 0};
                        cgltf_accessor_read_float(posAcc, v, pos, 3);
                        if (normAcc) cgltf_accessor_read_float(normAcc, v, norm, 3);
                        if (uvAcc) cgltf_accessor_read_float(uvAcc, v, uv, 2);
                        const glm::vec4 wp = bakeTransform * glm::vec4(pos[0], pos[1], pos[2], 1.0f);
                        bbMin = glm::min(bbMin, glm::vec3(wp));
                        bbMax = glm::max(bbMax, glm::vec3(wp));
                        hasBounds = true;
                        glm::vec3 wn = normalMat * glm::vec3(norm[0], norm[1], norm[2]);
                        wn = glm::length(wn) > 0.0001f ? glm::normalize(wn) : glm::vec3(0, 1, 0);
                        bucket.verts[baseVertex + v] = {wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, uv[0], uv[1]};
                    }

                    const std::size_t idxStart = bucket.indices.size();
                    if (prim.indices) {
                        bucket.indices.resize(idxStart + prim.indices->count);
                        for (cgltf_size i = 0; i < prim.indices->count; ++i) {
                            bucket.indices[idxStart + i] = baseVertex + static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i));
                        }
                    } else {
                        bucket.indices.resize(idxStart + posAcc->count);
                        for (cgltf_size i = 0; i < posAcc->count; ++i) {
                            bucket.indices[idxStart + i] = baseVertex + static_cast<std::uint32_t>(i);
                        }
                    }

                    if (!normAcc && bucket.indices.size() >= idxStart + 3) {
                        for (std::size_t i = idxStart; i + 2 < bucket.indices.size(); i += 3) {
                            auto& v0 = bucket.verts[bucket.indices[i]];
                            auto& v1 = bucket.verts[bucket.indices[i + 1]];
                            auto& v2 = bucket.verts[bucket.indices[i + 2]];
                            glm::vec3 n = glm::cross(
                                glm::vec3(v1.px - v0.px, v1.py - v0.py, v1.pz - v0.pz),
                                glm::vec3(v2.px - v0.px, v2.py - v0.py, v2.pz - v0.pz));
                            if (glm::length(n) > 0.0001f) {
                                n = glm::normalize(n);
                                v0.nx = v1.nx = v2.nx = n.x;
                                v0.ny = v1.ny = v2.ny = n.y;
                                v0.nz = v1.nz = v2.nz = n.z;
                            }
                        }
                    }
                }
            };

            bool processedAny = false;
            for (cgltf_size n = 0; n < data->nodes_count; ++n) {
                const cgltf_node* node = &data->nodes[n];
                if (!node->mesh) continue;
                float rawMat[16];
                cgltf_node_transform_world(node, rawMat);
                collectStaticMesh(node->mesh, glm::make_mat4(rawMat));
                processedAny = true;
            }
            if (!processedAny) {
                for (cgltf_size m = 0; m < data->meshes_count; ++m)
                    collectStaticMesh(&data->meshes[m], glm::mat4(1.0f));
            }

            for (auto& bucket : buckets) {
                if (bucket.verts.empty() || bucket.indices.empty()) continue;
                cpuGeom.emplace_back(bucket.verts, bucket.indices);
                GpuSubMesh sm;
                sm.upload(bucket.verts, bucket.indices);
                sm.attachInstanceBuffer(m_propInstVbo);
                if (bucket.mat) {
                    const cgltf_material* mat = bucket.mat;
                    if (mat->has_pbr_metallic_roughness) {
                        const auto& pbr = mat->pbr_metallic_roughness;
                        sm.baseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]};
                        sm.metallic = pbr.metallic_factor;
                        sm.roughness = pbr.roughness_factor;
                        if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                            sm.albedoTex = loadGltfImageTexture(pbr.base_color_texture.texture->image, modelDir);
                        }
                        if (pbr.metallic_roughness_texture.texture && pbr.metallic_roughness_texture.texture->image) {
                            sm.metRoughTex = loadGltfImageTexture(pbr.metallic_roughness_texture.texture->image, modelDir);
                        }
                    }
                    if (mat->normal_texture.texture && mat->normal_texture.texture->image) {
                        sm.normalTex = loadGltfImageTexture(mat->normal_texture.texture->image, modelDir);
                    }
                    sm.emissive = {mat->emissive_factor[0], mat->emissive_factor[1], mat->emissive_factor[2]};
                }
                gpuModel.subMeshes.push_back(sm);
            }
        }

        cgltf_free(data);
        if (gpuModel.subMeshes.empty()) { m_failedModels.insert(rawPath); return nullptr; }
        buildShadowMeshes(gpuModel, cpuGeom);
        if (hasBounds) {
            gpuModel.boundCenter = (bbMin + bbMax) * 0.5f;
            gpuModel.boundRadius = std::max(glm::length(bbMax - bbMin) * 0.5f, 0.5f);
            // Animated/skinned models can swing limbs or rotors slightly beyond rest AABB
            if (gpuModel.hasAnimations || gpuModel.hasSkins) {
                gpuModel.boundRadius = std::max(gpuModel.boundRadius * 1.45f, 2.2f);
            }
        }
        std::cout << "[SAGE][Model] Loaded \"" << rawPath << "\" (" << gpuModel.subMeshes.size() << " submesh(es)"
                  << (gpuModel.hasSkins ? (", " + std::to_string(gpuModel.skins.size()) + " skin(s)") : "")
                  << (gpuModel.hasAnimations ? (", " + std::to_string(gpuModel.animations.size()) + " anim clip(s)") : "") << ")\n";
        auto [it, inserted] = m_modelCache.emplace(rawPath, std::move(gpuModel));
        return &it->second;
    }

    void setLightTileCulling(bool enabled) override { m_lightTileCulling = enabled; }

    // -----------------------------------------------------------------------
    // Clustered light culling (exact)
    // -----------------------------------------------------------------------
    // The view is split into kLightTileSize-pixel screen tiles x kLightSlices depth slices
    // (exponentially spaced between the near and far planes). Each light's bit is set in
    // every cluster its sphere can touch:
    //  * screen extent: the 8 corners of the box around the sphere are projected and the
    //    rectangle is expanded by a pixel. Any point inside the sphere projects inside it;
    //    if a corner is behind the camera the light covers every tile instead.
    //  * depth extent: the sphere spans view depth [z - r, z + r]; one extra slice is added
    //    on each side so CPU/GPU floating-point differences can never drop a light.
    // The shader's falloff is exactly zero outside the sphere, so skipping a light in a
    // cluster it can't reach never changes a pixel.
    void buildLightTiles(const glm::mat4& viewProj, const glm::mat4& view, float nearZ, float farZ,
                         const glm::vec4* posRad, int count) {
        const int tilesX = (m_viewportWidth + kLightTileSize - 1) / kLightTileSize;
        const int tilesY = (m_viewportHeight + kLightTileSize - 1) / kLightTileSize;
        // Only ever touch our own texture unit, so other passes' bindings (shadow maps!) stay intact.
        gl::activeTexture(GL_TEXTURE0 + kLightTileTextureUnit);
        if (!m_lightTileTex || tilesX != m_lightTilesX || tilesY != m_lightTilesY) {
            if (!m_lightTileTex) glGenTextures(1, &m_lightTileTex);
            glBindTexture(GL_TEXTURE_2D, m_lightTileTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, tilesX, tilesY * kLightSlices, 0, GL_RED, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_lightTilesX = tilesX;
            m_lightTilesY = tilesY;
        }
        const std::size_t sliceCells = static_cast<std::size_t>(tilesX) * static_cast<std::size_t>(tilesY);
        m_lightClusterMasks.assign(sliceCells * kLightSlices, 0u);
        const float logScale = static_cast<float>(kLightSlices) / std::log(farZ / nearZ);
        const auto sliceOf = [&](float z) {
            return static_cast<int>(std::floor(std::log(std::max(z, nearZ) / nearZ) * logScale));
        };

        for (int li = 0; li < count && li < 16; ++li) {
            const glm::vec3 c(posRad[li]);
            const float r = std::max(posRad[li].w, 0.5f); // Same minimum radius as the shader

            // Depth extent in view space (view looks down -Z).
            const float viewZ = -(view * glm::vec4(c, 1.0f)).z;
            if (viewZ + r < nearZ * 0.5f || viewZ - r > farZ) {
                continue; // Entirely in front of the near plane or beyond the far plane
            }
            const int s0 = std::clamp(sliceOf(viewZ - r) - 1, 0, kLightSlices - 1);
            const int s1 = std::clamp(sliceOf(viewZ + r) + 1, 0, kLightSlices - 1);

            // Screen extent.
            int x0 = 0, y0 = 0, x1 = tilesX - 1, y1 = tilesY - 1;
            bool everywhere = false;
            float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
            for (int k = 0; k < 8; ++k) {
                const glm::vec3 corner = c + glm::vec3((k & 1) ? r : -r, (k & 2) ? r : -r, (k & 4) ? r : -r);
                const glm::vec4 clip = viewProj * glm::vec4(corner, 1.0f);
                if (clip.w <= 1e-4f) {
                    everywhere = true; // Box crosses the camera plane: be conservative
                    break;
                }
                const float sx = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(m_viewportWidth);
                const float sy = (clip.y / clip.w * 0.5f + 0.5f) * static_cast<float>(m_viewportHeight);
                minX = std::min(minX, sx); maxX = std::max(maxX, sx);
                minY = std::min(minY, sy); maxY = std::max(maxY, sy);
            }
            if (!everywhere) {
                if (maxX < -1.0f || maxY < -1.0f ||
                    minX > static_cast<float>(m_viewportWidth) + 1.0f || minY > static_cast<float>(m_viewportHeight) + 1.0f) {
                    continue; // Entirely off screen: affects no visible pixel
                }
                x0 = std::clamp(static_cast<int>(std::floor((minX - 1.0f) / kLightTileSize)), 0, tilesX - 1);
                x1 = std::clamp(static_cast<int>(std::floor((maxX + 1.0f) / kLightTileSize)), 0, tilesX - 1);
                y0 = std::clamp(static_cast<int>(std::floor((minY - 1.0f) / kLightTileSize)), 0, tilesY - 1);
                y1 = std::clamp(static_cast<int>(std::floor((maxY + 1.0f) / kLightTileSize)), 0, tilesY - 1);
            }

            const std::uint32_t bit = 1u << li;
            for (int sl = s0; sl <= s1; ++sl) {
                std::uint32_t* sliceBase = &m_lightClusterMasks[static_cast<std::size_t>(sl) * sliceCells];
                for (int ty = y0; ty <= y1; ++ty) {
                    std::uint32_t* row = sliceBase + static_cast<std::size_t>(ty) * static_cast<std::size_t>(tilesX);
                    for (int tx = x0; tx <= x1; ++tx) row[tx] |= bit;
                }
            }
        }

        m_lightTileData.resize(m_lightClusterMasks.size());
        for (std::size_t i = 0; i < m_lightClusterMasks.size(); ++i) {
            m_lightTileData[i] = static_cast<float>(m_lightClusterMasks[i]); // <= 0xFFFF: exact in a float
        }
        glBindTexture(GL_TEXTURE_2D, m_lightTileTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tilesX, tilesY * kLightSlices, GL_RED, GL_FLOAT, m_lightTileData.data());
        gl::activeTexture(GL_TEXTURE0);
    }

    // Calls fn for the mesh(es) to draw into a shadow map: the merged whole-model mesh when
    // available, otherwise each sub-mesh.
    template <typename Fn>
    static void forEachShadowMesh(const GpuModel& model, Fn&& fn) {
        if (model.shadowWholeMesh.indexCount > 0) {
            fn(model.shadowWholeMesh);
            return;
        }
        for (const GpuSubMesh& sm : model.subMeshes) fn(sm);
    }

    // Concatenates sub-mesh geometry into single depth-only meshes for the shadow pass.
    // Positions/skin attributes are kept; materials are irrelevant to depth rendering.
    void buildShadowMeshes(GpuModel& model,
                           const std::vector<std::pair<std::vector<Vertex3D>, std::vector<std::uint32_t>>>& geom) {
        if (geom.size() != model.subMeshes.size()) {
            return; // Defensive: alignment broken, keep per-sub-mesh shadow drawing.
        }
        const auto mergeInto = [&geom, this](GpuSubMesh& out, const std::vector<std::size_t>& subMeshIdx) {
            std::vector<Vertex3D> verts;
            std::vector<std::uint32_t> indices;
            for (const std::size_t i : subMeshIdx) {
                const auto base = static_cast<std::uint32_t>(verts.size());
                verts.insert(verts.end(), geom[i].first.begin(), geom[i].first.end());
                for (const std::uint32_t idx : geom[i].second) {
                    indices.push_back(base + idx);
                }
            }
            if (!indices.empty()) {
                out.upload(verts, indices);
                out.attachInstanceBuffer(m_propInstVbo);
                m_shadowMergedBytes += verts.size() * sizeof(Vertex3D) + indices.size() * sizeof(std::uint32_t);
            }
        };
        if (model.hasAnimations || model.hasSkins) {
            model.shadowNodeMeshes.resize(model.nodes.size());
            for (std::size_t n = 0; n < model.nodes.size(); ++n) {
                if (model.nodes[n].subMeshIndices.size() >= 2) {
                    mergeInto(model.shadowNodeMeshes[n], model.nodes[n].subMeshIndices);
                }
            }
        } else if (model.subMeshes.size() >= 2) {
            std::vector<std::size_t> all(model.subMeshes.size());
            for (std::size_t i = 0; i < all.size(); ++i) all[i] = i;
            mergeInto(model.shadowWholeMesh, all);
        }
    }

    std::size_t m_shadowMergedBytes = 0; // Extra GPU memory used by merged shadow meshes

    RendererInfo m_info;
    std::filesystem::path m_assetRoot;
    ClearColor m_clearColor{};

    GLuint m_fullscreenVao = 0;
    GLuint m_propInstVbo = 0;
    GLuint m_shadowProgram = 0, m_shadowFbo = 0, m_shadowTex = 0, m_shadowFboMid = 0, m_shadowTexMid = 0, m_shadowFboFar = 0, m_shadowTexFar = 0;
    GLint m_shadowModel = -1, m_shadowLightVP = -1, m_shadowHasSkin = -1, m_shadowUseInstancing = -1, m_shadowBoneMatrices = -1;

    GLuint m_hdrFbo = 0, m_hdrColorTex = 0, m_hdrDepthTex = 0, m_waterDepthCopyTex = 0;
    GLuint m_bloomFbo[2]{0, 0}, m_bloomTex[2]{0, 0};
    int m_bloomWidth = 640, m_bloomHeight = 360;

    GLuint m_skyProgram = 0;
    GLint m_skyInvViewProj = -1, m_skyCameraPos = -1, m_skySunDir = -1, m_skySunColor = -1;
    GLint m_skyZenith = -1, m_skyHorizon = -1, m_skyGround = -1, m_skyTime = -1;

    GLuint m_litProgram = 0;
    GLint m_uModel = -1, m_uViewProj = -1, m_uHasSkin = -1, m_uUseInstancing = -1, m_uBoneMatrices = -1, m_uBaseColor = -1, m_uEmissive = -1;
    GLint m_uMetallic = -1, m_uRoughness = -1, m_uHasTexture = -1, m_uAlbedoTex = -1;
    GLint m_uHasNormalTex = -1, m_uNormalTex = -1, m_uHasMetRoughTex = -1, m_uMetRoughTex = -1;
    GLint m_uTLayers = -1, m_uTLayerCount = -1, m_uTCliffColor = -1, m_uTSlopeSnow = -1, m_uTSnowColor = -1;
    GLint m_uPulse = -1, m_uTime = -1, m_uIsGround = -1, m_uTerrainMode = -1, m_uWetness = -1, m_uCloudShadowsEnabled = -1;
    GLint m_uShadowsEnabled = -1, m_uLightViewProj = -1, m_uLightViewProjMid = -1, m_uLightViewProjFar = -1;
    GLint m_uShadowMap = -1, m_uShadowMapMid = -1, m_uShadowMapFar = -1, m_uShadowTexelSize = -1;
    GLint m_uCameraPos = -1, m_uSunDir = -1, m_uSunColor = -1;
    GLint m_uSkyZenith = -1, m_uSkyHorizon = -1, m_uGroundAmbient = -1, m_uAmbientIntensity = -1;
    GLint m_uFogStart = -1, m_uFogEnd = -1;
    GLint m_uPointLightCount = -1, m_uPointLightPosRadius = -1, m_uPointLightColorInt = -1;
    GLint m_uLightTileMask = -1, m_uLightTilesEnabled = -1, m_uLightTileSize = -1;
    GLint m_uLightTilesY = -1, m_uLightSlices = -1, m_uCameraForward = -1, m_uClusterNear = -1, m_uClusterLogScale = -1;
    // Clustered light culling state
    static constexpr int kLightTileSize = 32;
    static constexpr int kLightSlices = 16;
    static constexpr int kLightTileTextureUnit = 8;
    bool m_lightTileCulling = true;
    GLuint m_lightTileTex = 0;
    int m_lightTilesX = 0, m_lightTilesY = 0;
    std::vector<float> m_lightTileData;
    std::vector<std::uint32_t> m_lightClusterMasks;

    GLuint m_grassProgram = 0;
    GLint m_grassViewProj = -1, m_grassCamPos = -1, m_grassInteractPos = -1, m_grassTime = -1;
    GLint m_grassSunDir = -1, m_grassSunColor = -1, m_grassSkyZenith = -1, m_grassSkyHorizon = -1;
    GLint m_grassGroundAmbient = -1, m_grassAmbientInt = -1, m_grassFogStart = -1, m_grassFogEnd = -1, m_grassCloudShadows = -1;
    GLint m_grassShadowsEnabled = -1, m_grassLightVP = -1, m_grassLightVPMid = -1, m_grassShadowMap = -1, m_grassShadowMapMid = -1;
    GLuint m_grassVao = 0, m_grassBladeVbo = 0, m_grassInstVbo = 0;
    std::size_t m_grassCount = 0;
    bool m_grassBuilt = false;
    int m_grassModeCached = -1;

    GLuint m_waterProgram = 0;
    GLint m_waterModel = -1, m_waterViewProj = -1, m_waterTime = -1, m_waterShallow = -1;
    GLint m_waterCamPos = -1, m_waterSunDir = -1, m_waterSunColor = -1;
    GLint m_waterSkyZenith = -1, m_waterSkyHorizon = -1, m_waterFogStart = -1, m_waterFogEnd = -1;
    GLint m_waterPointLightCount = -1, m_waterPointLightPosRadius = -1, m_waterPointLightColorInt = -1;
    GLint m_waterSceneDepthTex = -1, m_waterNearPlane = -1, m_waterFarPlane = -1;

    GLuint m_bloomExtractProgram = 0, m_bloomBlurProgram = 0, m_compositeProgram = 0;
    GLint m_bloomExtractTex = -1, m_bloomBlurImage = -1, m_bloomBlurDir = -1;
    GLint m_compHdrTex = -1, m_compBloomTex = -1, m_compDepthTex = -1, m_compBloomEnabled = -1, m_compBloomIntensity = -1;
    GLint m_compSsaoEnabled = -1, m_compFxaaEnabled = -1, m_compGodRaysEnabled = -1, m_compGodRayIntensity = -1;
    GLint m_compSunScreenPos = -1, m_compSunColor = -1;
    GLint m_compSsrEnabled = -1, m_compSsrIntensity = -1, m_compViewProj = -1, m_compInvViewProj = -1, m_compCameraPos = -1;
    GLint m_compExposure = -1, m_compTexelSize = -1, m_compNearPlane = -1, m_compFarPlane = -1;
    std::size_t m_lastCulledCount = 0;
    std::size_t m_lastDrawCalls = 0;
    std::size_t m_lastInstancedBatches = 0;
    std::size_t m_lastInstancedProps = 0;
    std::size_t m_lastSavedDrawCalls = 0;
    bool m_pipelineWarmed = false;

    std::uint64_t m_frameCounter = 0;
    bool m_farShadowValid = false;
    glm::mat4 m_cachedFarLvp{1.0f};
    glm::vec3 m_lastFarSunDir{0.0f, 1.0f, 0.0f};
    glm::vec3 m_lastFarFocus{0.0f};

    struct CachedPose {
        std::uint64_t frameId = 0;
        std::vector<glm::mat4> worldMats;
    };
    AnimEvalScratch m_animScratch;
    std::unordered_map<const Entity*, CachedPose> m_framePoseCache;

    GpuSubMesh m_boxMesh, m_planeMesh, m_waterMesh, m_sphereMesh, m_arrowMesh, m_ringMesh, m_terrainMesh;
    bool m_terrainMeshBuilt = false;
    GpuPassProfiler m_gpuProfiler;
    ShaderLibrary m_shaders;
    std::chrono::steady_clock::time_point m_lastShaderPoll{};
    const World* m_frameWorld = nullptr; // World being rendered (valid during renderWorld)
    std::vector<std::pair<float, const Entity*>> m_opaqueOrderScratch; // Reused every frame (no allocations)
    std::vector<const Entity*> m_opaqueGroundScratch;
    std::vector<const Entity*> m_opaqueDrawList;
    int m_terrainModeCached = -1;
    std::uint32_t m_terrainRevisionCached = 0;
    std::uint32_t m_grassRevisionCached = 0;
    float m_terrainHalfSizeCached = 0.0f;
    float m_terrainMaxHeightCached = 0.0f;
    int m_terrainGridCached = 0;
    std::unordered_map<std::string, GpuModel> m_modelCache;
    std::unordered_set<std::string> m_failedModels;
    std::unordered_map<const GpuModel*, std::vector<PropInstanceData>> m_instBatchesScratch;
    std::vector<PropInstanceData> m_rainInstScratch;
    std::vector<glm::mat4> m_bonePaletteScratch;
    std::vector<const Entity*> m_waterEntitiesScratch;

    int m_viewportWidth = 1280, m_viewportHeight = 720;
    bool m_initialized = false;
};

} // namespace

std::unique_ptr<Renderer> createOpenGLRenderer() {
    return std::make_unique<OpenGLRenderer>();
}

} // namespace sage
