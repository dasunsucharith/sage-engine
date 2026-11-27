#include "sage/renderer/renderer.h"

#include "renderer/gl_functions.h"
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
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
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

constexpr float kPi = 3.14159265358979323846f;

// -----------------------------------------------------------------------------
// Sky Gradient + Solar Disk Fullscreen Pass
// -----------------------------------------------------------------------------
constexpr const char* kSkyVertexShader = R"GLSL(
#version 330 core
out vec2 vNdc;

void main() {
    // Fullscreen triangle covering [-1, 1] without any VBO
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vNdc = positions[gl_VertexID];
    gl_Position = vec4(vNdc, 0.9999, 1.0);
}
)GLSL";

constexpr const char* kSkyFragmentShader = R"GLSL(
#version 330 core
in vec2 vNdc;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uGroundAmbient;

out vec4 FragColor;

vec3 acesToneMap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 farWorld = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 rayDir = normalize(farWorld.xyz / farWorld.w - uCameraPos);

    float h = clamp(rayDir.y, 0.0, 1.0);
    vec3 sky = mix(uSkyHorizon, uSkyZenith, pow(h, 0.55));
    if (rayDir.y < 0.0) {
        sky = mix(uSkyHorizon, uGroundAmbient, clamp(-rayDir.y * 4.0, 0.0, 1.0));
    }

    // Soft solar halo + crisp sun disk
    float sunDot = max(dot(rayDir, normalize(uSunDir)), 0.0);
    sky += uSunColor * pow(sunDot, 24.0) * 0.45;
    sky += uSunColor * smoothstep(0.9985, 0.9996, sunDot) * 2.5;

    vec3 mapped = pow(acesToneMap(sky), vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
)GLSL";

// -----------------------------------------------------------------------------
// 3D Lit Mesh Shader (Directional Sun + Hemisphere Sky + Specular + Fog + ACES)
// -----------------------------------------------------------------------------
constexpr const char* kLitVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vWorldNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUV = aUV;
    gl_Position = uViewProj * worldPos;
}
)GLSL";

constexpr const char* kLitFragmentShader = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;

uniform vec3 uBaseColor;
uniform vec3 uEmissive;
uniform float uMetallic;
uniform float uRoughness;
uniform int uHasTexture;
uniform sampler2D uAlbedoTex;
uniform float uPulse;
uniform float uTime;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uGroundAmbient;
uniform float uAmbientIntensity;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 FragColor;

vec3 acesToneMap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 albedo = uBaseColor;
    if (uHasTexture == 1) {
        vec4 texel = texture(uAlbedoTex, vUV);
        albedo *= pow(texel.rgb, vec3(2.2));
    }

    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(uSunDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    // Directional sunlight + subtle wrap diffuse for crisp 3D readability
    float ndotl = max(dot(N, L), 0.0);
    float wrapDiffuse = max((dot(N, L) + 0.18) / 1.18, 0.0);
    vec3 directDiffuse = albedo * (1.0 - 0.5 * uMetallic) * uSunColor * mix(ndotl, wrapDiffuse, 0.25) * 1.35;

    // Hemisphere ambient (sky overhead + warm/cool ground bounce underneath)
    float hemiT = N.y * 0.5 + 0.5;
    vec3 skyFill = mix(uSkyHorizon, uSkyZenith, 0.35);
    vec3 hemiAmbient = mix(uGroundAmbient, skyFill, hemiT) * albedo * (uAmbientIntensity * 1.35);

    // Specular highlight (Blinn-Phong scaled by roughness & metallic)
    float rough = clamp(uRoughness, 0.08, 1.0);
    float shininess = mix(96.0, 8.0, rough);
    float specStrength = mix(0.22, 0.85, uMetallic) * (1.0 - rough * 0.65);
    vec3 specTint = mix(vec3(1.0), albedo, uMetallic);
    vec3 specular = specTint * uSunColor * pow(max(dot(N, H), 0.0), shininess) * specStrength * step(0.001, ndotl);

    // Subtle Fresnel sky reflection rim on glossy/metallic surfaces (cars!)
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0) * (1.0 - rough) * 0.28;
    vec3 rim = skyFill * fresnel;

    vec3 emissiveTerm = uEmissive;
    if (uPulse > 0.5) {
        float p = 0.80 + 0.20 * sin(uTime * 5.0);
        emissiveTerm *= p * 1.25;
    }

    vec3 color = hemiAmbient + directDiffuse + specular + rim + emissiveTerm;

    // Atmospheric distance fog blending smoothly toward horizon
    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    color = mix(color, uSkyHorizon, fogFactor * fogFactor * 0.85);

    vec3 mapped = pow(acesToneMap(color), vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
)GLSL";

