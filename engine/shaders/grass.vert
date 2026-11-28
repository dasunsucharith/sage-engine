#version 330 core
layout (location = 0) in vec3 aBladePos;   // Local crossed-clump geometry (y in 0..1)
layout (location = 1) in vec4 aInstPosH;   // xyz = world base, w = height scale
layout (location = 2) in vec4 aInstMeta;   // x = yaw, y = colorShift, z = windPhase, w = flowerFlag

uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform vec3 uInteractPos;
uniform float uTime;

out float vHeightFrac;
out float vColorShift;
out float vFlowerFlag;
out vec3 vWorldPos;

void main() {
    vec3 basePos = aInstPosH.xyz;
    float dist = length(uCameraPos.xz - basePos.xz);
    float lodScale = 1.0 - smoothstep(105.0, 155.0, dist);
    if (lodScale <= 0.01) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vHeightFrac = 0.0;
        vColorShift = 0.0;
        vFlowerFlag = 0.0;
        vWorldPos = basePos;
        return;
    }

    float hFrac = clamp(aBladePos.y, 0.0, 1.0);
    float cy = cos(aInstMeta.x);
    float sy = sin(aInstMeta.x);

    vec3 local = vec3(
        aBladePos.x * cy - aBladePos.z * sy,
        aBladePos.y * aInstPosH.w * lodScale,
        aBladePos.x * sy + aBladePos.z * cy
    );

    // Multi-wave rolling wind gusts bending grass tips quadratically!
    float gust = sin(basePos.x * 0.13 + basePos.z * 0.09 + uTime * 2.2 + aInstMeta.z) * 0.24
               + cos(basePos.x * 0.29 - basePos.z * 0.21 + uTime * 3.6) * 0.10;
    float bend = hFrac * hFrac * aInstPosH.w * lodScale;
    local.x += gust * bend;
    local.z += gust * 0.65 * bend;

    // Interactive player / vehicle grass parting & flattening!
    vec2 toBlade = basePos.xz - uInteractPos.xz;
    float pDist = length(toBlade);
    if (pDist < 3.4 && abs(basePos.y - uInteractPos.y) < 2.8) {
        float push = (1.0 - pDist / 3.4);
        push = push * push;
        vec2 pushDir = (pDist > 0.05) ? (toBlade / pDist) : vec2(1.0, 0.0);
        local.xz += pushDir * (push * 0.65 * hFrac * aInstPosH.w);
        local.y *= (1.0 - push * 0.58 * hFrac);
    }

    vec3 wp = basePos + local;
    vWorldPos = wp;
    vHeightFrac = hFrac;
    vColorShift = aInstMeta.y;
    vFlowerFlag = aInstMeta.w;
    gl_Position = uViewProj * vec4(wp, 1.0);
}
