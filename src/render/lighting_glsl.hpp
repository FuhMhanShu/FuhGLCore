#pragma once

// Shared GLSL for the built-in lighting model (forward + deferred pipelines).
// Light data comes from a std140 UBO at binding 0; C++ mirror is render::LightBlock
// (include/fuhgl/render/light.hpp). Shadow sampling (F15) is mixed in via kShadowGlsl.

namespace fuhgl::render {

inline constexpr int kGlslMaxLights = 64;

constexpr const char* kLightingUniforms = R"GLSL(
uniform vec3 uCamPos;
uniform vec3 uAmbient;
uniform int uLightCount;
// NOTE: no `binding = 0` here — that layout qualifier needs GLSL 420. The block is
// bound to binding point 0 from C++ via glUniformBlockBinding after linking.
layout(std140) uniform LightBlock {
    vec4 lightPosIntensity[64]; // xyz = position, w = intensity
    vec4 lightColor[64];        // rgb = color
    vec4 lightDirType[64];      // xyz = direction, w = type (0 point, 1 directional, 2 spot)
    vec4 lightParams0[64];      // atten const/linear/quadratic, range
    vec4 lightParams1[64];      // cos(coneInner), cos(coneOuter)
};
)GLSL";

// F29: IBL samplers (set by the pipelines when SceneView::env is set; units
// kTextureUnitIrradiance/kTextureUnitPrefilter/kTextureUnitBrdfLut = 7/8/9).
constexpr const char* kEnvUniforms = R"GLSL(
uniform int uEnvEnabled;
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D uBrdfLut;
uniform float uPrefilterMaxLod;
uniform float uEnvIntensity;
)GLSL";

// F28: PBR (Cook-Torrance GGX metallic-roughness) + IBL ambient. The diffuse term is
// kD * albedo (no 1/pi — the irradiance bake carries no pi either, so direct and
// image-based ambient stay consistent).
constexpr const char* kPbrFuncs = R"GLSL(
float distributionGGX(float ndoth, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * d * d, 1e-7);
}
float geometrySchlickGGX(float ndotx, float roughness) {
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return ndotx / max(ndotx * (1.0 - k) + k, 1e-7);
}
float geometrySmith(float ndotv, float ndotl, float roughness) {
    return geometrySchlickGGX(ndotv, roughness) * geometrySchlickGGX(ndotl, roughness);
}
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// One direct light: radiance = lightColor * attenuation.
vec3 pbrDirect(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 radiance) {
    vec3 H = normalize(L + V);
    float ndotl = max(dot(N, L), 0.0);
    float ndotv = max(dot(N, V), 0.0);
    float ndoth = max(dot(N, H), 0.0);
    float hdotv = max(dot(H, V), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(hdotv, F0);
    float D = distributionGGX(ndoth, roughness);
    float G = geometrySmith(ndotv, ndotl, roughness);
    vec3 spec = (D * G * F) / max(4.0 * ndotv * ndotl, 1e-4);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo + spec) * radiance * ndotl;
}
// Ambient term: IBL when uEnvEnabled, otherwise the flat uAmbient.
vec3 pbrAmbient(vec3 albedo, float metallic, float roughness, float ao, vec3 N, vec3 V) {
    if (uEnvEnabled == 1) {
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 diffuse = irradiance * albedo;
        vec3 R = reflect(-V, N);
        vec3 prefiltered = textureLod(uPrefilterMap, R, roughness * uPrefilterMaxLod).rgb;
        vec2 brdf = texture(uBrdfLut, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefiltered * (F * brdf.x + vec3(brdf.y));
        return (kD * diffuse + specular) * ao * uEnvIntensity;
    }
    return albedo * uAmbient * ao;
}
)GLSL";

constexpr const char* kLightingLoop = R"GLSL(
// Returns direction TO the light (L) and its attenuation for light i.
void lightVector(int i, vec3 worldPos, out vec3 L, out float atten) {
    int t = int(lightDirType[i].w + 0.5);
    if (t == 1) { // directional
        L = normalize(-lightDirType[i].xyz);
        atten = lightPosIntensity[i].w;
    } else {
        vec3 toL = lightPosIntensity[i].xyz - worldPos;
        float d = length(toL);
        L = toL / max(d, 1e-5);
        vec4 pa = lightParams0[i];
        atten = lightPosIntensity[i].w / max(pa.x + pa.y * d + pa.z * d * d, 1e-5);
        if (pa.w > 0.0) atten *= clamp(1.0 - pow(clamp(d / pa.w, 0.0, 1.0), 4.0), 0.0, 1.0);
        if (t == 2) { // spot cone
            vec4 pc = lightParams1[i];
            float c = dot(-L, normalize(lightDirType[i].xyz));
            atten *= smoothstep(pc.y, pc.x, c); // pc.y = cos(outer) < pc.x = cos(inner)
        }
    }
}