struct Vertex3D {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct GpuSubMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    GLuint albedoTex = 0;
    glm::vec3 baseColor{1.0f, 1.0f, 1.0f};
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    float metallic = 0.1f;
    float roughness = 0.6f;

    void upload(const std::vector<Vertex3D>& vertices, const std::vector<std::uint32_t>& indices) {
        indexCount = static_cast<GLsizei>(indices.size());
        gl::genVertexArrays(1, &vao);
        gl::genBuffers(1, &vbo);
        gl::genBuffers(1, &ebo);

        gl::bindVertexArray(vao);

        gl::bindBuffer(GL_ARRAY_BUFFER, vbo);
        gl::bufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex3D)),
            vertices.data(),
            GL_STATIC_DRAW);

        gl::bindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        gl::bufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
            indices.data(),
            GL_STATIC_DRAW);

        gl::vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, px)));
        gl::enableVertexAttribArray(0);

        gl::vertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, nx)));
        gl::enableVertexAttribArray(1);

        gl::vertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, u)));
        gl::enableVertexAttribArray(2);

        gl::bindVertexArray(0);
    }

    void destroy() {
        if (albedoTex) {
            glDeleteTextures(1, &albedoTex);
            albedoTex = 0;
        }
        if (ebo) {
            gl::deleteBuffers(1, &ebo);
            ebo = 0;
        }
        if (vbo) {
            gl::deleteBuffers(1, &vbo);
            vbo = 0;
        }
        if (vao) {
            gl::deleteVertexArrays(1, &vao);
            vao = 0;
        }
        indexCount = 0;
    }
};

struct GpuModel {
    std::vector<GpuSubMesh> subMeshes;

    void destroy() {
        for (auto& sm : subMeshes) {
            sm.destroy();
        }
        subMeshes.clear();
    }
};

std::string safeGLString(GLenum name) {
    const GLubyte* raw = glGetString(name);
    return raw ? reinterpret_cast<const char*>(raw) : "Unknown";
}

GLuint compileShader(GLenum type, const char* source, const char* label) {
    const GLuint shader = gl::createShader(type);
    gl::shaderSource(shader, 1, &source, nullptr);
    gl::compileShader(shader);

    GLint success = 0;
    gl::getShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        gl::getShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(static_cast<std::size_t>(logLength > 1 ? logLength : 1));
        gl::getShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "[SAGE][OpenGL] Shader compilation failed (" << label << "):\n"
                  << infoLog.data() << "\n";
        gl::deleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vertSrc, const char* fragSrc, const char* label) {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc, label);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc, label);
    if (!vs || !fs) {
        if (vs) gl::deleteShader(vs);
        if (fs) gl::deleteShader(fs);
        return 0;
    }

    const GLuint program = gl::createProgram();
    gl::attachShader(program, vs);
    gl::attachShader(program, fs);
    gl::linkProgram(program);
    gl::deleteShader(vs);
    gl::deleteShader(fs);

    GLint linked = 0;
    gl::getProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength = 0;
        gl::getProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(static_cast<std::size_t>(logLength > 1 ? logLength : 1));
        gl::getProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "[SAGE][OpenGL] Shader program link failed (" << label << "):\n"
                  << infoLog.data() << "\n";
        gl::deleteProgram(program);
        return 0;
    }
    return program;
}

