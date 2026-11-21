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
vec3 shade(vec3 albedo, float metallic, float roughness, float ao, vec3 N, vec3 worldPos, vec3 V) {
    vec3 acc = pbrAmbient(albedo, metallic, roughness, ao, N, V);
    for (int i = 0; i < uLightCount && i < 64; ++i) {
        vec3 L;
        float atten;
        lightVector(i, worldPos, L, atten);
        float sh = 1.0;
        if (uShadowEnabled == 1 && i == uShadowLightIndex)
            sh = mix(1.0, shadowFactor(worldPos, N, L), uShadowStrength);
        acc += pbrDirect(albedo, metallic, roughness, N, V, L, lightColor[i].rgb * atten) * sh;
    }
    return acc;
}
)GLSL";

// F15: uniforms + PCF shadow sampling against the shadow map on texture unit uShadowMap.
// Assemble as: ... + kLightingUniforms + kShadowUniforms + kShadowFunc + kLightingLoop + ...
constexpr const char* kShadowUniforms = R"GLSL(
uniform sampler2D uShadowMap;
uniform mat4 uLightVP;
uniform int uShadowEnabled;
uniform int uShadowLightIndex;
uniform float uShadowStrength;
)GLSL";

constexpr const char* kShadowFunc = R"GLSL(
// 3x3 PCF (on top of the LINEAR-filtered depth texture) with slope-scaled bias.
// Returns 1.0 = fully lit, 0.0 = fully shadowed.
float shadowFactor(vec3 worldPos, vec3 N, vec3 L) {
    vec4 lp = uLightVP * vec4(worldPos, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = max(0.0015 * (1.0 - dot(N, L)), 0.0004);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float d = texture(uShadowMap, p.xy + vec2(float(x), float(y)) * texel).r;
            sum += (p.z - bias <= d) ? 1.0 : 0.0;
        }
    return sum / 9.0;
}
)GLSL";

} // namespace fuhgl::render
