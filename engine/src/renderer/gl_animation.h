#pragma once

// Internal renderer module (OpenGL backend). Not part of the public engine API.
// glTF animation evaluation: samples clips (with crossfades) and builds each node's
// world matrix, then layers procedural look-at IK and the ragdoll tumble on top.

#include "renderer/gl_model.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <vector>

namespace sage::render {

struct AnimEvalScratch {
    std::vector<glm::vec3> T, S, prevT, prevS;
    std::vector<glm::quat> R, prevR;
    std::vector<std::uint8_t> flag, prevFlag, computed;
    std::vector<glm::mat4> local;
    std::vector<const AnimClip*> clips;
};

void sampleModelClips(
    const GpuModel& model,
    const std::string& clipQuery,
    float clipTime,
    bool clipLoop,
    std::vector<glm::vec3>& T,
    std::vector<glm::quat>& R,
    std::vector<glm::vec3>& S,
    std::vector<std::uint8_t>& animatedFlag,
    std::vector<const AnimClip*>& clipsToEval);

// `look` / `rag` are the entity's optional components (nullptr when it has none).
void evaluateModelNodeMatrices(const GpuModel& model, const Entity& entity, const LookAtComponent* look,
                               const RagdollComponent* rag, AnimEvalScratch& sc, std::vector<glm::mat4>& outWorld);

} // namespace sage::render
