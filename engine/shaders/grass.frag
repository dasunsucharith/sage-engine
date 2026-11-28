#version 330 core
in float vHeightFrac;
in float vColorShift;
in float vFlowerFlag;
in vec3 vWorldPos;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uGroundAmbient;
uniform float uAmbientIntensity;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uTime;
uniform int uCloudShadowsEnabled;
uniform int uShadowsEnabled;
uniform mat4 uLightViewProj;
uniform mat4 uLightViewProjMid;
uniform sampler2DShadow uShadowMap;
uniform sampler2DShadow uShadowMapMid;

out vec4 FragColor;

float hash21G(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}
float noiseG(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21G(i), hash21G(i + vec2(1, 0)), f.x),
               mix(hash21G(i + vec2(0, 1)), hash21G(i + vec2(1, 1)), f.x), f.y);
}

float sampleGrassShadow(vec3 wp) {
    if (uShadowsEnabled == 0) return 1.0;
    vec4 lc0 = uLightViewProj * vec4(wp + vec3(0.0, 0.14, 0.0), 1.0);
    vec3 p0 = (lc0.xyz / lc0.w) * 0.5 + 0.5;
    if (p0.x > 0.02 && p0.x < 0.98 && p0.y > 0.02 && p0.y < 0.98 && p0.z <= 1.0) {
        return texture(uShadowMap, vec3(p0.xy, p0.z - 0.0012));
    }
    vec4 lc1 = uLightViewProjMid * vec4(wp + vec3(0.0, 0.22, 0.0), 1.0);
    vec3 p1 = (lc1.xyz / lc1.w) * 0.5 + 0.5;
    if (p1.x > 0.02 && p1.x < 0.98 && p1.y > 0.02 && p1.y < 0.98 && p1.z <= 1.0) {
        return texture(uShadowMapMid, vec3(p1.xy, p1.z - 0.0022));
    }
    return 1.0;
}

void main() {
    // Rich natural woodland grass palette that blends seamlessly with organic terrain floor!
    vec3 rootCol = vec3(0.052, 0.095, 0.036);
    vec3 tipCol  = mix(vec3(0.115, 0.225, 0.068), vec3(0.245, 0.280, 0.095), vColorShift);
    vec3 albedo  = mix(rootCol, tipCol, smoothstep(0.0, 0.92, vHeightFrac));

    // Subtle wildflower tips
    if (vFlowerFlag > 0.5 && vHeightFrac > 0.76) {
        vec3 flw = (vColorShift > 0.5) ? vec3(0.88, 0.72, 0.22) : vec3(0.76, 0.58, 0.88);
        albedo = mix(albedo, flw, smoothstep(0.76, 0.95, vHeightFrac));
    }

    vec3 L = normalize(uSunDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    float ndotl = clamp(L.y * 0.85 + 0.22, 0.12, 1.0);
    float translucency = pow(max(dot(-V, L), 0.0), 3.0) * vHeightFrac * 0.45;

    float shVis = sampleGrassShadow(vWorldPos);
    if (uCloudShadowsEnabled == 1) {
        float c1 = noiseG(vWorldPos.xz * 0.0085 + vec2(uTime * 0.022, uTime * 0.011));
        shVis *= mix(1.0, 0.78, smoothstep(0.48, 0.78, c1));
    }

    float rootAO = mix(0.44, 1.0, vHeightFrac);
    vec3 skyFill = mix(uGroundAmbient, mix(uSkyHorizon, uSkyZenith, 0.35), 0.60);
    vec3 color = albedo * (skyFill * uAmbientIntensity * 1.48 + uSunColor * (ndotl * 1.40 + translucency) * shVis) * rootAO;

    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    float sunInscatter = pow(max(dot(-V, L), 0.0), 6.0);
    vec3 fogColor = mix(uSkyHorizon, uSunColor * 1.12, sunInscatter * 0.36);
    color = mix(color, fogColor, fogFactor * fogFactor * 0.82);

    FragColor = vec4(color, 0.0);
}
