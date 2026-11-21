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

vec3 shade(vec3 albedo, vec3 N, vec3 worldPos, vec3 V) {
    vec3 acc = albedo * uAmbient;
    for (int i = 0; i < uLightCount && i < 64; ++i) {
        vec3 L;
        float atten;
        lightVector(i, worldPos, L, atten);
        float sh = 1.0;
        if (uShadowEnabled == 1 && i == uShadowLightIndex)
            sh = mix(1.0, shadowFactor(worldPos, N, L), uShadowStrength);
        float diff = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.4;
        acc += (albedo * diff + vec3(spec)) * lightColor[i].rgb * atten * sh;
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
