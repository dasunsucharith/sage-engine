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
constexpr int kShadowMapSize = 2048;

// -----------------------------------------------------------------------------
// Pass 0: Directional Shadow Map Depth Pass
// -----------------------------------------------------------------------------
constexpr const char* kShadowVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uLightViewProj;

void main() {
    gl_Position = uLightViewProj * uModel * vec4(aPos, 1.0);
}
)GLSL";

constexpr const char* kShadowFragmentShader = R"GLSL(
#version 330 core
void main() {}
)GLSL";

// -----------------------------------------------------------------------------
// Pass 1: HDR Procedural Sky Dome + Sun Disk + Drifting Volumetric-Style Clouds
// -----------------------------------------------------------------------------
constexpr const char* kSkyVertexShader = R"GLSL(
#version 330 core
out vec2 vNdc;

void main() {
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
uniform float uTime;

out vec4 FragColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float amp = 0.52;
    for (int i = 0; i < 4; ++i) {
        v += amp * valueNoise(p);
        p = p * 2.07 + vec2(13.1, 7.7);
        amp *= 0.5;
    }
    return v;
}

void main() {
    vec4 farWorld = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 rayDir = normalize(farWorld.xyz / farWorld.w - uCameraPos);

    float h = clamp(rayDir.y, 0.0, 1.0);
    vec3 sky = mix(uSkyHorizon, uSkyZenith, pow(h, 0.55));
    if (rayDir.y < 0.0) {
        sky = mix(uSkyHorizon, uGroundAmbient, clamp(-rayDir.y * 4.0, 0.0, 1.0));
    }

    vec3 sunN = normalize(uSunDir);
    float sunDot = max(dot(rayDir, sunN), 0.0);

    // Warm atmospheric scattering glow around the sun + twilight horizon warmth
    sky += uSunColor * pow(sunDot, 10.0) * 0.40;
    sky += uSunColor * smoothstep(0.9982, 0.9995, sunDot) * 4.0;

    // Night starfield (appears smoothly when sky zenith darkens into Neon Night!)
    float nightFactor = clamp(1.0 - (uSkyZenith.r + uSkyZenith.g + uSkyZenith.b) * 3.8, 0.0, 1.0);
    if (nightFactor > 0.02 && rayDir.y > 0.06) {
        vec2 starCell = floor((rayDir.xz / (rayDir.y + 0.35)) * 120.0);
        float rnd = hash21(starCell);
        if (rnd > 0.992) {
            float twinkle = 0.65 + 0.35 * sin(uTime * 3.5 + rnd * 120.0);
            sky += vec3(0.85, 0.92, 1.0) * (rnd - 0.992) * 260.0 * twinkle * nightFactor * smoothstep(0.06, 0.28, rayDir.y);
        }
    }

    // Drifting multi-layered procedural clouds overhead (projected onto high sky dome plane)
    if (rayDir.y > 0.030) {
        vec2 cloudUV = (rayDir.xz / (rayDir.y + 0.14)) * 1.35 + vec2(uTime * 0.012, uTime * 0.006);
        float c = fbm(cloudUV);
        float cDetail = fbm(cloudUV * 2.3 - vec2(uTime * 0.008, 0.0));
        float cloudShape = mix(c, cDetail, 0.28);
        float density = smoothstep(0.46, 0.78, cloudShape) * smoothstep(0.030, 0.22, rayDir.y);
        vec3 cloudLit = mix(uSkyHorizon * 0.95, uSunColor * 1.42, 0.45 + 0.55 * pow(sunDot, 4.0));
        vec3 cloudShadow = mix(uSkyZenith * 0.62, uSkyHorizon * 0.72, 0.45);
        vec3 cloudColor = mix(cloudShadow, cloudLit, clamp(cloudShape * 1.28, 0.0, 1.0));
        sky = mix(sky, cloudColor, density * 0.76);
    }

    // Outputs linear HDR color for post-processing Bloom + ACES Tone Mapping
    FragColor = vec4(sky, 1.0);
}
)GLSL";

// -----------------------------------------------------------------------------
// Pass 2: 3D HDR Lit Mesh Shader + Soft Shadows + Procedural Surface Micro-Grain
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
uniform int uHasNormalTex;
uniform sampler2D uNormalTex;
uniform int uHasMetRoughTex;
uniform sampler2D uMetRoughTex;
uniform float uPulse;
uniform float uTime;
uniform int uIsGround;
uniform float uWetness;

uniform int uShadowsEnabled;
uniform mat4 uLightViewProj;
uniform mat4 uLightViewProjFar;
uniform sampler2DShadow uShadowMap;
uniform sampler2DShadow uShadowMapFar;
uniform float uShadowTexelSize;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uGroundAmbient;
uniform float uAmbientIntensity;
uniform float uFogStart;
uniform float uFogEnd;

#define MAX_POINT_LIGHTS 16
uniform int uPointLightCount;
uniform vec4 uPointLightPosRadius[MAX_POINT_LIGHTS];
uniform vec4 uPointLightColorInt[MAX_POINT_LIGHTS];

out vec4 FragColor;

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float groundNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float sampleCascadePcf(sampler2DShadow smap, mat4 lvp, vec3 worldPos, vec3 N, vec3 L, float biasScale, out float edgeFade) {
    float ndotl = max(dot(N, L), 0.0);
    float normalOffset = (0.045 + 0.12 * (1.0 - ndotl)) * biasScale;
    vec4 lightClip = lvp * vec4(worldPos + N * normalOffset, 1.0);
    vec3 proj = lightClip.xyz / lightClip.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        edgeFade = 0.0;
        return 1.0;
    }

    vec2 edgeDist = min(proj.xy, 1.0 - proj.xy);
    edgeFade = smoothstep(0.0, 0.10, min(edgeDist.x, edgeDist.y));

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize * 1.35;
            visibility += texture(smap, vec3(proj.xy + offset, proj.z - 0.0005 * biasScale));
        }
    }
    return visibility / 9.0;
}

