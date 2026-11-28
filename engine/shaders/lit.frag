#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;
in vec4 vInstColorEmissive;

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
uniform int uTerrainMode;
// Data-driven terrain (uTerrainMode == 2): up to 6 height layers packed as 2 x vec4 each:
//   uTLayers[2i]   = (linear color.rgb, roughness)
//   uTLayers[2i+1] = (startHeight, blend, 0, 0)
uniform vec4 uTLayers[12];
uniform int uTLayerCount;
uniform vec4 uTCliffColor;   // rgb
uniform vec4 uTSlopeSnow;    // (cliffSlopeStart, cliffSlopeEnd, snowStartHeight, snowEndHeight)
uniform vec4 uTSnowColor;    // rgb, a = enabled (1/0)
uniform float uWetness;
uniform int uCloudShadowsEnabled;

uniform int uShadowsEnabled;
uniform mat4 uLightViewProj;
uniform mat4 uLightViewProjMid;
uniform mat4 uLightViewProjFar;
uniform sampler2DShadow uShadowMap;
uniform sampler2DShadow uShadowMapMid;
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
// Clustered light culling: the screen is split into tiles AND depth slices; each cluster
// stores a 16-bit mask of the lights that can reach it (stored exactly in a float).
// The mask texture stacks the depth slices vertically: row = tileY + slice * tilesY.
uniform sampler2D uLightTileMask;
uniform int uLightTilesEnabled;
uniform int uLightTileSize;
uniform int uLightTilesY;
uniform int uLightSlices;
uniform vec3 uCameraForward;
uniform float uClusterNear;
uniform float uClusterLogScale;   // slices / log(far / near)

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

float sampleCascadePcf(sampler2DShadow smap, mat4 lvp, vec3 worldPos, vec3 N, vec3 L, float biasScale, float texelStep, out float edgeFade) {
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
            vec2 offset = vec2(float(x), float(y)) * texelStep * 1.35;
            visibility += texture(smap, vec3(proj.xy + offset, proj.z - 0.0005 * biasScale));
        }
    }
    return visibility / 9.0;
}

float computeShadowVisibility(vec3 worldPos, vec3 N, vec3 L) {
    if (uShadowsEnabled == 0) {
        return 1.0;
    }

    // 3-Split Cascaded Shadow Maps (3-CSM v1.5.0): 24m Near (2048px) -> 92m Mid (1024px) -> 255m Mountain Far (1024px)!
    float nearFade = 0.0;
    float nearVis = sampleCascadePcf(uShadowMap, uLightViewProj, worldPos, N, L, 0.72, uShadowTexelSize, nearFade);
    if (nearFade > 0.99) {
        return nearVis;
    }
    float midFade = 0.0;
    float midVis = sampleCascadePcf(uShadowMapMid, uLightViewProjMid, worldPos, N, L, 1.65, uShadowTexelSize * 2.0, midFade);
    if (midFade > 0.99) {
        return mix(midVis, nearVis, nearFade);
    }
    float farFade = 0.0;
    float farVis = sampleCascadePcf(uShadowMapFar, uLightViewProjFar, worldPos, N, L, 3.30, uShadowTexelSize * 2.0, farFade);
    float farResult = mix(1.0, farVis, farFade);
    float midResult = mix(farResult, midVis, midFade);
    return mix(midResult, nearVis, nearFade);
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
    vec3 rawT = Q1 * st2.t - Q2 * st1.t;
    float tLenSq = dot(rawT, rawT);
    if (tLenSq < 1e-10) {
        return N;
    }
    vec3 T = rawT * inversesqrt(tLenSq);
    T = T - N * dot(N, T);
    float tOrthoSq = dot(T, T);
    if (tOrthoSq < 1e-10) {
        return N;
    }
    T *= inversesqrt(tOrthoSq);
    vec3 B = -cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 res = TBN * mapN;
    float rSq = dot(res, res);
    return (rSq > 1e-10) ? (res * inversesqrt(rSq)) : N;
}

float terrainFbm(vec2 p) {
    float v = 0.0;
    float a = 0.50;
    for (int i = 0; i < 3; ++i) {
        v += a * groundNoise(p);
        p = p * 2.13 + vec2(17.3, 9.1);
        a *= 0.50;
    }
    return v;
}