// F28: PBR shading — per-light GGX direct term + IBL/flat ambient, shadows on the shadow light.
// FUHGL_SHADOW_KIT (F46, deferred lighting pass only) switches to the shadow-kit sampling:
// filter modes (poisson PCF / PCSS / VSM), cascade cross-fade, point-light cube shadows
// and screen-space contact shadows. Without the define the legacy path is unchanged.
vec3 shade(vec3 albedo, float metallic, float roughness, float ao, vec3 N, vec3 worldPos, vec3 V) {
    vec3 acc = pbrAmbient(albedo, metallic, roughness, ao, N, V);
    for (int i = 0; i < uLightCount && i < 64; ++i) {
        vec3 L;
        float atten;
        lightVector(i, worldPos, L, atten);
        float sh = 1.0;
#ifdef FUHGL_SHADOW_KIT
        if (uShadowEnabled == 1 && i == uShadowLightIndex)
            sh = mix(1.0, shadowFactorKit(worldPos, N, L), uShadowStrength);
        if (uPointShadows == 1 && i == uPointShadowLightIndex)
            sh *= mix(1.0, pointShadowFactor(worldPos, N, L), uPointShadowStrength);
        if (uContactShadows == 1 && i == uShadowLightIndex)
            sh *= mix(1.0, contactShadowTerm(worldPos, L), uContactShadowStrength);
#else
        if (uShadowEnabled == 1 && i == uShadowLightIndex)
            sh = mix(1.0, shadowFactor(worldPos, N, L), uShadowStrength);
#endif
        acc += pbrDirect(albedo, metallic, roughness, N, V, L, lightColor[i].rgb * atten) * sh;
    }
    return acc;
}
)GLSL";

// F46: extra uniforms for the shadow kit (only assembled into the deferred lighting pass
// behind FUHGL_SHADOW_KIT; texture units 11..15, see material.hpp).
constexpr const char* kShadowKitUniforms = R"GLSL(
uniform int uShadowFilterMode;      // 0 = legacy 3x3 PCF, 1 = poisson PCF, 2 = PCSS, 3 = VSM
uniform float uShadowCascadeBlend;  // CSM cross-fade fraction of a cascade span (0 = hard)
uniform float uShadowLightSize;     // PCSS world-space light size
uniform float uShadowWorldWidth;    // PCSS world width of one map layer
uniform float uMinVariance;         // VSM min variance
uniform int uPointShadows;          // 1 = sample uShadowCube for uPointShadowLightIndex
uniform int uPointShadowLightIndex;
uniform float uPointShadowStrength;
uniform vec3 uPointLightPos;
uniform float uPointShadowFar;
uniform int uContactShadows;        // 1 = screen-space contact shadows on the shadow light
uniform float uContactShadowStrength;
uniform float uContactShadowMaxDist;
uniform float uContactShadowThickness;
uniform int uContactShadowSteps;
uniform sampler2D uShadowMoments;           // VSM moments, 2D (unit kTextureUnitShadowMoments)
uniform sampler2DArray uShadowMomentsArray; // VSM moments, array (unit kTextureUnitShadowMomentsArray)
uniform samplerCube uShadowCube;            // point-light cube depth (unit kTextureUnitShadowCube)
uniform samplerCube uShadowCubeMoments;     // point-light cube moments (unit kTextureUnitShadowCubeMoments)
uniform sampler2D uSceneDepth;              // G-buffer depth for contact shadows (unit kTextureUnitSceneDepth)
uniform mat4 uProj;                         // camera projection (contact shadows)
uniform float uCamZNear, uCamZFar;          // camera near/far (contact shadows)
)GLSL";