float computeShadowVisibility(vec3 worldPos, vec3 N, vec3 L) {
    if (uShadowsEnabled == 0) {
        return 1.0;
    }

    // 2-Cascade Shadow Maps (CSM): ultra-crisp 32m Near Cascade seamlessly blended into 135m Far Cascade!
    float nearFade = 0.0;
    float nearVis = sampleCascadePcf(uShadowMap, uLightViewProj, worldPos, N, L, 0.85, nearFade);
    if (nearFade > 0.99) {
        return nearVis;
    }
    float farFade = 0.0;
    float farVis = sampleCascadePcf(uShadowMapFar, uLightViewProjFar, worldPos, N, L, 2.2, farFade);
    float farResult = mix(1.0, farVis, farFade);
    return mix(farResult, nearVis, nearFade);
}

// Cook-Torrance GGX Microfacet PBR BRDF (matches modern UE4 / RAGE physically based lighting!)
const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 evaluatePbrLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metal, float rough, vec3 F0) {
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0001) return vec3(0.0);
    vec3 H = normalize(V + L);
    float D = distributionGGX(N, H, rough);
    float G = geometrySmith(N, V, L, rough);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 spec = (D * G * F) / max(4.0 * max(dot(N, V), 0.0) * NdotL, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metal);
    return (kD * albedo / PI + spec) * radiance * NdotL;
}

vec3 perturbNormalFromMap(vec3 N, vec3 worldPos, vec2 uv) {
    vec3 mapN = texture(uNormalTex, uv).xyz * 2.0 - 1.0;
    vec3 Q1 = dFdx(worldPos);
    vec3 Q2 = dFdy(worldPos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * mapN);
}

void main() {
    vec3 albedo = uBaseColor;
    if (uHasTexture == 1) {
        vec4 texel = texture(uAlbedoTex, vUV);
        albedo *= pow(texel.rgb, vec3(2.2));
    }

    // Procedural world-space micro-grain + glTF Metallic-Roughness texture + dynamic rain puddles!
    float rough = clamp(uRoughness, 0.06, 1.0);
    float metal = clamp(uMetallic, 0.0, 1.0);
    if (uHasMetRoughTex == 1) {
        vec4 mr = texture(uMetRoughTex, vUV);
        rough = clamp(rough * mr.g, 0.06, 1.0);
        metal = clamp(metal * mr.b, 0.0, 1.0);
    }

    if (uIsGround == 1) {
        float nLarge = groundNoise(vWorldPos.xz * 0.14);
        float nFine  = groundNoise(vWorldPos.xz * 1.65);
        float grain  = (nLarge * 0.65 + nFine * 0.35) - 0.5;
        albedo *= (1.0 + grain * 0.24);
        rough = clamp(rough + grain * 0.15, 0.22, 0.96);

        if (uWetness > 0.01) {
            float puddleNoise = groundNoise(vWorldPos.xz * 0.11 + vec2(3.7, 9.1));
            float puddle = smoothstep(0.42, 0.72, puddleNoise) * uWetness;
            albedo *= (1.0 - 0.34 * uWetness);
            rough = mix(rough, 0.06, clamp(uWetness * 0.45 + puddle * 0.55, 0.0, 0.92));
            metal = mix(metal, 0.48, puddle * 0.78);
        }
    }

    vec3 N = normalize(vWorldNormal);
    if (uHasNormalTex == 1) {
        N = perturbNormalFromMap(N, vWorldPos, vUV);
    }
    vec3 L = normalize(uSunDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metal);

    float shadowVis = computeShadowVisibility(vWorldPos, N, L);

    // Direct Sun Radiance using true Cook-Torrance GGX PBR!
    vec3 directSun = evaluatePbrLight(N, V, L, uSunColor * 3.45, albedo, metal, rough, F0) * shadowVis;

    // Hemisphere Sky & Ground Bounce IBL + Glossy Clearcoat Sky Reflections!
    float hemiT = N.y * 0.5 + 0.5;
    vec3 skyFill = mix(uSkyHorizon, uSkyZenith, 0.35);
    vec3 hemiAmbient = mix(uGroundAmbient, skyFill, hemiT) * albedo * (1.0 - 0.55 * metal) * (uAmbientIntensity * 1.40);

    vec3 R = reflect(-V, N);
    vec3 envSky = mix(uSkyHorizon, uSkyZenith, clamp(R.y * 1.2 + 0.15, 0.0, 1.0));
    vec3 Fenv = fresnelSchlick(max(dot(N, V), 0.0), F0);
    float envGloss = (1.0 - rough) * (1.0 - rough);
    vec3 iblSpec = envSky * Fenv * (0.28 + 0.92 * envGloss);

    // Subtle ground-contact ambient occlusion for grounded 3D buildings & cars near y=0
    float contactAO = (uIsGround == 0) ? mix(0.76, 1.0, smoothstep(0.0, 0.65, max(vWorldPos.y, 0.0))) : 1.0;

    // Emissive values exceed 1.0 in HDR so Bloom picks up headlights, neon & beacons!
    vec3 emissiveTerm = uEmissive * 1.55;
    if (uPulse > 0.5) {
        float p = 0.80 + 0.20 * sin(uTime * 5.0);
        emissiveTerm *= p * 1.35;
    }

    vec3 color = (hemiAmbient * contactAO) + directSun + iblSpec + emissiveTerm;

    // Real-time Dynamic Point Lights evaluated with true GGX PBR!
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (i >= uPointLightCount) break;
        vec3 lp = uPointLightPosRadius[i].xyz;
        float lr = max(uPointLightPosRadius[i].w, 0.5);
        vec3 lc = uPointLightColorInt[i].rgb * uPointLightColorInt[i].w;
        vec3 toL = lp - vWorldPos;
        float d = length(toL);
        if (d < lr) {
            vec3 plDir = toL / max(d, 0.001);
            float att = 1.0 - (d / lr);
            att = att * att;
            color += evaluatePbrLight(N, V, plDir, lc * att * 2.4, albedo, metal, rough, F0);
        }
    }

    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    color = mix(color, uSkyHorizon, fogFactor * fogFactor * 0.85);

    // Output linear HDR (tone-mapped in PostPass!)
    FragColor = vec4(color, 1.0);
}
)GLSL";

// -----------------------------------------------------------------------------
// Pass 3: Animated 3D Ocean Water Shader (Multi-Wave Gerstner + Fresnel + Foam!)
// -----------------------------------------------------------------------------
constexpr const char* kWaterVertexShader = R"GLSL(
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