void main() {
    vec3 albedo = uBaseColor * vInstColorEmissive.rgb;
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

    vec3 N = normalize(vWorldNormal);
    if (!gl_FrontFacing) {
        N = -N;
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
    } else if (uIsGround == 2) {
        // Multi-Biome 3D Heightmap Terrain PBR Splatting + Multi-Scale Micro-Normal Detail (v1.6.0)!
        float nMacro = terrainFbm(vWorldPos.xz * 0.028);
        float nMed   = terrainFbm(vWorldPos.xz * 0.16);
        float nFine  = groundNoise(vWorldPos.xz * 1.85);
        float nMicro = groundNoise(vWorldPos.xz * 6.40);
        float detail = (nMacro * 0.36 + nMed * 0.30 + nFine * 0.22 + nMicro * 0.12) - 0.5;
        vec3 Ngeom = normalize(vWorldPos.y > -99.0 ? normalize(vWorldNormal) : vec3(0, 1, 0));
        float slope = 1.0 - clamp(Ngeom.y, 0.0, 1.0);

        // Compute multi-scale procedural micro-bump normal gradient (crisp sand, meadow soil & craggy granite!)
        float bEps = 0.16;
        float bFreq = mix(0.85, 1.45, smoothstep(0.12, 0.35, slope));
        float b0 = terrainFbm(vWorldPos.xz * bFreq) * 0.60 + groundNoise(vWorldPos.xz * 4.8) * 0.40;
        float bx = terrainFbm((vWorldPos.xz + vec2(bEps, 0.0)) * bFreq) * 0.60 + groundNoise((vWorldPos.xz + vec2(bEps, 0.0)) * 4.8) * 0.40;
        float bz = terrainFbm((vWorldPos.xz + vec2(0.0, bEps)) * bFreq) * 0.60 + groundNoise((vWorldPos.xz + vec2(0.0, bEps)) * 4.8) * 0.40;
        float bumpAmp = mix(0.42, 1.05, smoothstep(0.08, 0.36, slope));
        N = normalize(Ngeom + vec3((b0 - bx) * bumpAmp, 0.0, (b0 - bz) * bumpAmp));

        if (uTerrainMode == 2) {
            // DATA-DRIVEN TERRAIN: colors, heights & slopes all come from the scene JSON "terrain" block.
            float hN = vWorldPos.y + detail * 2.4;
            vec3 col = uTLayers[0].rgb;
            float lr = uTLayers[0].a;
            for (int i = 1; i < 6; ++i) {
                if (i >= uTLayerCount) break;
                vec4 cr = uTLayers[i * 2];
                vec4 hb = uTLayers[i * 2 + 1];
                float t = smoothstep(hb.x - hb.y * 0.5, hb.x + hb.y * 0.5, hN);
                col = mix(col, cr.rgb, t);
                lr = mix(lr, cr.a, t);
            }
            col *= (1.0 + detail * 0.42);
            float cliffT = smoothstep(uTSlopeSnow.x, uTSlopeSnow.y, slope);
            float rockCrag = terrainFbm(vec2(vWorldPos.x * 0.07 + vWorldPos.y * 0.11, vWorldPos.z * 0.07 - vWorldPos.y * 0.08));
            vec3 cliffCol = uTCliffColor.rgb * (0.70 + 0.60 * rockCrag) * (1.0 + detail * 0.30);
            col = mix(col, cliffCol, cliffT);
            lr = mix(lr, 0.90, cliffT);
            float snowMask = 0.0;
            if (uTSnowColor.a > 0.5) {
                snowMask = smoothstep(uTSlopeSnow.z, uTSlopeSnow.w, vWorldPos.y + detail * 4.5) * smoothstep(0.38, 0.70, Ngeom.y);
                col = mix(col, uTSnowColor.rgb * (0.95 + 0.07 * nMicro), snowMask);
                lr = mix(lr, 0.62, snowMask);
            }
            albedo = col;
            rough = clamp(lr + detail * 0.12, 0.20, 0.98);
            metal = 0.02;
            if (uWetness > 0.01) {
                albedo *= (1.0 - 0.30 * uWetness);
                rough = mix(rough, 0.18, uWetness * 0.60);
            }
        } else if (uTerrainMode == 1) {
            // ORGANIC PHOTOREALISTIC WILDERNESS ISLAND (Rich earthy woodland tones + fractured granite!)
            float hN = vWorldPos.y + detail * 2.4;
            vec3 wetSand    = vec3(0.22, 0.18, 0.12) * (1.0 + detail * 0.32);
            vec3 goldSand   = vec3(0.52, 0.43, 0.28) * (1.0 + detail * 0.34);
            vec3 dryGrass   = vec3(0.14, 0.20, 0.08) * (1.0 + detail * 0.44);
            vec3 lushForest = mix(vec3(0.072, 0.150, 0.052), vec3(0.140, 0.222, 0.074), nMacro) * (1.0 + detail * 0.48);
            vec3 pineHumus  = vec3(0.115, 0.095, 0.062) * (1.0 + detail * 0.44);
            vec3 alpineMoss = vec3(0.16, 0.15, 0.10) * (1.0 + detail * 0.42);
            // Non-repeating 3D-warped fractured alpine granite (ZERO horizontal banding!)
            float rockCragA = terrainFbm(vec2(vWorldPos.x * 0.07 + vWorldPos.y * 0.11, vWorldPos.z * 0.07 - vWorldPos.y * 0.08));
            float rockCragB = terrainFbm(vec2(vWorldPos.x * 0.24 - vWorldPos.y * 0.19, vWorldPos.z * 0.24 + vWorldPos.y * 0.17));
            vec3 cliffRock  = mix(vec3(0.11, 0.108, 0.105), vec3(0.26, 0.255, 0.242), clamp(rockCragA * 0.58 + rockCragB * 0.42, 0.0, 1.0));
            vec3 snowCap    = vec3(0.84, 0.88, 0.94) * (0.95 + 0.07 * nMicro);

            vec3 col = mix(wetSand, goldSand, smoothstep(-0.2, 1.3, hN));
            col = mix(col, dryGrass, smoothstep(2.2, 4.0, hN));
            col = mix(col, lushForest, smoothstep(3.6, 6.2, hN));
            col = mix(col, pineHumus, smoothstep(0.46, 0.78, nMed) * smoothstep(4.2, 9.5, hN) * 0.55);
            col = mix(col, alpineMoss, smoothstep(15.5, 22.0, hN));
            col = mix(col, cliffRock, smoothstep(0.12, 0.30, slope));
            float snowMask = smoothstep(27.5, 35.5, vWorldPos.y + detail * 4.5) * smoothstep(0.38, 0.70, Ngeom.y);
            albedo = mix(col, snowCap, snowMask);

            float shoreWet = 1.0 - smoothstep(-0.2, 1.35, vWorldPos.y);
            rough = clamp(mix(0.90, 0.62, snowMask) - shoreWet * 0.45 + detail * 0.12, 0.20, 0.98);
            metal = shoreWet * 0.16;
            if (uWetness > 0.01) {
                albedo *= (1.0 - 0.32 * uWetness);
                rough = mix(rough, 0.16, uWetness * 0.65);
            }
        } else {
            // Stunt City 4-Biome Open World Splatting (Alpine Grass, Red Rock Strata, Sand, Cliff Rock & Snow Peaks!)
            float bxCity = smoothstep(-28.0, 28.0, vWorldPos.x);
            float bzCity = smoothstep(-28.0, 28.0, vWorldPos.z);

            vec3 alpineGrass = vec3(0.11, 0.26, 0.10) * (1.0 + detail * 0.32);
            vec3 parkGrass   = vec3(0.14, 0.28, 0.13) * (1.0 + detail * 0.28);
            vec3 coastSand   = vec3(0.56, 0.46, 0.28) * (1.0 + detail * 0.24);
            float strata     = 0.86 + 0.14 * sin(vWorldPos.y * 2.1 + nMacro * 3.5);
            vec3 desertRock  = vec3(0.52, 0.24, 0.13) * strata * (1.0 + detail * 0.26);

            vec3 northCol = mix(parkGrass, alpineGrass, bxCity);
            vec3 southCol = mix(desertRock, coastSand, bxCity);
            vec3 baseBiome = mix(southCol, northCol, bzCity);

            vec3 cliffGranite = mix(vec3(0.24, 0.22, 0.21), vec3(0.42, 0.21, 0.13), (1.0 - bxCity) * (1.0 - bzCity)) * (1.0 + detail * 0.35);
            albedo = mix(baseBiome, cliffGranite, smoothstep(0.14, 0.38, slope));

            float snowMask = smoothstep(16.0, 25.0, vWorldPos.y + detail * 4.5) * smoothstep(-35.0, 35.0, vWorldPos.z) * smoothstep(0.42, 0.72, Ngeom.y);
            albedo = mix(albedo, vec3(0.86, 0.90, 0.96), snowMask);

            rough = clamp(mix(0.88, 0.62, snowMask) + detail * 0.12, 0.35, 0.96);
            metal = 0.02;
            if (uWetness > 0.01) {
                albedo *= (1.0 - 0.28 * uWetness);
                rough = mix(rough, 0.24, uWetness * 0.55);
            }
        }
    }

    if (uHasNormalTex == 1) {
        N = perturbNormalFromMap(N, vWorldPos, vUV);
    }
    vec3 L = normalize(uSunDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metal);

    float shadowVis = computeShadowVisibility(vWorldPos, N, L);

    // Drifting FBM Cloud Shadows across the 560m open world & 3D mountain ridges!
    if (uCloudShadowsEnabled == 1) {
        float c1 = groundNoise(vWorldPos.xz * 0.0085 + vec2(uTime * 0.022, uTime * 0.011));
        float c2 = groundNoise(vWorldPos.xz * 0.019 - vec2(uTime * 0.014, -uTime * 0.008));
        float cloudCover = smoothstep(0.48, 0.78, c1 * 0.65 + c2 * 0.35);
        shadowVis *= mix(1.0, 0.78, cloudCover);
    }

    // Direct Sun Radiance using true Cook-Torrance GGX PBR!
    vec3 directSun = evaluatePbrLight(N, V, L, uSunColor * 3.35, albedo, metal, rough, F0) * shadowVis;

    // Rich Multi-Bounce Hemisphere Sky & Ground IBL (prevents dark/black shadow faces on characters!)
    float hemiT = N.y * 0.5 + 0.5;
    vec3 skyFill = mix(uSkyHorizon, uSkyZenith, 0.32);
    vec3 groundBounce = mix(uGroundAmbient, uSkyHorizon * 0.55, 0.35);
    vec3 hemiAmbient = mix(groundBounce, skyFill, hemiT) * albedo * (1.0 - 0.55 * metal) * (uAmbientIntensity * 1.68);

    vec3 R = reflect(-V, N);
    vec3 envSky = mix(uSkyHorizon, uSkyZenith, clamp(R.y * 1.2 + 0.15, 0.0, 1.0));
    vec3 Fenv = fresnelSchlick(max(dot(N, V), 0.0), F0);
    float envGloss = (1.0 - rough) * (1.0 - rough);
    vec3 iblSpec = envSky * Fenv * (0.04 + 0.92 * envGloss);

    // Subtle fresnel rim-light accent on 3D characters & vehicles (modulated by sky/gloss so matte cloth never glows white!)
    float rimTerm = pow(1.0 - max(dot(N, V), 0.0), 4.2) * (uIsGround == 0 ? 1.0 : 0.0);
    vec3 rimGlow = mix(uSkyHorizon, uSunColor, 0.45) * rimTerm * (0.06 + 0.18 * envGloss);

    // Emissive values exceed 1.0 in HDR so Bloom picks up headlights, neon & beacons!
    vec3 baseEmissive = (vInstColorEmissive.a > 0.5) ? (albedo * 0.90) : uEmissive;
    vec3 emissiveTerm = baseEmissive * 1.35;
    if (uPulse > 0.5) {
        float p = 0.80 + 0.20 * sin(uTime * 5.0);
        emissiveTerm *= p * 1.30;
    }

    vec3 color = hemiAmbient + directSun + iblSpec + rimGlow + emissiveTerm;

    // Real-time Dynamic Point Lights evaluated with true GGX PBR!
    // Tiled culling: this pixel's tile lists which lights can possibly reach it, so lights
    // that are far away on screen are skipped without any per-light work.
    uint tileLights = 0xFFFFu;
    if (uLightTilesEnabled == 1) {
        float viewZ = max(dot(vWorldPos - uCameraPos, uCameraForward), uClusterNear);
        int slice = clamp(int(floor(log(viewZ / uClusterNear) * uClusterLogScale)), 0, uLightSlices - 1);
        ivec2 tile = ivec2(gl_FragCoord.xy) / uLightTileSize;
        tileLights = uint(texelFetch(uLightTileMask, ivec2(tile.x, tile.y + slice * uLightTilesY), 0).r + 0.5);
    }
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (i >= uPointLightCount || tileLights == 0u) break;
        if ((tileLights & (1u << uint(i))) == 0u) continue;
        vec3 lp = uPointLightPosRadius[i].xyz;
        float lr = max(uPointLightPosRadius[i].w, 0.5);
        vec3 lc = uPointLightColorInt[i].rgb * uPointLightColorInt[i].w;
        vec3 toL = lp - vWorldPos;
        float d = length(toL);
        if (d < lr) {
            vec3 plDir = toL / max(d, 0.001);
            float att = 1.0 - (d / lr);
            att = att * att;
            color += evaluatePbrLight(N, V, plDir, lc * att * 2.2, albedo, metal, rough, F0);
        }
    }

    // Atmospheric Aerial Perspective + Mie Solar Scattering Glow in distance fog!
    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    float sunInscatter = pow(max(dot(-V, L), 0.0), 6.0);
    vec3 fogColor = mix(uSkyHorizon, uSunColor * 1.12, sunInscatter * 0.36);
    color = mix(color, fogColor, fogFactor * fogFactor * 0.82);

    // Output sanitized linear HDR + per-pixel reflectivity in alpha for Screen-Space Reflections!
    float wetBoost = (uIsGround == 1) ? (uWetness * 0.85) : 0.0;
    float reflectivity = clamp((1.0 - rough) * (0.14 + 0.86 * metal + wetBoost), 0.0, 1.0);
    if (uIsGround == 2) reflectivity *= 0.16;
    FragColor = vec4(clamp(color, vec3(0.0), vec3(48.0)), reflectivity);
}
