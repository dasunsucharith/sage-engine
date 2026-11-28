#pragma once

// Internal renderer module (OpenGL backend). Not part of the public engine API.
// GPU-side mesh/model data (vertices, sub-meshes, skins, animation clips, loaded glTF
// models) plus small helpers: primitive meshes, glTF textures and entity matrices.

#include "renderer/gl_functions.h"
#include "sage/world/world.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct cgltf_image;

namespace sage::render {

inline constexpr float kPi = 3.14159265358979323846f;

struct Vertex3D {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float j0 = 0.0f, j1 = 0.0f, j2 = 0.0f, j3 = 0.0f;
    float w0 = 1.0f, w1 = 0.0f, w2 = 0.0f, w3 = 0.0f;
};

struct PropInstanceData {
    glm::mat4 model{1.0f};
    glm::vec4 colorEmissive{1.0f, 1.0f, 1.0f, 0.0f};
};

struct GpuSubMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    GLuint albedoTex = 0;
    GLuint normalTex = 0;
    GLuint metRoughTex = 0;
    glm::vec3 baseColor{1.0f, 1.0f, 1.0f};
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    float metallic = 0.1f;
    float roughness = 0.6f;

    void attachInstanceBuffer(GLuint instVbo) const {
        if (!vao || !instVbo) return;
        gl::bindVertexArray(vao);
        gl::bindBuffer(GL_ARRAY_BUFFER, instVbo);
        for (int col = 0; col < 4; ++col) {
            const GLuint loc = static_cast<GLuint>(5 + col);
            gl::vertexAttribPointer(
                loc, 4, GL_FLOAT, GL_FALSE, sizeof(PropInstanceData),
                reinterpret_cast<void*>(static_cast<std::size_t>(col) * sizeof(glm::vec4)));
            gl::enableVertexAttribArray(loc);
            gl::vertexAttribDivisor(loc, 1);
        }
        gl::vertexAttribPointer(
            9, 4, GL_FLOAT, GL_FALSE, sizeof(PropInstanceData),
            reinterpret_cast<void*>(offsetof(PropInstanceData, colorEmissive)));
        gl::enableVertexAttribArray(9);
        gl::vertexAttribDivisor(9, 1);
        gl::bindVertexArray(0);
    }

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
        gl::vertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, j0)));
        gl::enableVertexAttribArray(3);
        gl::vertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, w0)));
        gl::enableVertexAttribArray(4);

        gl::bindVertexArray(0);
    }

    void destroy() {
        if (metRoughTex) { glDeleteTextures(1, &metRoughTex); metRoughTex = 0; }
        if (normalTex) { glDeleteTextures(1, &normalTex); normalTex = 0; }
        if (albedoTex) { glDeleteTextures(1, &albedoTex); albedoTex = 0; }
        if (ebo) { gl::deleteBuffers(1, &ebo); ebo = 0; }
        if (vbo) { gl::deleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { gl::deleteVertexArrays(1, &vao); vao = 0; }
        indexCount = 0;
    }
};

struct GpuSkin {
    std::vector<int> jointNodeIndices;
    std::vector<glm::mat4> inverseBindMatrices;
};

enum class BoneRole : std::uint8_t {
    Other = 0,
    Hips,
    Spine,
    Chest,
    Neck,
    Head,
    UpperArmL,
    UpperArmR,
    ForearmL,
    ForearmR,
    ThighL,
    ThighR,
    ShinL,
    ShinR
};

struct ModelNode {
    int parentIdx = -1;
    int skinIdx = -1;
    BoneRole boneRole = BoneRole::Other;
    bool hasMatrix = false;
    glm::mat4 staticMatrix{1.0f};
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::quat restWorldRot{1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<std::size_t> subMeshIndices;
};

enum class AnimPath { Translation, Rotation, Scale };

struct AnimChannel {
    int nodeIdx = -1;
    AnimPath path = AnimPath::Translation;
    std::vector<float> times;
    std::vector<glm::vec4> values;
};

struct AnimClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimChannel> channels;
};

struct GpuModel {
    std::vector<GpuSubMesh> subMeshes;
    std::vector<ModelNode> nodes;
    std::vector<GpuSkin> skins;
    std::vector<AnimClip> animations;
    glm::vec3 boundCenter{0.0f, 1.0f, 0.0f};
    float boundRadius = 4.0f;
    int headBoneCount = 0;
    int neckBoneCount = 0;
    int chestBoneCount = 0;
    bool hasAnimations = false;
    bool hasSkins = false;

    // Depth-only shadow meshes. The shadow pass ignores materials, so all sub-meshes that
    // move together can be drawn in ONE call: per node for animated/skinned models, or the
    // whole model for static ones. indexCount == 0 means "not merged, draw sub-meshes".
    std::vector<GpuSubMesh> shadowNodeMeshes; // Indexed like `nodes` (animated/skinned models)
    GpuSubMesh shadowWholeMesh;               // Static models

    void destroy() {
        for (auto& sm : subMeshes) sm.destroy();
        subMeshes.clear();
        for (auto& sm : shadowNodeMeshes) sm.destroy();
        shadowNodeMeshes.clear();
        shadowWholeMesh.destroy();
        nodes.clear();
        skins.clear();
        animations.clear();
        headBoneCount = 0;
        neckBoneCount = 0;
        chestBoneCount = 0;
        hasAnimations = false;
        hasSkins = false;
    }
};

BoneRole classifyBoneRole(const char* rawName);

std::string safeGLString(GLenum name);

GLuint createTextureFromPixels(const unsigned char* pixels, int width, int height);

GLuint loadGltfImageTexture(const cgltf_image* image, const std::filesystem::path& modelDir);

GpuSubMesh buildUnitCubeMesh();

GpuSubMesh buildUnitPlaneMesh();

GpuSubMesh buildWaterGridMesh(int divisions = 56);

GpuSubMesh buildUnitSphereMesh();

GpuSubMesh buildUnitArrowMesh();

GpuSubMesh buildUnitRingMesh();

glm::mat4 buildEntityMatrix(const Entity& e);

} // namespace sage::render