void main() {
    vec4 baseWorld = uModel * vec4(aPos, 1.0);
    vec2 p = baseWorld.xz;

    // 4-directional ocean wave superposition
    float w1 = sin(p.x * 0.16 + p.y * 0.11 + uTime * 1.85) * 0.22;
    float w2 = sin(p.x * -0.24 + p.y * 0.19 + uTime * 2.35) * 0.14;
    float w3 = cos(p.x * 0.38 - p.y * 0.31 + uTime * 3.10) * 0.07;
    float w4 = sin((p.x + p.y) * 0.55 - uTime * 2.75) * 0.04;

    float disp = w1 + w2 + w3 + w4;
    baseWorld.y += disp;

    vWorldPos = baseWorld.xyz;
    vUV = aUV;
    vWaveCrest = clamp((disp + 0.45) / 0.90, 0.0, 1.0);
    gl_Position = uViewProj * baseWorld;
}
)GLSL";

constexpr const char* kWaterFragmentShader = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec2 vUV;
in float vWaveCrest;

uniform float uTime;
uniform vec3 uShallowColor;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform float uFogStart;
uniform float uFogEnd;

#define MAX_POINT_LIGHTS 16
uniform int uWaterPointLightCount;
uniform vec4 uWaterPointLightPosRadius[MAX_POINT_LIGHTS];
uniform vec4 uWaterPointLightColorInt[MAX_POINT_LIGHTS];

out vec4 FragColor;

void main() {
    vec2 p = vWorldPos.xz;

    // Analytical + high-frequency ripple normal for sparkling water highlights
    float dHx = cos(p.x * 0.16 + p.y * 0.11 + uTime * 1.85) * 0.055
              - cos(p.x * -0.24 + p.y * 0.19 + uTime * 2.35) * 0.050
              - sin(p.x * 0.38 - p.y * 0.31 + uTime * 3.10) * 0.042
              + cos(p.x * 1.35 + p.y * 0.90 + uTime * 4.20) * 0.025;
    float dHz = cos(p.x * 0.16 + p.y * 0.11 + uTime * 1.85) * 0.040
              + cos(p.x * -0.24 + p.y * 0.19 + uTime * 2.35) * 0.045
              + sin(p.x * 0.38 - p.y * 0.31 + uTime * 3.10) * 0.036
              + sin(p.x * 0.85 - p.y * 1.40 + uTime * 3.80) * 0.025;

    vec3 N = normalize(vec3(-dHx, 1.0, -dHz));
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunDir);
    vec3 R = reflect(-V, N);

    // Distance from shore edge (vUV near 0/1 = shallow turquoise + animated shoreline foam!)
    float edgeDist = min(min(vUV.x, 1.0 - vUV.x), min(vUV.y, 1.0 - vUV.y));
    float shallowFactor = 1.0 - smoothstep(0.0, 0.28, edgeDist);

    vec3 deepColor = uShallowColor * vec3(0.28, 0.42, 0.68);
    vec3 waterBody = mix(deepColor, uShallowColor * 1.25, shallowFactor);

    // Schlick Fresnel reflection of the real dynamic sky gradient!
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 4.5);
    vec3 skyReflect = mix(uSkyHorizon, uSkyZenith, clamp(R.y * 1.4, 0.0, 1.0));
    vec3 color = mix(waterBody, skyReflect, clamp(fresnel * 0.88, 0.0, 0.92));

    // Crisp sun specular sparkle path across the waves (triggers subtle HDR bloom!)
    vec3 H = normalize(L + V);
    float specBroad = pow(max(dot(N, H), 0.0), 48.0) * 0.65;
    float specSharp = pow(max(dot(N, H), 0.0), 320.0) * 3.20;
    color += uSunColor * (specBroad + specSharp);

    // Dynamic point-light shimmer & colored reflections across the animated water ripples!
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (i >= uWaterPointLightCount) break;
        vec3 lp = uWaterPointLightPosRadius[i].xyz;
        float lr = max(uWaterPointLightPosRadius[i].w * 1.35, 1.0);
        vec3 lc = uWaterPointLightColorInt[i].rgb * uWaterPointLightColorInt[i].w;
        vec3 toL = lp - vWorldPos;
        float d = length(toL);
        if (d < lr) {
            vec3 plDir = toL / max(d, 0.001);
            float att = 1.0 - (d / lr);
            att = att * att;
            vec3 pH = normalize(plDir + V);
            float wSpec = pow(max(dot(N, pH), 0.0), 72.0) * 2.2 + max(dot(N, plDir), 0.0) * 0.25;
            color += lc * wSpec * att;
        }
    }

    // Animated shoreline surf foam + subtle wave-crest whitecaps
    float foamWave = sin(edgeDist * 55.0 - uTime * 4.2 + sin(p.x * 0.35 + p.y * 0.35) * 2.5);
    float shoreFoam = smoothstep(0.11, 0.0, edgeDist) * smoothstep(0.15, 0.75, foamWave);
    float crestFoam = smoothstep(0.84, 0.98, vWaveCrest) * 0.35;
    color = mix(color, vec3(0.95, 0.98, 1.0) * (0.75 + 0.35 * uSunColor), clamp(shoreFoam + crestFoam, 0.0, 0.88));

    // Distance atmospheric fog
    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    color = mix(color, uSkyHorizon, fogFactor * fogFactor * 0.85);

    float alpha = mix(0.94, 0.62, smoothstep(0.06, 0.0, edgeDist));
    FragColor = vec4(color, alpha);
}
)GLSL";

// -----------------------------------------------------------------------------
// Pass 4 & 5: HDR Bloom Bright-Extract, Separable Gaussian Blur & Final Composite
// -----------------------------------------------------------------------------
constexpr const char* kPostVertexShader = R"GLSL(
#version 330 core
out vec2 vUV;
void main() {
    vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vUV = pos[gl_VertexID] * 0.5 + 0.5;
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}
)GLSL";

constexpr const char* kBloomExtractFrag = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uHdrTex;
out vec4 FragColor;

void main() {
    vec3 col = texture(uHdrTex, vUV).rgb;
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    float soft = smoothstep(0.85, 1.65, lum);
    FragColor = vec4(col * soft, 1.0);
}
)GLSL";

constexpr const char* kBloomBlurFrag = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uImage;
uniform vec2 uDirection;
out vec4 FragColor;

