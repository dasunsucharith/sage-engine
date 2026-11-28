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

    // Outputs linear HDR color (alpha = 0.0 -> non-reflective sky for SSR pass)
    FragColor = vec4(sky, 0.0);
}
