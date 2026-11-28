#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform float uTime;

out vec3 vWorldPos;
out vec2 vUV;
out float vWaveCrest;
out vec4 vClipPos;

void main() {
    vec4 baseWorld = uModel * vec4(aPos, 1.0);
    vec2 p = baseWorld.xz;

    // 4-directional ocean wave superposition (matches World::sampleWater exactly!)
    float w1 = sin(p.x * 0.16 + p.y * 0.11 + uTime * 1.85) * 0.22;
    float w2 = sin(p.x * -0.24 + p.y * 0.19 + uTime * 2.35) * 0.14;
    float w3 = cos(p.x * 0.38 - p.y * 0.31 + uTime * 3.10) * 0.07;
    float w4 = sin((p.x + p.y) * 0.55 - uTime * 2.75) * 0.04;

    float disp = w1 + w2 + w3 + w4;
    baseWorld.y += disp;

    vWorldPos = baseWorld.xyz;
    vUV = aUV;
    vWaveCrest = clamp((disp + 0.45) / 0.90, 0.0, 1.0);
    vClipPos = uViewProj * baseWorld;
    gl_Position = vClipPos;
}