void main() {
    const float weights[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(uImage, vUV).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 off = uDirection * float(i) * 1.4;
        result += texture(uImage, vUV + off).rgb * weights[i];
        result += texture(uImage, vUV - off).rgb * weights[i];
    }
    FragColor = vec4(result, 1.0);
}
)GLSL";

constexpr const char* kCompositeFrag = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uHdrTex;
uniform sampler2D uBloomTex;
uniform sampler2D uDepthTex;
uniform int uBloomEnabled;
uniform float uBloomIntensity;
uniform int uSsaoEnabled;
uniform float uExposure;
uniform vec2 uTexelSize;
uniform float uNearPlane;
uniform float uFarPlane;

out vec4 FragColor;

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uNearPlane * uFarPlane) / max(uFarPlane + uNearPlane - z * (uFarPlane - uNearPlane), 0.001);
}

vec3 acesToneMap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHdrTex, vUV).rgb;

    // Real-time Screen-Space Depth Contact AO (SSAO) darkening geometric crevices & contact seams!
    if (uSsaoEnabled == 1) {
        float centerRaw = texture(uDepthTex, vUV).r;
        if (centerRaw < 0.9992) {
            float centerZ = linearizeDepth(centerRaw);
            const vec2 dirs[8] = vec2[8](
                vec2( 1.0,  0.0), vec2(-1.0,  0.0), vec2( 0.0,  1.0), vec2( 0.0, -1.0),
                vec2( 0.71, 0.71), vec2(-0.71, 0.71), vec2( 0.71,-0.71), vec2(-0.71,-0.71)
            );
            float radScale = clamp(42.0 / max(centerZ, 4.0), 1.5, 6.5);
            float occ = 0.0;
            for (int i = 0; i < 8; ++i) {
                vec2 off = dirs[i] * uTexelSize * radScale;
                float sampleZ = linearizeDepth(texture(uDepthTex, vUV + off).r);
                float diff = centerZ - sampleZ;
                float rangeCheck = smoothstep(2.6, 0.15, abs(diff));
                occ += step(0.06, diff) * rangeCheck;
            }
            float ao = 1.0 - (occ / 8.0) * 0.42;
            hdr *= ao;
        }
    }

    if (uBloomEnabled == 1) {
        vec3 bloom = texture(uBloomTex, vUV).rgb;
        hdr += bloom * uBloomIntensity;
    }

    hdr *= max(uExposure, 0.2);

    // Subtle cinematic edge vignette
    vec2 centered = vUV * 2.0 - 1.0;
    float vig = 1.0 - dot(centered, centered) * 0.10;
    hdr *= clamp(vig, 0.80, 1.0);

    vec3 mapped = acesToneMap(hdr);
    // Subtle cinematic vibrance lift for crisp colors
    float luma = dot(mapped, vec3(0.299, 0.587, 0.114));
    mapped = clamp(mix(vec3(luma), mapped, 1.08), 0.0, 1.0);
    mapped = pow(mapped, vec3(1.0 / 2.2));
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
    GLuint normalTex = 0;
    GLuint metRoughTex = 0;
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
        if (metRoughTex) { glDeleteTextures(1, &metRoughTex); metRoughTex = 0; }
        if (normalTex) { glDeleteTextures(1, &normalTex); normalTex = 0; }
        if (albedoTex) { glDeleteTextures(1, &albedoTex); albedoTex = 0; }
        if (ebo) { gl::deleteBuffers(1, &ebo); ebo = 0; }
        if (vbo) { gl::deleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { gl::deleteVertexArrays(1, &vao); vao = 0; }
        indexCount = 0;
    }
};

struct ModelNode {
    int parentIdx = -1;
    bool hasMatrix = false;
    glm::mat4 staticMatrix{1.0f};
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
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
    std::vector<AnimClip> animations;
    bool hasAnimations = false;