// F46: shadow-kit sampling functions (mirror of the public kit in render/shadow_glsl.hpp,
// with uniform names matching kShadowUniforms/kShadowKitUniforms). Assemble after kShadowFunc.
constexpr const char* kShadowKitFuncs = R"GLSL(
vec2 skPoissonTap(int i, int count, float rotation) {
    float a = 2.39996323 * float(i) + rotation;
    float r = sqrt((float(i) + 0.5) / float(max(count, 1)));
    return vec2(cos(a), sin(a)) * r;
}
float skPcfPoisson2D(sampler2D map, vec2 uv, float z, float bias, float radiusTexels, int taps) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0));
    float sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        float d = texture(map, uv + skPoissonTap(i, taps, 0.0) * radiusTexels * texel).r;
        sum += (z - bias <= d) ? 1.0 : 0.0;
    }
    return sum / float(max(taps, 1));
}
float skPcfPoissonArray(sampler2DArray map, vec2 uv, float z, float bias, float radiusTexels, int layer, int taps) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0).xy);
    float sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        float d = texture(map, vec3(uv + skPoissonTap(i, taps, 0.0) * radiusTexels * texel, float(layer))).r;
        sum += (z - bias <= d) ? 1.0 : 0.0;
    }
    return sum / float(max(taps, 1));
}
float skBlocker2D(sampler2D map, vec2 uv, float z, float bias, float searchTexels, int taps) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0));
    float sum = 0.0, cnt = 0.0;
    for (int i = 0; i < taps; ++i) {
        float d = texture(map, uv + skPoissonTap(i, taps, 0.0) * searchTexels * texel).r;
        if (z - bias > d) { sum += d; cnt += 1.0; }
    }
    return cnt > 0.0 ? sum / cnt : z;
}
float skBlockerArray(sampler2DArray map, vec2 uv, float z, float bias, float searchTexels, int layer, int taps) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0).xy);
    float sum = 0.0, cnt = 0.0;
    for (int i = 0; i < taps; ++i) {
        float d = texture(map, vec3(uv + skPoissonTap(i, taps, 0.0) * searchTexels * texel, float(layer))).r;
        if (z - bias > d) { sum += d; cnt += 1.0; }
    }
    return cnt > 0.0 ? sum / cnt : z;
}
// PCSS: penumbra from the blocker distance, search radius from the world-space light size
float skPcss2D(sampler2D map, vec2 uv, float z, float bias) {
    float mapSize = float(textureSize(map, 0).x);
    float searchTexels = clamp(uShadowLightSize / max(uShadowWorldWidth, 1e-6) * mapSize, 1.0, 64.0);
    float blocker = skBlocker2D(map, uv, z, bias, searchTexels, 8);
    float penumbra = clamp(searchTexels * (z - blocker) / max(blocker, 1e-4), 1.0, 64.0);
    return skPcfPoisson2D(map, uv, z, bias, penumbra, 12);
}
float skPcssArray(sampler2DArray map, vec2 uv, float z, float bias, int layer) {
    float mapSize = float(textureSize(map, 0).x);
    float searchTexels = clamp(uShadowLightSize / max(uShadowWorldWidth, 1e-6) * mapSize, 1.0, 64.0);
    float blocker = skBlockerArray(map, uv, z, bias, searchTexels, layer, 8);
    float penumbra = clamp(searchTexels * (z - blocker) / max(blocker, 1e-4), 1.0, 64.0);
    return skPcfPoissonArray(map, uv, z, bias, penumbra, layer, 12);
}
// VSM: Chebyshev upper bound on an RGBA32F moments map
float skVsm2D(sampler2D moments, vec2 uv, float z) {
    vec2 m = texture(moments, uv).rg;
    if (z <= m.x) return 1.0;
    float var = max(m.y - m.x * m.x, 0.0);
    float d = z - m.x;
    float pMax = var / max(var + d * d, 1e-8);
    return clamp((pMax - uMinVariance) / max(1.0 - uMinVariance, 1e-6), 0.0, 1.0);
}
float skVsmArray(sampler2DArray moments, vec2 uv, float z, int layer) {
    vec2 m = texture(moments, vec3(uv, float(layer))).rg;
    if (z <= m.x) return 1.0;
    float var = max(m.y - m.x * m.x, 0.0);
    float d = z - m.x;
    float pMax = var / max(var + d * d, 1e-8);
    return clamp((pMax - uMinVariance) / max(1.0 - uMinVariance, 1e-6), 0.0, 1.0);
}
// cube-map shadow for the point light (poisson offsets in world units)
float skCubeShadow(samplerCube map, vec3 worldPos, float bias, float radiusWorld, int taps) {
    vec3 dir = worldPos - uPointLightPos;
    float z = length(dir) / max(uPointShadowFar, 1e-6);
    if (z > 1.0) return 1.0;
    vec3 up = abs(dir.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 a = normalize(cross(dir, up));
    vec3 b = cross(dir, a);
    float sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        vec2 t = skPoissonTap(i, taps, 0.7) * radiusWorld;
        vec3 d2 = normalize(dir + a * t.x + b * t.y);
        sum += (z - bias <= texture(map, d2).r) ? 1.0 : 0.0;
    }
    return sum / float(max(taps, 1));
}
float skCubeShadowMoments(samplerCube moments, vec3 worldPos) {
    vec3 dir = worldPos - uPointLightPos;
    float z = length(dir) / max(uPointShadowFar, 1e-6);
    if (z > 1.0) return 1.0;
    vec2 m = texture(moments, dir).rg;
    if (z <= m.x) return 1.0;
    float var = max(m.y - m.x * m.x, 0.0);
    float d = z - m.x;
    float pMax = var / max(var + d * d, 1e-8);
    return clamp((pMax - uMinVariance) / max(1.0 - uMinVariance, 1e-6), 0.0, 1.0);
}
// kit version of shadowFactor(): filter-mode branch + cascade cross-fade
float shadowFactorKit(vec3 worldPos, vec3 N, vec3 L) {
    float bias = max(0.0015 * (1.0 - dot(N, L)), 0.0004);
    if (uShadowMode == 1) {
        float viewDepth = -(uView * vec4(worldPos, 1.0)).z;
        int cascade = uCascadeCount - 1;
        for (int i = 0; i < uCascadeCount; ++i) {
            if (viewDepth <= uCascadeSplits[i + 1]) { cascade = i; break; }
        }
        vec4 lp = uLightVPs[cascade] * vec4(worldPos, 1.0);
        vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
        if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
        float sh;
        if (uShadowFilterMode == 3) sh = skVsmArray(uShadowMomentsArray, p.xy, p.z, cascade);
        else if (uShadowFilterMode == 2) sh = skPcssArray(uShadowMapArray, p.xy, p.z, bias, cascade);
        else if (uShadowFilterMode == 1) sh = skPcfPoissonArray(uShadowMapArray, p.xy, p.z, bias, 6.0, cascade, 12);
        else sh = pcfArray(p.xy, p.z, bias, cascade); // legacy 3x3
        if (uShadowCascadeBlend > 0.0 && cascade < uCascadeCount - 1) {
            float span = max(uCascadeSplits[cascade + 1] - uCascadeSplits[cascade], 1e-6);
            float t = clamp((viewDepth - (uCascadeSplits[cascade + 1] - uShadowCascadeBlend * span)) /
                                max(uShadowCascadeBlend * span, 1e-6), 0.0, 1.0);
            if (t > 0.0) {
                vec4 lp2 = uLightVPs[cascade + 1] * vec4(worldPos, 1.0);
                vec3 p2 = lp2.xyz / lp2.w * 0.5 + 0.5;
                if (p2.z <= 1.0 && p2.x >= 0.0 && p2.x <= 1.0 && p2.y >= 0.0 && p2.y <= 1.0) {
                    float sh2;
                    if (uShadowFilterMode == 3) sh2 = skVsmArray(uShadowMomentsArray, p2.xy, p2.z, cascade + 1);
                    else if (uShadowFilterMode == 2) sh2 = skPcssArray(uShadowMapArray, p2.xy, p2.z, bias, cascade + 1);
                    else if (uShadowFilterMode == 1) sh2 = skPcfPoissonArray(uShadowMapArray, p2.xy, p2.z, bias, 6.0, cascade + 1, 12);
                    else sh2 = pcfArray(p2.xy, p2.z, bias, cascade + 1);
                    sh = mix(sh, sh2, t);
                }
            }
        }
        return sh;
    }
    vec4 lp = uLightVP * vec4(worldPos, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    if (uShadowFilterMode == 3) return skVsm2D(uShadowMoments, p.xy, p.z);
    if (uShadowFilterMode == 2) return skPcss2D(uShadowMap, p.xy, p.z, bias);
    if (uShadowFilterMode == 1) return skPcfPoisson2D(uShadowMap, p.xy, p.z, bias, 6.0, 12);
    return pcf2D(p.xy, p.z, bias, uShadowMap);
}
// cube-map shadow of the point light
float pointShadowFactor(vec3 worldPos, vec3 N, vec3 L) {
    float bias = max(0.0015 * (1.0 - dot(N, L)), 0.0004);
    if (uShadowFilterMode == 3) return skCubeShadowMoments(uShadowCubeMoments, worldPos);
    return skCubeShadow(uShadowCube, worldPos, bias, 0.05, 8);
}
// screen-space contact shadow: march the view-space ray towards the light against uSceneDepth
float skLinearizeDepth(float d) {
    return 2.0 * uCamZNear * uCamZFar / max(uCamZFar + uCamZNear - (2.0 * d - 1.0) * (uCamZFar - uCamZNear), 1e-6);
}
float contactShadowTerm(vec3 worldPos, vec3 L) {
    vec3 viewPos = (uView * vec4(worldPos, 1.0)).xyz;
    vec3 lightViewPos = (uView * vec4(worldPos + L, 1.0)).xyz;
    vec3 dir = lightViewPos - viewPos;
    float dist = length(dir);
    if (dist < 1e-4) return 1.0;
    dir /= dist;
    float maxDist = min(uContactShadowMaxDist, dist);
    float acc = 1.0;
    float stepLen = maxDist / float(max(uContactShadowSteps, 1));
    for (int i = 1; i <= uContactShadowSteps; ++i) {
        vec3 sp = viewPos + dir * (float(i) * stepLen);
        vec4 c = uProj * vec4(sp, 1.0);
        if (c.w <= 0.0) continue;
        vec2 uv = c.xy / c.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || sp.z >= -uCamZNear) continue;
        float sceneDepth = skLinearizeDepth(texture(uSceneDepth, uv).r);
        float d = -sp.z;
        float vis = (sceneDepth >= d) ? 1.0 : clamp((d - sceneDepth) / max(uContactShadowThickness, 1e-5), 0.0, 1.0);
        acc = min(acc, vis);
        if (acc <= 0.0) break;
    }
    return acc;
}
)GLSL";

