#include "renderer/gl_model.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <cgltf.h>     // Implementations are compiled in gl_renderer.cpp
#include <stb_image.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iostream>

namespace sage::render {

BoneRole classifyBoneRole(const char* rawName) {
    if (!rawName || rawName[0] == '\0') return BoneRole::Other;
    std::string s(rawName);
    for (char& ch : s) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    const bool isL = (s.find("_l") != std::string::npos || s.find(".l") != std::string::npos || s.find("left") != std::string::npos);
    const bool isR = (s.find("_r") != std::string::npos || s.find(".r") != std::string::npos || s.find("right") != std::string::npos);
    if (s.find("hip") != std::string::npos || s.find("pelvis") != std::string::npos) return BoneRole::Hips;
    if (s.find("head") != std::string::npos) return BoneRole::Head;
    if (s.find("neck") != std::string::npos) return BoneRole::Neck;
    if (s.find("chest") != std::string::npos || s.find("torso") != std::string::npos || s.find("spine2") != std::string::npos) return BoneRole::Chest;
    if (s.find("spine") != std::string::npos) return BoneRole::Spine;
    if (s.find("forearm") != std::string::npos || s.find("lowerarm") != std::string::npos) return isL ? BoneRole::ForearmL : (isR ? BoneRole::ForearmR : BoneRole::Other);
    if (s.find("upperarm") != std::string::npos || s.find("arm") != std::string::npos) return isL ? BoneRole::UpperArmL : (isR ? BoneRole::UpperArmR : BoneRole::Other);
    if (s.find("thigh") != std::string::npos || s.find("upleg") != std::string::npos) return isL ? BoneRole::ThighL : (isR ? BoneRole::ThighR : BoneRole::Other);
    if (s.find("shin") != std::string::npos || s.find("calf") != std::string::npos || s.find("leg") != std::string::npos) return isL ? BoneRole::ShinL : (isR ? BoneRole::ShinR : BoneRole::Other);
    return BoneRole::Other;
}

std::string safeGLString(GLenum name) {
    const GLubyte* raw = glGetString(name);
    return raw ? reinterpret_cast<const char*>(raw) : "Unknown";
}

GLuint createTextureFromPixels(const unsigned char* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 8x Anisotropic Filtering (GL_EXT_texture_filter_anisotropic) for crisp oblique ground/bark/fabric textures!
    constexpr GLenum kTexMaxAniso = 0x84FE;
    constexpr GLenum kMaxTexMaxAniso = 0x84FF;
    static float s_maxAniso = -1.0f;
    if (s_maxAniso < 0.0f) {
        float queried = 1.0f;
        glGetFloatv(kMaxTexMaxAniso, &queried);
        if (glGetError() != GL_NO_ERROR || queried < 1.0f) queried = 1.0f;
        s_maxAniso = std::min(queried, 8.0f);
    }
    if (s_maxAniso > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, kTexMaxAniso, s_maxAniso);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    gl::generateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint loadGltfImageTexture(const cgltf_image* image, const std::filesystem::path& modelDir) {
    if (!image) return 0;
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = nullptr;

    if (image->buffer_view && image->buffer_view->buffer && image->buffer_view->buffer->data) {
        const auto* base = static_cast<const unsigned char*>(image->buffer_view->buffer->data);
        pixels = stbi_load_from_memory(base + image->buffer_view->offset, static_cast<int>(image->buffer_view->size), &width, &height, &channels, 4);
    } else if (image->uri && image->uri[0] != '\0') {
        const std::filesystem::path imgPath = modelDir / image->uri;
        pixels = stbi_load(imgPath.string().c_str(), &width, &height, &channels, 4);
    }
    if (!pixels) return 0;
    const GLuint tex = createTextureFromPixels(pixels, width, height);
    stbi_image_free(pixels);
    return tex;
}

GpuSubMesh buildUnitCubeMesh() {
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;
    struct FaceDef { glm::vec3 n; glm::vec3 corners[4]; };
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
    GpuSubMesh mesh;
    mesh.upload(verts, {0, 1, 2, 0, 2, 3});
    return mesh;
}

GpuSubMesh buildWaterGridMesh(int divisions) {
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;
    verts.reserve(static_cast<std::size_t>((divisions + 1) * (divisions + 1)));
    idx.reserve(static_cast<std::size_t>(divisions * divisions * 6));

    for (int z = 0; z <= divisions; ++z) {
        const float v = static_cast<float>(z) / static_cast<float>(divisions);
        const float pz = v - 0.5f;
        for (int x = 0; x <= divisions; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(divisions);
            const float px = u - 0.5f;
            verts.push_back({px, 0.0f, pz, 0.0f, 1.0f, 0.0f, u, v});
        }
    }
    const int stride = divisions + 1;
    for (int z = 0; z < divisions; ++z) {
        for (int x = 0; x < divisions; ++x) {
            const auto i0 = static_cast<std::uint32_t>(z * stride + x);
            const auto i1 = i0 + 1;
            const auto i2 = static_cast<std::uint32_t>((z + 1) * stride + x);
            const auto i3 = i2 + 1;
            idx.insert(idx.end(), {i0, i2, i1, i1, i2, i3});
        }
    }
    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

GpuSubMesh buildUnitSphereMesh() {
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;
    constexpr int kRings = 16, kSectors = 24;
    for (int r = 0; r <= kRings; ++r) {
        const float v = static_cast<float>(r) / kRings;
        const float phi = v * kPi;
        for (int s = 0; s <= kSectors; ++s) {
            const float u = static_cast<float>(s) / kSectors;
            const float theta = u * 2.0f * kPi;
            const float nx = std::sin(phi) * std::cos(theta);
            const float ny = std::cos(phi);
            const float nz = std::sin(phi) * std::sin(theta);
            verts.push_back({nx * 0.5f, ny * 0.5f, nz * 0.5f, nx, ny, nz, u, v});
        }
    }
    for (int r = 0; r < kRings; ++r) {
        for (int s = 0; s < kSectors; ++s) {
            const auto i0 = static_cast<std::uint32_t>(r * (kSectors + 1) + s);
            const auto i1 = i0 + 1;
            const auto i2 = static_cast<std::uint32_t>((r + 1) * (kSectors + 1) + s);
            const auto i3 = i2 + 1;
            idx.insert(idx.end(), {i0, i2, i1, i1, i2, i3});
        }
    }
    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

GpuSubMesh buildUnitArrowMesh() {
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
    auto addQuad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) { addTri(a, b, c); addTri(a, c, d); };
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
    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;
    constexpr int kSegments = 32;
    constexpr float kOuterR = 0.50f, kInnerR = 0.42f;
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
        addQuad({c0 * kOuterR, 0, s0 * kOuterR}, {c1 * kOuterR, 0, s1 * kOuterR}, {c1 * kOuterR, 1, s1 * kOuterR}, {c0 * kOuterR, 1, s0 * kOuterR}, nOut);
        addQuad({c1 * kInnerR, 0, s1 * kInnerR}, {c0 * kInnerR, 0, s0 * kInnerR}, {c0 * kInnerR, 1, s0 * kInnerR}, {c1 * kInnerR, 1, s1 * kInnerR}, -nOut);
        addQuad({c0 * kOuterR, 1, s0 * kOuterR}, {c1 * kOuterR, 1, s1 * kOuterR}, {c1 * kInnerR, 1, s1 * kInnerR}, {c0 * kInnerR, 1, s0 * kInnerR}, {0, 1, 0});
    }
    GpuSubMesh mesh;
    mesh.upload(verts, idx);
    return mesh;
}

glm::mat4 buildEntityMatrix(const Entity& e) {
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
    return model;
}

} // namespace sage::render