    void destroy() {
        for (auto& sm : subMeshes) sm.destroy();
        subMeshes.clear();
        nodes.clear();
        animations.clear();
        hasAnimations = false;
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
    if (!pixels || width <= 0 || height <= 0) return 0;
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

GpuSubMesh buildWaterGridMesh(int divisions = 56) {
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

void evaluateModelNodeMatrices(const GpuModel& model, const Entity& entity, std::vector<glm::mat4>& outWorld) {
    const std::size_t n = model.nodes.size();
    outWorld.assign(n, glm::mat4(1.0f));
    if (n == 0) return;

    std::vector<glm::vec3> T(n), S(n);
    std::vector<glm::quat> R(n);
    std::vector<bool> animatedFlag(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        T[i] = model.nodes[i].translation;
        R[i] = model.nodes[i].rotation;
        S[i] = model.nodes[i].scale;
    }

    std::vector<const AnimClip*> clipsToEval;
    if (!model.animations.empty() && entity.activeAnimation != "none") {
        if (!entity.activeAnimation.empty()) {
            // Exact match first, then case-insensitive prefix match (handles Blender NLA strip names!)
            for (const auto& c : model.animations) {
                if (c.name == entity.activeAnimation) { clipsToEval.push_back(&c); break; }
            }
            if (clipsToEval.empty()) {
                for (const auto& c : model.animations) {
                    if (c.name.size() >= entity.activeAnimation.size() &&
                        c.name.compare(0, entity.activeAnimation.size(), entity.activeAnimation) == 0) {
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
        float t = entity.animTime;
        if (entity.animLoop) {
            t = std::fmod(t, clip->duration);
            if (t < 0.0f) t += clip->duration;
        } else {
            t = std::clamp(t, 0.0f, clip->duration);
        }
        for (const auto& ch : clip->channels) {
            if (ch.nodeIdx < 0 || static_cast<std::size_t>(ch.nodeIdx) >= n || ch.times.empty()) continue;
            animatedFlag[ch.nodeIdx] = true;
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
            if (ch.path == AnimPath::Translation) T[ch.nodeIdx] = glm::mix(glm::vec3(v0), glm::vec3(v1), alpha);
            else if (ch.path == AnimPath::Scale) S[ch.nodeIdx] = glm::mix(glm::vec3(v0), glm::vec3(v1), alpha);
            else if (ch.path == AnimPath::Rotation) {
                R[ch.nodeIdx] = glm::slerp(glm::normalize(glm::quat(v0.w, v0.x, v0.y, v0.z)),
                                           glm::normalize(glm::quat(v1.w, v1.x, v1.y, v1.z)), alpha);
            }
        }
    }

    std::vector<glm::mat4> local(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (model.nodes[i].hasMatrix && !animatedFlag[i]) local[i] = model.nodes[i].staticMatrix;
        else local[i] = glm::translate(glm::mat4(1.0f), T[i]) * glm::mat4_cast(R[i]) * glm::scale(glm::mat4(1.0f), S[i]);
    }
    std::vector<bool> computed(n, false);
    auto computeWorld = [&](auto&& self, std::size_t idx) -> const glm::mat4& {
        if (computed[idx]) return outWorld[idx];
        const int parent = model.nodes[idx].parentIdx;
        outWorld[idx] = (parent >= 0 && static_cast<std::size_t>(parent) < n)
            ? self(self, static_cast<std::size_t>(parent)) * local[idx]
            : local[idx];
        computed[idx] = true;
        return outWorld[idx];
    };
    for (std::size_t i = 0; i < n; ++i) computeWorld(computeWorld, i);
}

class OpenGLRenderer final : public Renderer {
public:
    ~OpenGLRenderer() override { shutdown(); }

    bool initialize(int viewportWidth, int viewportHeight) override {
        if (!gl::loadFunctions()) return false;

        m_info.backendName = "OpenGL 3.3 Core (v1.1: GGX PBR + Normal/MR Maps + SSAO + 2-Cascade CSM + HDR Bloom + Wet Roads)";
        m_info.gpuVendor = safeGLString(GL_VENDOR);
        m_info.gpuRenderer = safeGLString(GL_RENDERER);
        m_info.apiVersion = safeGLString(GL_VERSION);

        m_shadowProgram = linkProgram(kShadowVertexShader, kShadowFragmentShader, "ShadowPass");
        m_skyProgram = linkProgram(kSkyVertexShader, kSkyFragmentShader, "SkyPass");
        m_litProgram = linkProgram(kLitVertexShader, kLitFragmentShader, "Lit3DPass");
        m_waterProgram = linkProgram(kWaterVertexShader, kWaterFragmentShader, "OceanWaterPass");
        m_bloomExtractProgram = linkProgram(kPostVertexShader, kBloomExtractFrag, "BloomExtract");
        m_bloomBlurProgram = linkProgram(kPostVertexShader, kBloomBlurFrag, "BloomBlur");
        m_compositeProgram = linkProgram(kPostVertexShader, kCompositeFrag, "HDRComposite");

        if (!m_shadowProgram || !m_skyProgram || !m_litProgram || !m_waterProgram ||
            !m_bloomExtractProgram || !m_bloomBlurProgram || !m_compositeProgram) {
            shutdown();
            return false;
        }

        gl::genVertexArrays(1, &m_fullscreenVao);

        // Dual 2048x2048 hardware-PCF Cascaded Shadow Map FBOs (Cascade 0 = Near 32m, Cascade 1 = Far 135m!)
        auto createShadowCascade = [](GLuint& fboOut, GLuint& texOut) {
            gl::genFramebuffers(1, &fboOut);
            glGenTextures(1, &texOut);
            glBindTexture(GL_TEXTURE_2D, texOut);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowMapSize, kShadowMapSize, 0,
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
        createShadowCascade(m_shadowFbo, m_shadowTex);
        createShadowCascade(m_shadowFboFar, m_shadowTexFar);

        // Cache uniform locations
        m_shadowModel = gl::getUniformLocation(m_shadowProgram, "uModel");
        m_shadowLightVP = gl::getUniformLocation(m_shadowProgram, "uLightViewProj");

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
        m_uWetness = gl::getUniformLocation(m_litProgram, "uWetness");
        m_uShadowsEnabled = gl::getUniformLocation(m_litProgram, "uShadowsEnabled");
        m_uLightViewProj = gl::getUniformLocation(m_litProgram, "uLightViewProj");
        m_uLightViewProjFar = gl::getUniformLocation(m_litProgram, "uLightViewProjFar");
        m_uShadowMap = gl::getUniformLocation(m_litProgram, "uShadowMap");
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

        m_bloomExtractTex = gl::getUniformLocation(m_bloomExtractProgram, "uHdrTex");
        m_bloomBlurImage = gl::getUniformLocation(m_bloomBlurProgram, "uImage");
        m_bloomBlurDir = gl::getUniformLocation(m_bloomBlurProgram, "uDirection");
        m_compHdrTex = gl::getUniformLocation(m_compositeProgram, "uHdrTex");
        m_compBloomTex = gl::getUniformLocation(m_compositeProgram, "uBloomTex");
        m_compDepthTex = gl::getUniformLocation(m_compositeProgram, "uDepthTex");
        m_compBloomEnabled = gl::getUniformLocation(m_compositeProgram, "uBloomEnabled");
        m_compBloomIntensity = gl::getUniformLocation(m_compositeProgram, "uBloomIntensity");
        m_compSsaoEnabled = gl::getUniformLocation(m_compositeProgram, "uSsaoEnabled");
        m_compExposure = gl::getUniformLocation(m_compositeProgram, "uExposure");
        m_compTexelSize = gl::getUniformLocation(m_compositeProgram, "uTexelSize");
        m_compNearPlane = gl::getUniformLocation(m_compositeProgram, "uNearPlane");
        m_compFarPlane = gl::getUniformLocation(m_compositeProgram, "uFarPlane");

        m_boxMesh = buildUnitCubeMesh();
        m_planeMesh = buildUnitPlaneMesh();
        m_waterMesh = buildWaterGridMesh(56);
        m_sphereMesh = buildUnitSphereMesh();
        m_arrowMesh = buildUnitArrowMesh();
        m_ringMesh = buildUnitRingMesh();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        onResize(viewportWidth, viewportHeight);
        m_initialized = true;
        return true;
    }

    void destroyPostBuffers() {
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

        // Main HDR RGBA16F framebuffer + sampleable Depth24 texture (for real-time Screen-Space AO!)
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
        for (auto& [path, model] : m_modelCache) model.destroy();
        m_modelCache.clear();
        m_failedModels.clear();

        m_boxMesh.destroy();
        m_planeMesh.destroy();
        m_waterMesh.destroy();
        m_sphereMesh.destroy();
        m_arrowMesh.destroy();
        m_ringMesh.destroy();

        destroyPostBuffers();
        if (m_shadowTexFar) { glDeleteTextures(1, &m_shadowTexFar); m_shadowTexFar = 0; }
        if (m_shadowFboFar) { gl::deleteFramebuffers(1, &m_shadowFboFar); m_shadowFboFar = 0; }
        if (m_shadowTex) { glDeleteTextures(1, &m_shadowTex); m_shadowTex = 0; }
        if (m_shadowFbo) { gl::deleteFramebuffers(1, &m_shadowFbo); m_shadowFbo = 0; }
        if (m_fullscreenVao) { gl::deleteVertexArrays(1, &m_fullscreenVao); m_fullscreenVao = 0; }

        auto delProg = [](GLuint& p) { if (p) { gl::deleteProgram(p); p = 0; } };
        delProg(m_shadowProgram);
        delProg(m_skyProgram);
        delProg(m_litProgram);
        delProg(m_waterProgram);
        delProg(m_bloomExtractProgram);
        delProg(m_bloomBlurProgram);
        delProg(m_compositeProgram);
        m_initialized = false;
    }

    void setAssetRoot(const std::filesystem::path& assetRoot) override { m_assetRoot = assetRoot; }

    void onResize(int width, int height) override {
        m_viewportWidth = width > 0 ? width : 1;
        m_viewportHeight = height > 0 ? height : 1;
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        if (m_initialized || m_hdrProgramReady()) {
            recreatePostBuffers(m_viewportWidth, m_viewportHeight);
        }
    }

    void beginFrame(const ClearColor& clearColor) override {
        m_clearColor = clearColor;
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void renderWorld(const World& world, float timeSeconds) override {
        if (!m_initialized) return;

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

        // --- Pass 0: Texel-Snapped 2-Cascade 2048x2048 Directional Shadow Maps (Near 32m + Far 135m!) ---
        const bool useShadows = sky.shadowsEnabled && (m_shadowFbo != 0) && (m_shadowFboFar != 0);
        glm::mat4 lightViewProj(1.0f);
        glm::mat4 lightViewProjFar(1.0f);
        if (useShadows) {
            const glm::vec3 sunDir = glm::normalize(glm::vec3(sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z));
            const glm::vec3 focusPoint(camTarget.x, 0.0f, camTarget.z);
            const glm::vec3 upAxis = std::abs(sunDir.y) > 0.98f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

            auto renderShadowCascade = [&](GLuint fbo, float radius, float backDist, float farDist) -> glm::mat4 {
                glm::mat4 lightView = glm::lookAt(focusPoint + sunDir * backDist, focusPoint, upAxis);
                const float texelWorld = (radius * 2.0f) / static_cast<float>(kShadowMapSize);
                const glm::vec4 originLS = lightView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                lightView[3][0] += std::floor(originLS.x / texelWorld) * texelWorld - originLS.x;
                lightView[3][1] += std::floor(originLS.y / texelWorld) * texelWorld - originLS.y;

                const glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 5.0f, farDist);
                const glm::mat4 lvp = lightProj * lightView;

                gl::bindFramebuffer(GL_FRAMEBUFFER, fbo);
                glViewport(0, 0, kShadowMapSize, kShadowMapSize);
                glClear(GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(2.0f, 4.0f);

                gl::useProgram(m_shadowProgram);
                gl::uniformMatrix4fv(m_shadowLightVP, 1, GL_FALSE, glm::value_ptr(lvp));

                const float cullDistSq = (radius + 32.0f) * (radius + 32.0f);
                for (const Entity& e : world.entities()) {
                    if (!e.visible || e.shape == EntityShape::Plane || e.shape == EntityShape::Water || e.shape == EntityShape::Ring) {
                        continue;
                    }
                    const float dx = e.position.x - focusPoint.x;
                    const float dz = e.position.z - focusPoint.z;
                    if (dx * dx + dz * dz > cullDistSq) {
                        continue;
                    }
                    const glm::mat4 entityMat = buildEntityMatrix(e);
                    if (e.shape == EntityShape::Model) {
                        const GpuModel* gpuModel = getOrLoadModel(e.modelPath);
                        if (!gpuModel) continue;
                        if (gpuModel->hasAnimations) {
                            evaluateModelNodeMatrices(*gpuModel, e, m_nodeWorldScratch);
                            for (std::size_t ni = 0; ni < gpuModel->nodes.size(); ++ni) {
                                if (gpuModel->nodes[ni].subMeshIndices.empty()) continue;
                                const glm::mat4 fullMat = entityMat * m_nodeWorldScratch[ni];
                                gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(fullMat));
                                for (const std::size_t smIdx : gpuModel->nodes[ni].subMeshIndices) {
                                    gl::bindVertexArray(gpuModel->subMeshes[smIdx].vao);
                                    glDrawElements(GL_TRIANGLES, gpuModel->subMeshes[smIdx].indexCount, GL_UNSIGNED_INT, nullptr);
                                }
                            }
                        } else {
                            gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                            for (const GpuSubMesh& sm : gpuModel->subMeshes) {
                                gl::bindVertexArray(sm.vao);
                                glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, nullptr);
                            }
                        }
                    } else {
                        const GpuSubMesh* prim = getPrimitiveMesh(e.shape);
                        gl::uniformMatrix4fv(m_shadowModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                        gl::bindVertexArray(prim->vao);
                        glDrawElements(GL_TRIANGLES, prim->indexCount, GL_UNSIGNED_INT, nullptr);
                    }
                }
                glDisable(GL_POLYGON_OFFSET_FILL);
                return lvp;
            };

            lightViewProj = renderShadowCascade(m_shadowFbo, 32.0f, 85.0f, 200.0f);
            lightViewProjFar = renderShadowCascade(m_shadowFboFar, 135.0f, 165.0f, 380.0f);
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

        // --- Pass 2: Opaque 3D Lit Geometry & Animated glTF Models ---
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
        gl::uniform1f(m_uWetness, std::clamp(sky.wetness, 0.0f, 1.0f));
        gl::uniform1i(m_uAlbedoTex, 0);
        gl::uniform1i(m_uNormalTex, 4);
        gl::uniform1i(m_uMetRoughTex, 5);
        gl::uniform1i(m_uShadowsEnabled, useShadows ? 1 : 0);
        gl::uniformMatrix4fv(m_uLightViewProj, 1, GL_FALSE, glm::value_ptr(lightViewProj));
        gl::uniformMatrix4fv(m_uLightViewProjFar, 1, GL_FALSE, glm::value_ptr(lightViewProjFar));
        gl::uniform1i(m_uShadowMap, 1);
        gl::uniform1i(m_uShadowMapFar, 3);
        gl::uniform1f(m_uShadowTexelSize, 1.0f / static_cast<float>(kShadowMapSize));
        if (useShadows) {
            gl::activeTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_shadowTex);
            gl::activeTexture(GL_TEXTURE3);
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

        m_waterEntitiesScratch.clear();

        for (const Entity& e : world.entities()) {
            if (!e.visible) continue;
            if (e.shape == EntityShape::Water) {
                m_waterEntitiesScratch.push_back(&e);
                continue;
            }

            const glm::mat4 entityMat = buildEntityMatrix(e);
            if (e.shape == EntityShape::Model) {
                const GpuModel* gpuModel = getOrLoadModel(e.modelPath);
                if (!gpuModel) continue;
                gl::uniform1i(m_uIsGround, 0);
                if (gpuModel->hasAnimations) {
                    evaluateModelNodeMatrices(*gpuModel, e, m_nodeWorldScratch);
                    for (std::size_t ni = 0; ni < gpuModel->nodes.size(); ++ni) {
                        if (gpuModel->nodes[ni].subMeshIndices.empty()) continue;
                        const glm::mat4 fullMat = entityMat * m_nodeWorldScratch[ni];
                        gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(fullMat));
                        for (const std::size_t smIdx : gpuModel->nodes[ni].subMeshIndices) {
                            const GpuSubMesh& sm = gpuModel->subMeshes[smIdx];
                            const glm::vec3 tinted = sm.baseColor * glm::vec3(e.color.r, e.color.g, e.color.b);
                            const glm::vec3 emissive = e.emissive ? tinted * 0.9f : sm.emissive;
                            drawSubMesh(sm, tinted, emissive, sm.metallic, sm.roughness, 0.0f);
                        }
                    }
                } else {
                    gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(entityMat));
                    for (const GpuSubMesh& sm : gpuModel->subMeshes) {
                        const glm::vec3 tinted = sm.baseColor * glm::vec3(e.color.r, e.color.g, e.color.b);
                        const glm::vec3 emissive = e.emissive ? tinted * 0.9f : sm.emissive;
                        drawSubMesh(sm, tinted, emissive, sm.metallic, sm.roughness, 0.0f);
                    }
                }
                continue;
            }

            gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(entityMat));
            const GpuSubMesh* prim = getPrimitiveMesh(e.shape);
            const bool isGround = (e.shape == EntityShape::Plane && !e.emissive);
            gl::uniform1i(m_uIsGround, isGround ? 1 : 0);
            float pulse = (e.shape == EntityShape::Ring) ? 1.0f : 0.0f;
            float roughness = (e.shape == EntityShape::Plane) ? 0.85f : 0.55f;

            const glm::vec3 baseCol(e.color.r, e.color.g, e.color.b);
            const glm::vec3 emissiveCol = (e.emissive || e.shape == EntityShape::Ring) ? baseCol * 1.15f : glm::vec3(0.0f);
            drawSubMesh(*prim, baseCol, emissiveCol, 0.10f, roughness, pulse);
        }

        // --- Pass 3: Animated 3D Ocean Water (Alpha-Blended Waves + Fresnel + Foam) ---
        if (!m_waterEntitiesScratch.empty()) {
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
            }
            glDisable(GL_BLEND);
        }

        // --- Pass 3B: 3D HDR Particle Effects (Water Splashes, Drift Smoke, Nitro Sparks) ---
        if (!world.particles().empty()) {
            gl::useProgram(m_litProgram);
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

        // --- Pass 3C: Real-Time 3D Falling Rain Streaks (when Weather rainIntensity > 0.05!) ---
        if (sky.rainIntensity > 0.05f) {
            gl::useProgram(m_litProgram);
            gl::uniform1i(m_uIsGround, 0);
            gl::uniform1i(m_uHasTexture, 0);
            gl::uniform1i(m_uHasNormalTex, 0);
            gl::uniform1i(m_uHasMetRoughTex, 0);
            gl::uniform3f(m_uBaseColor, 0.72f, 0.84f, 0.96f);
            gl::uniform3f(m_uEmissive, 0.45f, 0.58f, 0.78f);
            gl::uniform1f(m_uMetallic, 0.0f);
            gl::uniform1f(m_uRoughness, 0.3f);
            gl::uniform1f(m_uPulse, 0.0f);
            gl::bindVertexArray(m_boxMesh.vao);

            const int dropCount = static_cast<int>(std::clamp(sky.rainIntensity, 0.0f, 1.0f) * 140.0f);
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
                rMat = glm::scale(rMat, glm::vec3(0.025f, 0.78f, 0.025f));
                gl::uniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(rMat));
                glDrawElements(GL_TRIANGLES, m_boxMesh.indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }

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
    RendererInfo info() const override { return m_info; }
    std::size_t loadedModelCount() const override { return m_modelCache.size(); }

private:
    bool m_hdrProgramReady() const { return m_compositeProgram != 0; }

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

        auto uploadMeshPrimitives = [&](const cgltf_mesh* mesh, const glm::mat4& bakeTransform, std::vector<std::size_t>* outIndices) {
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

                std::vector<Vertex3D> vertices(posAcc->count);
                for (cgltf_size v = 0; v < posAcc->count; ++v) {
                    float pos[3] = {0, 0, 0}, norm[3] = {0, 1, 0}, uv[2] = {0, 0};
                    cgltf_accessor_read_float(posAcc, v, pos, 3);
                    if (normAcc) cgltf_accessor_read_float(normAcc, v, norm, 3);
                    if (uvAcc) cgltf_accessor_read_float(uvAcc, v, uv, 2);
                    const glm::vec4 wp = bakeTransform * glm::vec4(pos[0], pos[1], pos[2], 1.0f);
                    glm::vec3 wn = normalMat * glm::vec3(norm[0], norm[1], norm[2]);
                    wn = glm::length(wn) > 0.0001f ? glm::normalize(wn) : glm::vec3(0, 1, 0);
                    vertices[v] = {wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, uv[0], uv[1]};
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

                GpuSubMesh sm;
                sm.upload(vertices, indices);
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

        if (gpuModel.hasAnimations && data->nodes_count > 0) {
            gpuModel.nodes.resize(data->nodes_count);
            for (cgltf_size n = 0; n < data->nodes_count; ++n) {
                const cgltf_node* src = &data->nodes[n];
                ModelNode& dst = gpuModel.nodes[n];
                dst.parentIdx = src->parent ? static_cast<int>(src->parent - data->nodes) : -1;
                if (src->has_matrix) { dst.hasMatrix = true; dst.staticMatrix = glm::make_mat4(src->matrix); }
                if (src->has_translation) dst.translation = {src->translation[0], src->translation[1], src->translation[2]};
                if (src->has_rotation) dst.rotation = glm::quat(src->rotation[3], src->rotation[0], src->rotation[1], src->rotation[2]);
                if (src->has_scale) dst.scale = {src->scale[0], src->scale[1], src->scale[2]};
                if (src->mesh) uploadMeshPrimitives(src->mesh, glm::mat4(1.0f), &dst.subMeshIndices);
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
            bool processedAny = false;
            for (cgltf_size n = 0; n < data->nodes_count; ++n) {
                const cgltf_node* node = &data->nodes[n];
                if (!node->mesh) continue;
                float rawMat[16];
                cgltf_node_transform_world(node, rawMat);
                uploadMeshPrimitives(node->mesh, glm::make_mat4(rawMat), nullptr);
                processedAny = true;
            }
            if (!processedAny) {
                for (cgltf_size m = 0; m < data->meshes_count; ++m)
                    uploadMeshPrimitives(&data->meshes[m], glm::mat4(1.0f), nullptr);
            }
        }

        cgltf_free(data);
        if (gpuModel.subMeshes.empty()) { m_failedModels.insert(rawPath); return nullptr; }
        std::cout << "[SAGE][Model] Loaded \"" << rawPath << "\" (" << gpuModel.subMeshes.size() << " submesh(es)"
                  << (gpuModel.hasAnimations ? (", " + std::to_string(gpuModel.animations.size()) + " anim clip(s)") : "") << ")\n";
        auto [it, inserted] = m_modelCache.emplace(rawPath, std::move(gpuModel));
        return &it->second;
    }

    RendererInfo m_info;
    std::filesystem::path m_assetRoot;
    ClearColor m_clearColor{};

    GLuint m_fullscreenVao = 0;
    GLuint m_shadowProgram = 0, m_shadowFbo = 0, m_shadowTex = 0, m_shadowFboFar = 0, m_shadowTexFar = 0;
    GLint m_shadowModel = -1, m_shadowLightVP = -1;

    GLuint m_hdrFbo = 0, m_hdrColorTex = 0, m_hdrDepthTex = 0;
    GLuint m_bloomFbo[2]{0, 0}, m_bloomTex[2]{0, 0};
    int m_bloomWidth = 640, m_bloomHeight = 360;

    GLuint m_skyProgram = 0;
    GLint m_skyInvViewProj = -1, m_skyCameraPos = -1, m_skySunDir = -1, m_skySunColor = -1;
    GLint m_skyZenith = -1, m_skyHorizon = -1, m_skyGround = -1, m_skyTime = -1;

    GLuint m_litProgram = 0;
    GLint m_uModel = -1, m_uViewProj = -1, m_uBaseColor = -1, m_uEmissive = -1;
    GLint m_uMetallic = -1, m_uRoughness = -1, m_uHasTexture = -1, m_uAlbedoTex = -1;
    GLint m_uHasNormalTex = -1, m_uNormalTex = -1, m_uHasMetRoughTex = -1, m_uMetRoughTex = -1;
    GLint m_uPulse = -1, m_uTime = -1, m_uIsGround = -1, m_uWetness = -1;
    GLint m_uShadowsEnabled = -1, m_uLightViewProj = -1, m_uLightViewProjFar = -1, m_uShadowMap = -1, m_uShadowMapFar = -1, m_uShadowTexelSize = -1;
    GLint m_uCameraPos = -1, m_uSunDir = -1, m_uSunColor = -1;
    GLint m_uSkyZenith = -1, m_uSkyHorizon = -1, m_uGroundAmbient = -1, m_uAmbientIntensity = -1;
    GLint m_uFogStart = -1, m_uFogEnd = -1;
    GLint m_uPointLightCount = -1, m_uPointLightPosRadius = -1, m_uPointLightColorInt = -1;

    GLuint m_waterProgram = 0;
    GLint m_waterModel = -1, m_waterViewProj = -1, m_waterTime = -1, m_waterShallow = -1;
    GLint m_waterCamPos = -1, m_waterSunDir = -1, m_waterSunColor = -1;
    GLint m_waterSkyZenith = -1, m_waterSkyHorizon = -1, m_waterFogStart = -1, m_waterFogEnd = -1;
    GLint m_waterPointLightCount = -1, m_waterPointLightPosRadius = -1, m_waterPointLightColorInt = -1;

    GLuint m_bloomExtractProgram = 0, m_bloomBlurProgram = 0, m_compositeProgram = 0;
    GLint m_bloomExtractTex = -1, m_bloomBlurImage = -1, m_bloomBlurDir = -1;
    GLint m_compHdrTex = -1, m_compBloomTex = -1, m_compDepthTex = -1, m_compBloomEnabled = -1, m_compBloomIntensity = -1;
    GLint m_compSsaoEnabled = -1, m_compExposure = -1, m_compTexelSize = -1, m_compNearPlane = -1, m_compFarPlane = -1;

    GpuSubMesh m_boxMesh, m_planeMesh, m_waterMesh, m_sphereMesh, m_arrowMesh, m_ringMesh;
    std::unordered_map<std::string, GpuModel> m_modelCache;
    std::unordered_set<std::string> m_failedModels;
    std::vector<glm::mat4> m_nodeWorldScratch;
    std::vector<const Entity*> m_waterEntitiesScratch;

    int m_viewportWidth = 1280, m_viewportHeight = 720;
    bool m_initialized = false;
};

} // namespace

std::unique_ptr<Renderer> createOpenGLRenderer() {
    return std::make_unique<OpenGLRenderer>();
}

} // namespace sage
