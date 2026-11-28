#version 330 core
in vec2 vUV;
uniform sampler2D uHdrTex;
uniform sampler2D uBloomTex;
uniform sampler2D uDepthTex;
uniform int uBloomEnabled;
uniform float uBloomIntensity;
uniform int uSsaoEnabled;
uniform int uFxaaEnabled;
uniform int uGodRaysEnabled;
uniform float uGodRayIntensity;
uniform vec2 uSunScreenPos;
uniform vec3 uSunColorComp;
uniform int uSsrEnabled;
uniform float uSsrIntensity;
uniform mat4 uViewProjComp;
uniform mat4 uInvViewProjComp;
uniform vec3 uCameraPosComp;
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

float lumaOf(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

// Fast FXAA 3.11 edge-directed anti-aliasing directly in HDR color space!
vec3 sampleHdrFxaa(vec2 uv) {
    vec3 rgbM = texture(uHdrTex, uv).rgb;
    if (uFxaaEnabled == 0) {
        return rgbM;
    }
    vec3 rgbNW = texture(uHdrTex, uv + vec2(-1.0, -1.0) * uTexelSize).rgb;
    vec3 rgbNE = texture(uHdrTex, uv + vec2( 1.0, -1.0) * uTexelSize).rgb;
    vec3 rgbSW = texture(uHdrTex, uv + vec2(-1.0,  1.0) * uTexelSize).rgb;
    vec3 rgbSE = texture(uHdrTex, uv + vec2( 1.0,  1.0) * uTexelSize).rgb;

    float lumaM  = lumaOf(acesToneMap(rgbM));
    float lumaNW = lumaOf(acesToneMap(rgbNW));
    float lumaNE = lumaOf(acesToneMap(rgbNE));
    float lumaSW = lumaOf(acesToneMap(rgbSW));
    float lumaSE = lumaOf(acesToneMap(rgbSE));

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.045, lumaMax * 0.125)) {
        return rgbM;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-6.0), vec2(6.0)) * uTexelSize;

    vec3 rgbA = 0.5 * (
        texture(uHdrTex, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(uHdrTex, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uHdrTex, uv + dir * -0.5).rgb +
        texture(uHdrTex, uv + dir *  0.5).rgb);

    float lumaB = lumaOf(acesToneMap(rgbB));
    if (lumaB < lumaMin || lumaB > lumaMax) {
        return rgbA;
    }
    return rgbB;
}

void main() {
    vec4 hdrRaw = texture(uHdrTex, vUV);
    vec3 hdr = sampleHdrFxaa(vUV);

    // Real-Time Screen-Space Reflections (SSR) on Wet Roads, Glossy Vehicles & Reflective Surfaces (v1.4.0)!
    if (uSsrEnabled == 1 && hdrRaw.a > 0.14) {
        float depthCenter = texture(uDepthTex, vUV).r;
        if (depthCenter < 0.9985) {
            vec4 ndc = vec4(vUV * 2.0 - 1.0, depthCenter * 2.0 - 1.0, 1.0);
            vec4 wpH = uInvViewProjComp * ndc;
            vec3 worldPos = wpH.xyz / max(wpH.w, 0.0001);

            vec3 dPdx = dFdx(worldPos);
            vec3 dPdy = dFdy(worldPos);
            vec3 N = normalize(cross(dPdx, dPdy));
            vec3 viewDir = normalize(worldPos - uCameraPosComp);
            if (dot(N, -viewDir) < 0.0) N = -N;

            vec3 R = normalize(reflect(viewDir, N));
            float fresnel = 0.25 + 0.75 * pow(clamp(1.0 - max(dot(N, -viewDir), 0.0), 0.0, 1.0), 3.0);

            float stepDist = 1.6;
            vec3 marchPos = worldPos + N * 0.08;
            for (int i = 0; i < 12; ++i) {
                marchPos += R * stepDist;
                stepDist *= 1.24;
                vec4 clip = uViewProjComp * vec4(marchPos, 1.0);
                if (clip.w <= 0.05) break;
                vec3 proj = (clip.xyz / clip.w) * 0.5 + 0.5;
                if (proj.x < 0.01 || proj.x > 0.99 || proj.y < 0.01 || proj.y > 0.99 || proj.z > 0.999) break;

                float sceneDepth = texture(uDepthTex, proj.xy).r;
                float rayLinZ = linearizeDepth(proj.z);
                float sceneLinZ = linearizeDepth(sceneDepth);
                float diff = rayLinZ - sceneLinZ;
                if (diff > 0.08 && diff < (0.85 + stepDist * 0.65)) {
                    vec2 edgeDist = min(proj.xy, 1.0 - proj.xy);
                    float edgeFade = smoothstep(0.0, 0.14, min(edgeDist.x, edgeDist.y));
                    vec3 reflCol = texture(uHdrTex, proj.xy).rgb;
                    float weight = clamp(hdrRaw.a * uSsrIntensity * fresnel * edgeFade * 0.72, 0.0, 0.72);
                    hdr = mix(hdr, reflCol, weight);
                    break;
                }
            }
        }
    }

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

    // Screen-Space Volumetric Sun Shafts / Crepuscular God-Rays (v1.3.0)!
    if (uGodRaysEnabled == 1 && uGodRayIntensity > 0.005) {
        vec2 delta = (uSunScreenPos - vUV) * (1.0 / 10.0) * 0.85;
        vec2 rayUV = vUV;
        float illum = 0.0;
        float decay = 1.0;
        for (int i = 0; i < 10; ++i) {
            rayUV += delta;
            if (rayUV.x >= 0.0 && rayUV.x <= 1.0 && rayUV.y >= 0.0 && rayUV.y <= 1.0) {
                float dSample = texture(uDepthTex, rayUV).r;
                illum += step(0.9985, dSample) * decay;
            }
            decay *= 0.84;
        }
        float sunProximity = exp(-length(vUV - uSunScreenPos) * 2.1);
        hdr += uSunColorComp * (illum / 10.0) * sunProximity * uGodRayIntensity;
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