// F15: uniforms + PCF shadow sampling against the shadow map on texture unit uShadowMap.
// Assemble as: ... + kLightingUniforms + kShadowUniforms + kShadowFunc + kLightingLoop + ...
constexpr const char* kShadowUniforms = R"GLSL(
uniform sampler2D uShadowMap;            // single map (F15)
uniform sampler2DArray uShadowMapArray;  // CSM depth array (F30)
uniform mat4 uLightVP;                   // single-map VP
uniform mat4 uLightVPs[8];               // CSM: per-cascade VPs
uniform mat4 uView;                      // camera view (CSM view-space depth)
uniform int uShadowEnabled;
uniform int uShadowLightIndex;
uniform float uShadowStrength;
uniform int uShadowMode;                 // 0 = single 2D map, 1 = CSM array
uniform int uCascadeCount;
uniform float uCascadeSplits[9];         // count+1 view-space far planes, ascending
)GLSL";

constexpr const char* kShadowFunc = R"GLSL(
// 3x3 PCF (on top of the LINEAR-filtered depth texture) with slope-scaled bias.
// Returns 1.0 = fully lit, 0.0 = fully shadowed.
float pcf2D(vec2 uv, float z, float bias, sampler2D map) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0));
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float d = texture(map, uv + vec2(float(x), float(y)) * texel).r;
            sum += (z - bias <= d) ? 1.0 : 0.0;
        }
    return sum / 9.0;
}
float pcfArray(vec2 uv, float z, float bias, int layer) {
    vec2 texel = 1.0 / vec2(textureSize(uShadowMapArray, 0).xy);
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float d = texture(uShadowMapArray, vec3(uv + vec2(float(x), float(y)) * texel, float(layer))).r;
            sum += (z - bias <= d) ? 1.0 : 0.0;
        }
    return sum / 9.0;
}
float shadowFactor(vec3 worldPos, vec3 N, vec3 L) {
    float bias = max(0.0015 * (1.0 - dot(N, L)), 0.0004);
    if (uShadowMode == 1) {
        // CSM (F30): pick the cascade by view-space depth, sample its array layer
        float viewDepth = -(uView * vec4(worldPos, 1.0)).z;
        int cascade = uCascadeCount - 1;
        for (int i = 0; i < uCascadeCount; ++i) {
            if (viewDepth <= uCascadeSplits[i + 1]) { cascade = i; break; }
        }
        vec4 lp = uLightVPs[cascade] * vec4(worldPos, 1.0);
        vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
        if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
        return pcfArray(p.xy, p.z, bias, cascade);
    }
    vec4 lp = uLightVP * vec4(worldPos, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    return pcf2D(p.xy, p.z, bias, uShadowMap);
}
)GLSL";

} // namespace fuhgl::render
