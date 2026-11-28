#version 330 core
in vec3 vWorldPos;
in vec2 vUV;
in float vWaveCrest;
in vec4 vClipPos;

uniform float uTime;
uniform vec3 uShallowColor;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform float uFogStart;
uniform float uFogEnd;
uniform sampler2D uWaterSceneDepthTex;
uniform float uWaterNearPlane;
uniform float uWaterFarPlane;

#define MAX_POINT_LIGHTS 16
uniform int uWaterPointLightCount;
uniform vec4 uWaterPointLightPosRadius[MAX_POINT_LIGHTS];
uniform vec4 uWaterPointLightColorInt[MAX_POINT_LIGHTS];

out vec4 FragColor;

float linearizeW(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uWaterNearPlane * uWaterFarPlane) / max(uWaterFarPlane + uWaterNearPlane - z * (uWaterFarPlane - uWaterNearPlane), 0.001);
}

float hash21W(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 34.5);
    return fract(p.x * p.y);
}

float noiseW(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21W(i), hash21W(i + vec2(1.0, 0.0)), f.x),
               mix(hash21W(i + vec2(0.0, 1.0)), hash21W(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main() {
    vec2 p = vWorldPos.xz;

    // Non-repeating multi-octave organic ocean & lake ripple normals (eliminates repeating stripes!)
    float e = 0.22;
    vec2 uvA = p * 0.14 + vec2(uTime * 0.28, uTime * 0.19);
    vec2 uvB = p * 0.34 - vec2(uTime * 0.42, -uTime * 0.26);
    vec2 uvC = p * 0.85 + vec2(-uTime * 0.55, uTime * 0.48);
    float h0 = noiseW(uvA) * 0.55 + noiseW(uvB) * 0.30 + noiseW(uvC) * 0.15;
    float hx = noiseW(uvA + vec2(e, 0.0)) * 0.55 + noiseW(uvB + vec2(e, 0.0)) * 0.30 + noiseW(uvC + vec2(e, 0.0)) * 0.15;
    float hz = noiseW(uvA + vec2(0.0, e)) * 0.55 + noiseW(uvB + vec2(0.0, e)) * 0.30 + noiseW(uvC + vec2(0.0, e)) * 0.15;

    float swellX = cos(p.x * 0.16 + p.y * 0.11 + uTime * 1.85) * 0.035;
    float swellZ = cos(p.x * -0.24 + p.y * 0.19 + uTime * 2.35) * 0.032;
    vec3 N = normalize(vec3((h0 - hx) * 0.42 - swellX, 1.0, (h0 - hz) * 0.42 - swellZ));
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunDir);
    vec3 R = reflect(-V, N);

    // TRUE PER-PIXEL DEPTH-BUFFER WATER DEPTH (works around EVERY beach, cove, boulder & inland lake!)
    vec2 screenUV = (vClipPos.xy / max(vClipPos.w, 0.001)) * 0.5 + 0.5;
    float sceneRawDepth = texture(uWaterSceneDepthTex, clamp(screenUV, vec2(0.001), vec2(0.999))).r;
    float sceneLinZ = linearizeW(sceneRawDepth);
    float waterLinZ = linearizeW(gl_FragCoord.z);
    float waterDepth = max(sceneLinZ - waterLinZ, 0.0);

    // Rich translucent Caribbean/Pacific turquoise shallows -> deep ocean navy!
    float shallowT = 1.0 - smoothstep(0.0, 6.5, waterDepth);
    vec3 deepColor    = vec3(0.012, 0.085, 0.185);
    vec3 midColor     = uShallowColor * vec3(0.45, 0.82, 0.92);
    vec3 shallowCol   = vec3(0.08, 0.46, 0.50);
    vec3 waterBody    = mix(mix(deepColor, midColor, smoothstep(0.0, 0.65, shallowT)), shallowCol, smoothstep(0.55, 1.0, shallowT));

    // Schlick Fresnel reflection of the dynamic sky gradient!
    float fresnel = 0.03 + 0.97 * pow(1.0 - max(dot(N, V), 0.0), 4.8);
    vec3 skyReflect = mix(uSkyHorizon, uSkyZenith, clamp(R.y * 1.35 + 0.05, 0.0, 1.0));
    vec3 color = mix(waterBody, skyReflect, clamp(fresnel * 0.82, 0.0, 0.88));

    // Crisp sparkling sun highlights across ripples!
    vec3 H = normalize(L + V);
    float specBroad = pow(max(dot(N, H), 0.0), 56.0) * 0.45;
    float specSharp = pow(max(dot(N, H), 0.0), 340.0) * 2.80;
    color += uSunColor * (specBroad + specSharp);

    // Dynamic point-light shimmer (campfires & radio beacon reflected in water!)
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
            float wSpec = pow(max(dot(N, pH), 0.0), 72.0) * 2.0 + max(dot(N, plDir), 0.0) * 0.22;
            color += lc * wSpec * att;
        }
    }

    // Animated organic lapping shoreline surf foam + subtle offshore whitecaps!
    float foamNoise = noiseW(p * 0.55 + vec2(uTime * 0.35, -uTime * 0.25));
    float surfRipple = sin(waterDepth * 6.5 - uTime * 3.4 + foamNoise * 4.5);
    float edgeLine = (1.0 - smoothstep(0.0, 0.42, waterDepth)) * (0.65 + 0.35 * foamNoise);
    float surfBand = (1.0 - smoothstep(0.15, 1.65, waterDepth)) * smoothstep(0.25, 0.85, surfRipple) * foamNoise;
    float crestFoam = smoothstep(0.86, 0.99, vWaveCrest) * foamNoise * 0.28;
    float totalFoam = clamp(edgeLine * 0.85 + surfBand * 0.72 + crestFoam, 0.0, 0.90);
    color = mix(color, vec3(0.92, 0.96, 0.99) * (0.68 + 0.38 * uSunColor), totalFoam);

    // Distance atmospheric fog
    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    color = mix(color, uSkyHorizon, fogFactor * fogFactor * 0.82);

    // Crystal-clear soft shoreline alpha fade (0.0m..0.55m depth — ZERO harsh polygon seam!)
    float alpha = clamp(smoothstep(0.0, 0.55, waterDepth) * 0.86 + totalFoam * 0.45 + fresnel * 0.14, 0.0, 0.96);
    FragColor = vec4(color, alpha);
}