GLuint createTextureFromPixels(const unsigned char* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) {
        return 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    gl::generateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint loadGltfImageTexture(const cgltf_image* image, const std::filesystem::path& modelDir) {
    if (!image) {
        return 0;
    }

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = nullptr;

    if (image->buffer_view && image->buffer_view->buffer && image->buffer_view->buffer->data) {
        const auto* base = static_cast<const unsigned char*>(image->buffer_view->buffer->data);
        const unsigned char* dataPtr = base + image->buffer_view->offset;
        const int dataSize = static_cast<int>(image->buffer_view->size);
        pixels = stbi_load_from_memory(dataPtr, dataSize, &width, &height, &channels, 4);
    } else if (image->uri && image->uri[0] != '\0') {
        const std::filesystem::path imgPath = modelDir / image->uri;
        pixels = stbi_load(imgPath.string().c_str(), &width, &height, &channels, 4);
    }

    if (!pixels) {
        return 0;
    }

    const GLuint tex = createTextureFromPixels(pixels, width, height);
    stbi_image_free(pixels);
    return tex;
}

GpuSubMesh buildUnitCubeMesh() {
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;

    struct FaceDef {
        glm::vec3 n;
        glm::vec3 corners[4];
    };
    const FaceDef faces[6] = {
        {{ 1, 0, 0}, {{ 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}}},
        {{-1, 0, 0}, {{-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}}},
        {{ 0, 1, 0}, {{-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}}},
        {{ 0,-1, 0}, {{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}}},
        {{ 0, 0, 1}, {{-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}}},
        {{ 0, 0,-1}, {{ 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}}},
    };
    constexpr float uvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    for (const auto& f : faces) {
        const auto base = static_cast<std::uint32_t>(verts.size());
        for (int i = 0; i < 4; ++i) {
            verts.push_back({f.corners[i].x, f.corners[i].y, f.corners[i].z, f.n.x, f.n.y, f.n.z, uvs[i][0], uvs[i][1]});
        }
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }

    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

GpuSubMesh buildUnitPlaneMesh() {
    const std::vector<Vertex3D> verts = {
        {-0.5f, 0.0f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f},
        { 0.5f, 0.0f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f},
        { 0.5f, 0.0f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f},
        {-0.5f, 0.0f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f},
    };
    const std::vector<std::uint32_t> idx = {0, 1, 2, 0, 2, 3};

    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

GpuSubMesh buildUnitArrowMesh() {
    // 3D extruded pointer arrow facing +Z
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;

    const glm::vec3 tipT(0.0f, 0.2f, 0.55f), leftT(-0.45f, 0.2f, -0.45f), rightT(0.45f, 0.2f, -0.45f);
    const glm::vec3 tipB(0.0f, -0.2f, 0.55f), leftB(-0.45f, -0.2f, -0.45f), rightB(0.45f, -0.2f, -0.45f);

    auto addTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        const auto base = static_cast<std::uint32_t>(verts.size());
        verts.push_back({a.x, a.y, a.z, n.x, n.y, n.z, 0.5f, 1.0f});
        verts.push_back({b.x, b.y, b.z, n.x, n.y, n.z, 0.0f, 0.0f});
        verts.push_back({c.x, c.y, c.z, n.x, n.y, n.z, 1.0f, 0.0f});
        idx.insert(idx.end(), {base, base + 1, base + 2});
    };
    auto addQuad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        addTri(a, b, c);
        addTri(a, c, d);
    };

    addTri(tipT, rightT, leftT);
    addTri(tipB, leftB, rightB);
    addQuad(leftB, tipB, tipT, leftT);
    addQuad(tipB, rightB, rightT, tipT);
    addQuad(rightB, leftB, leftT, rightT);

    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

GpuSubMesh buildUnitRingMesh() {
    // 3D hollow beacon cylinder/ring (radius 0.5, vertical height 1.0)
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;

    constexpr int kSegments = 32;
    constexpr float kOuterR = 0.50f;
    constexpr float kInnerR = 0.42f;
    constexpr float kY0 = 0.0f;
    constexpr float kY1 = 1.0f;

    auto addQuad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
        const auto base = static_cast<std::uint32_t>(verts.size());
        verts.push_back({a.x, a.y, a.z, n.x, n.y, n.z, 0.0f, 0.0f});
        verts.push_back({b.x, b.y, b.z, n.x, n.y, n.z, 1.0f, 0.0f});
        verts.push_back({c.x, c.y, c.z, n.x, n.y, n.z, 1.0f, 1.0f});
        verts.push_back({d.x, d.y, d.z, n.x, n.y, n.z, 0.0f, 1.0f});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };

    for (int i = 0; i < kSegments; ++i) {
        const float a0 = (static_cast<float>(i) / kSegments) * 2.0f * kPi;
        const float a1 = (static_cast<float>(i + 1) / kSegments) * 2.0f * kPi;
        const float c0 = std::cos(a0), s0 = std::sin(a0);
        const float c1 = std::cos(a1), s1 = std::sin(a1);
        const glm::vec3 nOut = glm::normalize(glm::vec3(c0 + c1, 0.0f, s0 + s1));

        const glm::vec3 o0b(c0 * kOuterR, kY0, s0 * kOuterR);
        const glm::vec3 o1b(c1 * kOuterR, kY0, s1 * kOuterR);
        const glm::vec3 o1t(c1 * kOuterR, kY1, s1 * kOuterR);
        const glm::vec3 o0t(c0 * kOuterR, kY1, s0 * kOuterR);

        const glm::vec3 i0b(c0 * kInnerR, kY0, s0 * kInnerR);
        const glm::vec3 i1b(c1 * kInnerR, kY0, s1 * kInnerR);
        const glm::vec3 i1t(c1 * kInnerR, kY1, s1 * kInnerR);
        const glm::vec3 i0t(c0 * kInnerR, kY1, s0 * kInnerR);

        addQuad(o0b, o1b, o1t, o0t, nOut);
        addQuad(i1b, i0b, i0t, i1t, -nOut);
        addQuad(o0t, o1t, i1t, i0t, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

class OpenGLRenderer final : public Renderer {
public:
    ~OpenGLRenderer() override {
        shutdown();
    }

    bool initialize(int viewportWidth, int viewportHeight) override {
        if (!gl::loadFunctions()) {
            return false;
        }

        m_info.backendName = "OpenGL 3.3 Core (3D Lit + glTF 2.0)";
        m_info.gpuVendor = safeGLString(GL_VENDOR);
        m_info.gpuRenderer = safeGLString(GL_RENDERER);
        m_info.apiVersion = safeGLString(GL_VERSION);

        m_skyProgram = linkProgram(kSkyVertexShader, kSkyFragmentShader, "SkyPass");
        m_litProgram = linkProgram(kLitVertexShader, kLitFragmentShader, "Lit3DPass");
        if (!m_skyProgram || !m_litProgram) {
            shutdown();
            return false;
        }

        gl::genVertexArrays(1, &m_skyVao);

        // Cache sky uniform locations
        m_skyInvViewProj = gl::getUniformLocation(m_skyProgram, "uInvViewProj");
        m_skyCameraPos = gl::getUniformLocation(m_skyProgram, "uCameraPos");
        m_skySunDir = gl::getUniformLocation(m_skyProgram, "uSunDir");
        m_skySunColor = gl::getUniformLocation(m_skyProgram, "uSunColor");
        m_skyZenith = gl::getUniformLocation(m_skyProgram, "uSkyZenith");
        m_skyHorizon = gl::getUniformLocation(m_skyProgram, "uSkyHorizon");
        m_skyGround = gl::getUniformLocation(m_skyProgram, "uGroundAmbient");

        // Cache 3D lit uniform locations
        m_uModel = gl::getUniformLocation(m_litProgram, "uModel");
        m_uViewProj = gl::getUniformLocation(m_litProgram, "uViewProj");
        m_uBaseColor = gl::getUniformLocation(m_litProgram, "uBaseColor");
        m_uEmissive = gl::getUniformLocation(m_litProgram, "uEmissive");
        m_uMetallic = gl::getUniformLocation(m_litProgram, "uMetallic");
        m_uRoughness = gl::getUniformLocation(m_litProgram, "uRoughness");
        m_uHasTexture = gl::getUniformLocation(m_litProgram, "uHasTexture");
        m_uAlbedoTex = gl::getUniformLocation(m_litProgram, "uAlbedoTex");
        m_uPulse = gl::getUniformLocation(m_litProgram, "uPulse");
        m_uTime = gl::getUniformLocation(m_litProgram, "uTime");
        m_uCameraPos = gl::getUniformLocation(m_litProgram, "uCameraPos");
        m_uSunDir = gl::getUniformLocation(m_litProgram, "uSunDir");
        m_uSunColor = gl::getUniformLocation(m_litProgram, "uSunColor");
        m_uSkyZenith = gl::getUniformLocation(m_litProgram, "uSkyZenith");
        m_uSkyHorizon = gl::getUniformLocation(m_litProgram, "uSkyHorizon");
        m_uGroundAmbient = gl::getUniformLocation(m_litProgram, "uGroundAmbient");
        m_uAmbientIntensity = gl::getUniformLocation(m_litProgram, "uAmbientIntensity");
        m_uFogStart = gl::getUniformLocation(m_litProgram, "uFogStart");
        m_uFogEnd = gl::getUniformLocation(m_litProgram, "uFogEnd");

        m_boxMesh = buildUnitCubeMesh();
        m_planeMesh = buildUnitPlaneMesh();
        m_arrowMesh = buildUnitArrowMesh();
        m_ringMesh = buildUnitRingMesh();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        onResize(viewportWidth, viewportHeight);
        m_initialized = true;
        return true;
    }

    void shutdown() override {
        for (auto& [path, model] : m_modelCache) {
            model.destroy();
        }
        m_modelCache.clear();
        m_failedModels.clear();

        m_boxMesh.destroy();
        m_planeMesh.destroy();
        m_arrowMesh.destroy();
        m_ringMesh.destroy();

        if (m_skyVao) {
            gl::deleteVertexArrays(1, &m_skyVao);
            m_skyVao = 0;
        }
        if (m_skyProgram) {
            gl::deleteProgram(m_skyProgram);
            m_skyProgram = 0;
        }
        if (m_litProgram) {
            gl::deleteProgram(m_litProgram);
            m_litProgram = 0;
        }
        m_initialized = false;
    }

    void setAssetRoot(const std::filesystem::path& assetRoot) override {
        m_assetRoot = assetRoot;
    }

    void onResize(int width, int height) override {
        m_viewportWidth = width > 0 ? width : 1;
        m_viewportHeight = height > 0 ? height : 1;
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    }

    void beginFrame(const ClearColor& clearColor) override {
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void renderWorld(const World& world, float timeSeconds) override {
        if (!m_initialized) {
            return;
        }

        const CameraState& cam = world.camera();
        const SkySettings& sky = world.sky();
        const float aspect = static_cast<float>(m_viewportWidth) /
                             static_cast<float>(m_viewportHeight);

        const glm::vec3 camPos(cam.position.x, cam.position.y, cam.position.z);
        glm::vec3 camTarget(cam.target.x, cam.target.y, cam.target.z);
        if (glm::length(camTarget - camPos) < 0.001f) {
            camTarget = camPos + glm::vec3(0.0f, 0.0f, 1.0f);
        }

        const glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 proj = glm::perspective(
            glm::radians(std::clamp(cam.fovDegrees, 20.0f, 120.0f)),
            aspect,
            cam.nearPlane,
            cam.farPlane);
        const glm::mat4 viewProj = proj * view;
        const glm::mat4 invViewProj = glm::inverse(viewProj);

        // --- Pass 1: Procedural Sky Gradient & Sun Disk ---
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
        gl::bindVertexArray(m_skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        gl::bindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);

        // --- Pass 2: 3D Lit Entities & glTF 2.0 (.glb) Models ---
        gl::useProgram(m_litProgram);
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
        gl::uniform1i(m_uAlbedoTex, 0);

        for (const Entity& e : world.entities()) {
            if (!e.visible) {
                continue;
            }

            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(e.position.x, e.position.y, e.position.z));
            if (e.useQuaternion) {
                const glm::quat q(e.rotationQuat.w, e.rotationQuat.x, e.rotationQuat.y, e.rotationQuat.z);
                model *= glm::mat4_cast(glm::normalize(q));
            } else {
                model = glm::rotate(model, glm::radians(e.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(e.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(e.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            }
            model = glm::scale(model, glm::vec3(e.scale.x, e.scale.y, e.scale.z));
            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));

            if (e.shape == EntityShape::Model) {
                const GpuModel* gpuModel = getOrLoadModel(e.modelPath);
                if (!gpuModel) {
                    continue;
                }
                for (const GpuSubMesh& sm : gpuModel->subMeshes) {
                    const glm::vec3 tinted = sm.baseColor * glm::vec3(e.color.r, e.color.g, e.color.b);
                    const glm::vec3 emissive = e.emissive ? tinted * 0.9f : sm.emissive;
                    drawSubMesh(sm, tinted, emissive, sm.metallic, sm.roughness, 0.0f);
                }
                continue;
            }

            const GpuSubMesh* prim = &m_boxMesh;
            float pulse = 0.0f;
            float roughness = 0.65f;
            if (e.shape == EntityShape::Plane) {
                prim = &m_planeMesh;
                roughness = 0.88f;
            } else if (e.shape == EntityShape::Triangle) {
                prim = &m_arrowMesh;
                roughness = 0.35f;
            } else if (e.shape == EntityShape::Ring) {
                prim = &m_ringMesh;
                pulse = 1.0f;
                roughness = 0.25f;
            }

            const glm::vec3 baseCol(e.color.r, e.color.g, e.color.b);
            const glm::vec3 emissiveCol = (e.emissive || e.shape == EntityShape::Ring)
                ? baseCol * 1.1f
                : glm::vec3(0.0f);
            drawSubMesh(*prim, baseCol, emissiveCol, 0.08f, roughness, pulse);
        }

        gl::bindVertexArray(0);
    }

    void endFrame() override {}

    RendererInfo info() const override {
        return m_info;
    }

    std::size_t loadedModelCount() const override {
        return m_modelCache.size();
    }

private:
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

        gl::bindVertexArray(sm.vao);
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr);
    }

    std::filesystem::path resolveModelPath(const std::string& rawPath) const {
        const std::filesystem::path p(rawPath);
        if (std::filesystem::exists(p)) {
            return p;
        }
        if (!m_assetRoot.empty()) {
            const std::filesystem::path fromRoot = m_assetRoot / p;
            if (std::filesystem::exists(fromRoot)) {
                return fromRoot;
            }
        }
        const std::filesystem::path parentCandidate = std::filesystem::path("..") / p;
        if (std::filesystem::exists(parentCandidate)) {
            return parentCandidate;
        }
        return p;
    }

    const GpuModel* getOrLoadModel(const std::string& rawPath) {
        if (const auto it = m_modelCache.find(rawPath); it != m_modelCache.end()) {
            return &it->second;
        }
        if (m_failedModels.contains(rawPath)) {
            return nullptr;
        }

        const std::filesystem::path fullPath = resolveModelPath(rawPath);
        if (!std::filesystem::exists(fullPath)) {
            std::cerr << "[SAGE][Model] glTF/.glb file not found: \"" << rawPath
                      << "\" (resolved: " << fullPath.string() << ")\n";
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

        auto processMesh = [&](const cgltf_mesh* mesh, const glm::mat4& worldTransform) {
            const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

            for (cgltf_size pIdx = 0; pIdx < mesh->primitives_count; ++pIdx) {
                const cgltf_primitive& prim = mesh->primitives[pIdx];
                if (prim.type != cgltf_primitive_type_triangles) {
                    continue;
                }

                const cgltf_accessor* posAcc = nullptr;
                const cgltf_accessor* normAcc = nullptr;
                const cgltf_accessor* uvAcc = nullptr;

                for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                    const cgltf_attribute& attr = prim.attributes[a];
                    if (attr.type == cgltf_attribute_type_position) posAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_normal) normAcc = attr.data;
                    else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uvAcc = attr.data;
                }

                if (!posAcc || posAcc->count == 0) {
                    continue;
                }

                std::vector<Vertex3D> vertices(posAcc->count);
                for (cgltf_size v = 0; v < posAcc->count; ++v) {
                    float pos[3] = {0, 0, 0};
                    float norm[3] = {0, 1, 0};
                    float uv[2] = {0, 0};

                    cgltf_accessor_read_float(posAcc, v, pos, 3);
                    if (normAcc) cgltf_accessor_read_float(normAcc, v, norm, 3);
                    if (uvAcc) cgltf_accessor_read_float(uvAcc, v, uv, 2);

                    const glm::vec4 wp = worldTransform * glm::vec4(pos[0], pos[1], pos[2], 1.0f);
                    glm::vec3 wn = normalMat * glm::vec3(norm[0], norm[1], norm[2]);
                    if (glm::length(wn) > 0.0001f) {
                        wn = glm::normalize(wn);
                    } else {
                        wn = glm::vec3(0.0f, 1.0f, 0.0f);
                    }

                    vertices[v] = {wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, uv[0], uv[1]};
                }

                std::vector<std::uint32_t> indices;
                if (prim.indices) {
                    indices.resize(prim.indices->count);
                    for (cgltf_size i = 0; i < prim.indices->count; ++i) {
                        indices[i] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i));
                    }
                } else {
                    indices.resize(vertices.size());
                    for (std::size_t i = 0; i < vertices.size(); ++i) {
                        indices[i] = static_cast<std::uint32_t>(i);
                    }
                }

                // Auto-compute flat normals if the glTF omitted NORMAL attributes
                if (!normAcc && indices.size() >= 3) {
                    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
                        auto& v0 = vertices[indices[i]];
                        auto& v1 = vertices[indices[i + 1]];
                        auto& v2 = vertices[indices[i + 2]];
                        const glm::vec3 a(v0.px, v0.py, v0.pz);
                        const glm::vec3 b(v1.px, v1.py, v1.pz);
                        const glm::vec3 c(v2.px, v2.py, v2.pz);
                        glm::vec3 n = glm::cross(b - a, c - a);
                        if (glm::length(n) > 0.0001f) {
                            n = glm::normalize(n);
                            v0.nx = v1.nx = v2.nx = n.x;
                            v0.ny = v1.ny = v2.ny = n.y;
                            v0.nz = v1.nz = v2.nz = n.z;
                        }
                    }
                }

                GpuSubMesh sm;
                sm.upload(vertices, indices);

                if (prim.material) {
                    const cgltf_material* mat = prim.material;
                    if (mat->has_pbr_metallic_roughness) {
                        const auto& pbr = mat->pbr_metallic_roughness;
                        sm.baseColor = {
                            pbr.base_color_factor[0],
                            pbr.base_color_factor[1],
                            pbr.base_color_factor[2],
                        };
                        sm.metallic = pbr.metallic_factor;
                        sm.roughness = pbr.roughness_factor;
                        if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                            sm.albedoTex = loadGltfImageTexture(pbr.base_color_texture.texture->image, modelDir);
                        }
                    }
                    sm.emissive = {
                        mat->emissive_factor[0],
                        mat->emissive_factor[1],
                        mat->emissive_factor[2],
                    };
                }

                gpuModel.subMeshes.push_back(sm);
            }
        };

        bool processedAnyNode = false;
        for (cgltf_size n = 0; n < data->nodes_count; ++n) {
            const cgltf_node* node = &data->nodes[n];
            if (!node->mesh) {
                continue;
            }
            float rawMat[16];
            cgltf_node_transform_world(node, rawMat);
            const glm::mat4 worldMat = glm::make_mat4(rawMat);
            processMesh(node->mesh, worldMat);
            processedAnyNode = true;
        }
        if (!processedAnyNode) {
            for (cgltf_size m = 0; m < data->meshes_count; ++m) {
                processMesh(&data->meshes[m], glm::mat4(1.0f));
            }
        }

        cgltf_free(data);

        if (gpuModel.subMeshes.empty()) {
            std::cerr << "[SAGE][Model] Warning: no drawable triangle meshes found in " << pathStr << "\n";
            m_failedModels.insert(rawPath);
            return nullptr;
        }

        std::cout << "[SAGE][Model] Loaded \"" << rawPath << "\" ("
                  << gpuModel.subMeshes.size() << " submesh(es))\n";
        auto [it, inserted] = m_modelCache.emplace(rawPath, std::move(gpuModel));
        return &it->second;
    }

    RendererInfo m_info;
    std::filesystem::path m_assetRoot;

    GLuint m_skyProgram = 0;
    GLuint m_skyVao = 0;
    GLint m_skyInvViewProj = -1;
    GLint m_skyCameraPos = -1;
    GLint m_skySunDir = -1;
    GLint m_skySunColor = -1;
    GLint m_skyZenith = -1;
    GLint m_skyHorizon = -1;
    GLint m_skyGround = -1;

    GLuint m_litProgram = 0;
    GLint m_uModel = -1;
    GLint m_uViewProj = -1;
    GLint m_uBaseColor = -1;
    GLint m_uEmissive = -1;
    GLint m_uMetallic = -1;
    GLint m_uRoughness = -1;
    GLint m_uHasTexture = -1;
    GLint m_uAlbedoTex = -1;
    GLint m_uPulse = -1;
    GLint m_uTime = -1;
    GLint m_uCameraPos = -1;
    GLint m_uSunDir = -1;
    GLint m_uSunColor = -1;
    GLint m_uSkyZenith = -1;
    GLint m_uSkyHorizon = -1;
    GLint m_uGroundAmbient = -1;
    GLint m_uAmbientIntensity = -1;
    GLint m_uFogStart = -1;
    GLint m_uFogEnd = -1;

    GpuSubMesh m_boxMesh;
    GpuSubMesh m_planeMesh;
    GpuSubMesh m_arrowMesh;
    GpuSubMesh m_ringMesh;

    std::unordered_map<std::string, GpuModel> m_modelCache;
    std::unordered_set<std::string> m_failedModels;

    int m_viewportWidth = 1280;
    int m_viewportHeight = 720;
    bool m_initialized = false;
};

} // namespace

std::unique_ptr<Renderer> createOpenGLRenderer() {
    return std::make_unique<OpenGLRenderer>();
}

} // namespace sage
