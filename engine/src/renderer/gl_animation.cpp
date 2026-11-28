#include "renderer/gl_animation.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace sage::render {

void sampleModelClips(
    const GpuModel& model,
    const std::string& clipQuery,
    float clipTime,
    bool clipLoop,
    std::vector<glm::vec3>& T,
    std::vector<glm::quat>& R,
    std::vector<glm::vec3>& S,
    std::vector<std::uint8_t>& animatedFlag,
    std::vector<const AnimClip*>& clipsToEval) {
    const std::size_t n = model.nodes.size();
    T.resize(n);
    R.resize(n);
    S.resize(n);
    animatedFlag.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        T[i] = model.nodes[i].translation;
        R[i] = model.nodes[i].rotation;
        S[i] = model.nodes[i].scale;
    }

    clipsToEval.clear();
    if (!model.animations.empty() && clipQuery != "none") {
        if (!clipQuery.empty()) {
            for (const auto& c : model.animations) {
                if (c.name == clipQuery) { clipsToEval.push_back(&c); break; }
            }
            if (clipsToEval.empty()) {
                for (const auto& c : model.animations) {
                    if (c.name.size() >= clipQuery.size() &&
                        c.name.compare(0, clipQuery.size(), clipQuery) == 0) {
                        clipsToEval.push_back(&c);
                    }
                }
            }
        }
        if (clipsToEval.empty()) {
            for (const auto& c : model.animations) clipsToEval.push_back(&c);
        }
    }

    for (const AnimClip* clip : clipsToEval) {
        if (!clip || clip->duration <= 0.0001f) continue;
        float t = clipTime;
        if (clipLoop) {
            t = std::fmod(t, clip->duration);
            if (t < 0.0f) t += clip->duration;
        } else {
            t = std::clamp(t, 0.0f, clip->duration);
        }
        for (const auto& ch : clip->channels) {
            if (ch.nodeIdx < 0 || static_cast<std::size_t>(ch.nodeIdx) >= n || ch.times.empty()) continue;
            animatedFlag[static_cast<std::size_t>(ch.nodeIdx)] = 1;
            std::size_t i0 = 0, i1 = 0;
            float alpha = 0.0f;
            if (t <= ch.times.front()) { i0 = i1 = 0; }
            else if (t >= ch.times.back()) { i0 = i1 = ch.times.size() - 1; }
            else {
                const auto it = std::upper_bound(ch.times.begin(), ch.times.end(), t);
                i1 = static_cast<std::size_t>(it - ch.times.begin());
                i0 = i1 > 0 ? i1 - 1 : 0;
                const float span = ch.times[i1] - ch.times[i0];
                alpha = span > 0.00001f ? (t - ch.times[i0]) / span : 0.0f;
            }
            const glm::vec4& v0 = ch.values[i0];
            const glm::vec4& v1 = ch.values[i1];
            const std::size_t ni = static_cast<std::size_t>(ch.nodeIdx);
            if (ch.path == AnimPath::Translation) T[ni] = glm::mix(glm::vec3(v0), glm::vec3(v1), alpha);
            else if (ch.path == AnimPath::Scale) S[ni] = glm::mix(glm::vec3(v0), glm::vec3(v1), alpha);
            else if (ch.path == AnimPath::Rotation) {
                R[ni] = glm::slerp(glm::normalize(glm::quat(v0.w, v0.x, v0.y, v0.z)),
                                   glm::normalize(glm::quat(v1.w, v1.x, v1.y, v1.z)), alpha);
            }
        }
    }
}

