#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;
layout (location = 5) in vec4 aInstCol0;
layout (location = 6) in vec4 aInstCol1;
layout (location = 7) in vec4 aInstCol2;
layout (location = 8) in vec4 aInstCol3;

#define MAX_BONES 64
uniform int uHasSkin;
uniform int uUseInstancing;
uniform mat4 uBoneMatrices[MAX_BONES];
uniform mat4 uModel;
uniform mat4 uLightViewProj;

void main() {
    vec4 localPos = vec4(aPos, 1.0);
    if (uHasSkin == 1) {
        int j0 = clamp(int(aJoints.x + 0.5), 0, MAX_BONES - 1);
        int j1 = clamp(int(aJoints.y + 0.5), 0, MAX_BONES - 1);
        int j2 = clamp(int(aJoints.z + 0.5), 0, MAX_BONES - 1);
        int j3 = clamp(int(aJoints.w + 0.5), 0, MAX_BONES - 1);
        mat4 skinMat = aWeights.x * uBoneMatrices[j0]
                     + aWeights.y * uBoneMatrices[j1]
                     + aWeights.z * uBoneMatrices[j2]
                     + aWeights.w * uBoneMatrices[j3];
        localPos = skinMat * localPos;
    }
    mat4 modelMat = (uUseInstancing == 1) ? mat4(aInstCol0, aInstCol1, aInstCol2, aInstCol3) : uModel;
    gl_Position = uLightViewProj * modelMat * localPos;
}