// `look` / `rag` are the entity's optional components (nullptr when it has none).
void evaluateModelNodeMatrices(const GpuModel& model, const Entity& entity, const LookAtComponent* look,
                               const RagdollComponent* rag, AnimEvalScratch& sc, std::vector<glm::mat4>& outWorld) {
    const float ragdollBlend = rag ? rag->blend : 0.0f;
    const std::size_t n = model.nodes.size();
    outWorld.resize(n);
    if (n == 0) return;

    sampleModelClips(model, entity.activeAnimation, entity.animTime, entity.animLoop, sc.T, sc.R, sc.S, sc.flag, sc.clips);

    // Real-time bone-level crossfade blending between outgoing & incoming animation clips!
    if (entity.blendDuration > 0.001f && !entity.prevAnimation.empty() && entity.blendElapsed < entity.blendDuration) {
        sampleModelClips(model, entity.prevAnimation, entity.prevAnimTime, entity.prevAnimLoop, sc.prevT, sc.prevR, sc.prevS, sc.prevFlag, sc.clips);

        const float rawW = std::clamp(entity.blendElapsed / entity.blendDuration, 0.0f, 1.0f);
        const float w = rawW * rawW * (3.0f - 2.0f * rawW); // Smoothstep S-curve!
        for (std::size_t i = 0; i < n; ++i) {
            if (sc.flag[i] || sc.prevFlag[i]) {
                sc.T[i] = glm::mix(sc.prevT[i], sc.T[i], w);
                sc.R[i] = glm::slerp(sc.prevR[i], sc.R[i], w);
                sc.S[i] = glm::mix(sc.prevS[i], sc.S[i], w);
                sc.flag[i] = 1;
            }
        }
    }

    // Real-Time Entity-Space Normalized Head/Neck/Chest Look-At IK (v1.6.0: Bipeds + Multi-Bone Quadrupeds!)
    if (look && look->weight > 0.01f && ragdollBlend < 0.75f) {
        const float eyeH = std::max(model.boundCenter.y * std::abs(entity.scale.y) + 0.35f, 0.65f);
        const float dx = look->target.x - entity.position.x;
        const float dy = look->target.y - (entity.position.y + eyeH);
        const float dz = look->target.z - entity.position.z;
        const float horiz = std::sqrt(dx * dx + dz * dz);
        if (horiz > 0.30f) {
            const float targetYaw = std::atan2(dx, dz);
            const float entYaw = glm::radians(entity.rotation.y);
            float relYaw = targetYaw - entYaw;
            while (relYaw > kPi) relYaw -= 2.0f * kPi;
            while (relYaw < -kPi) relYaw += 2.0f * kPi;

            // Smoothly fade look-at if target is far behind character's shoulders
            const float frontFade = 1.0f - std::clamp((std::abs(relYaw) - 1.20f) / 0.85f, 0.0f, 1.0f);
            const float w = look->weight * frontFade * (1.0f - ragdollBlend);
            if (w > 0.01f) {
                const float clampedYaw = std::clamp(relYaw, -0.95f, 0.95f);
                const float clampedPitch = std::clamp(-std::atan2(dy, std::max(horiz, 0.6f)), -0.38f, 0.38f);
                const float headShare = (model.headBoneCount > 0) ? (0.50f / static_cast<float>(model.headBoneCount)) : 0.0f;
                const float neckShare = (model.neckBoneCount > 0) ? (0.32f / static_cast<float>(model.neckBoneCount)) : 0.0f;
                const float chestShare = (model.chestBoneCount > 0) ? (0.18f / static_cast<float>(model.chestBoneCount)) : 0.0f;
                for (std::size_t i = 0; i < n; ++i) {
                    const BoneRole role = model.nodes[i].boneRole;
                    float frac = 0.0f;
                    if (role == BoneRole::Head) frac = headShare;
                    else if (role == BoneRole::Neck) frac = neckShare;
                    else if (role == BoneRole::Chest) frac = chestShare;
                    if (frac > 0.0f) {
                        // Transform entity-space yaw/pitch into this bone's exact rest-pose local space!
                        const glm::quat qEntity =
                            glm::angleAxis(clampedYaw * frac * w, glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::angleAxis(clampedPitch * frac * w, glm::vec3(1.0f, 0.0f, 0.0f));
                        const glm::quat& restW = model.nodes[i].restWorldRot;
                        const glm::quat qLocal = glm::normalize(glm::conjugate(restW) * qEntity * restW);
                        sc.R[i] = glm::normalize(sc.R[i] * qLocal);
                        sc.flag[i] = 1;
                    }
                }
            }
        }
    }

    // Euphoria-Style Active-Physics Ragdoll Bone Layer!
    if (rag && ragdollBlend > 0.004f) {
        const float b = std::clamp(ragdollBlend, 0.0f, 1.0f);
        const float rb = b * b * (3.0f - 2.0f * b);
        const float t = rag->elapsed;
        const float speed = std::sqrt(
            rag->vel.x * rag->vel.x +
            rag->vel.y * rag->vel.y +
            rag->vel.z * rag->vel.z);
        const float flailAmp = std::clamp(speed * 0.08f, 0.12f, 1.0f);
        const float airborneFactor = std::clamp(std::abs(rag->vel.y) * 0.14f, 0.0f, 1.0f);
        const float waveA = std::sin(t * 7.8f) * flailAmp;
        const float waveB = std::cos(t * 6.3f + 0.9f) * flailAmp;

        for (std::size_t i = 0; i < n; ++i) {
            const BoneRole role = model.nodes[i].boneRole;
            if (role == BoneRole::Other) continue;

            glm::quat deltaQ(1.0f, 0.0f, 0.0f, 0.0f);
            switch (role) {
                case BoneRole::Hips: {
                    const glm::quat qTumble =
                        glm::angleAxis(rag->angle.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::angleAxis(rag->angle.x, glm::vec3(1.0f, 0.0f, 0.0f)) *
                        glm::angleAxis(rag->angle.z, glm::vec3(0.0f, 0.0f, 1.0f));
                    deltaQ = qTumble;
                    const float groundFactor = 1.0f - airborneFactor;
                    sc.T[i].y = glm::mix(sc.T[i].y, sc.T[i].y - 0.62f * groundFactor, rb);
                    break;
                }
                case BoneRole::Spine:
                case BoneRole::Chest:
                    deltaQ = glm::angleAxis(0.28f * waveA - 0.22f * airborneFactor, glm::vec3(1.0f, 0.0f, 0.0f))
                           * glm::angleAxis(0.24f * waveB, glm::vec3(0.0f, 0.0f, 1.0f));
                    break;
                case BoneRole::Neck:
                case BoneRole::Head:
                    deltaQ = glm::angleAxis(-0.35f * waveB, glm::vec3(1.0f, 0.0f, 0.0f))
                           * glm::angleAxis(0.32f * waveA, glm::vec3(0.0f, 1.0f, 0.0f));
                    break;
                case BoneRole::UpperArmL:
                    deltaQ = glm::angleAxis(-0.85f - 0.65f * airborneFactor + 0.55f * waveA, glm::vec3(0.0f, 0.0f, 1.0f))
                           * glm::angleAxis(-0.55f + 0.45f * waveB, glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                case BoneRole::UpperArmR:
                    deltaQ = glm::angleAxis(0.85f + 0.65f * airborneFactor + 0.55f * waveB, glm::vec3(0.0f, 0.0f, 1.0f))
                           * glm::angleAxis(-0.55f - 0.45f * waveA, glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                case BoneRole::ForearmL:
                    deltaQ = glm::angleAxis(-0.55f - 0.35f * waveB, glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                case BoneRole::ForearmR:
                    deltaQ = glm::angleAxis(-0.55f + 0.35f * waveA, glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                case BoneRole::ThighL:
                    deltaQ = glm::angleAxis(-0.42f + 0.55f * waveB, glm::vec3(1.0f, 0.0f, 0.0f))
                           * glm::angleAxis(-0.22f, glm::vec3(0.0f, 0.0f, 1.0f));
                    break;
                case BoneRole::ThighR:
                    deltaQ = glm::angleAxis(-0.38f - 0.55f * waveA, glm::vec3(1.0f, 0.0f, 0.0f))
                           * glm::angleAxis(0.22f, glm::vec3(0.0f, 0.0f, 1.0f));
                    break;
                case BoneRole::ShinL:
                    deltaQ = glm::angleAxis(0.68f + 0.35f * waveA, glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                case BoneRole::ShinR:
                    deltaQ = glm::angleAxis(0.62f - 0.35f * waveB, glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                case BoneRole::Other:
                default:
                    break;
            }
            sc.R[i] = glm::slerp(sc.R[i], glm::normalize(sc.R[i] * deltaQ), rb);
            sc.flag[i] = 1;
        }
    }

    sc.local.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (model.nodes[i].hasMatrix && !sc.flag[i]) sc.local[i] = model.nodes[i].staticMatrix;
        else sc.local[i] = glm::translate(glm::mat4(1.0f), sc.T[i]) * glm::mat4_cast(sc.R[i]) * glm::scale(glm::mat4(1.0f), sc.S[i]);
    }
    sc.computed.assign(n, 0);
    auto computeWorld = [&](auto&& self, std::size_t idx) -> const glm::mat4& {
        if (sc.computed[idx]) return outWorld[idx];
        const int parent = model.nodes[idx].parentIdx;
        outWorld[idx] = (parent >= 0 && static_cast<std::size_t>(parent) < n)
            ? self(self, static_cast<std::size_t>(parent)) * sc.local[idx]
            : sc.local[idx];
        sc.computed[idx] = 1;
        return outWorld[idx];
    };
    for (std::size_t i = 0; i < n; ++i) computeWorld(computeWorld, i);
}

} // namespace sage::render
