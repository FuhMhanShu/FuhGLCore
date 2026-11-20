#pragma once

// Shared GLSL for the built-in lighting model (forward + deferred pipelines).
// Light data comes from a std140 UBO at binding 0; C++ mirror is render::LightBlock.

namespace fuhgl::render {

inline constexpr int kGlslMaxLights = 64;

constexpr const char* kLightingUniforms = R"GLSL(
uniform vec3 uCamPos;
uniform vec3 uAmbient;
uniform int uLightCount;
layout(std140, binding = 0) uniform LightBlock {
    vec4 lightPosIntensity[64]; // xyz = position, w = intensity
    vec4 lightColor[64];        // rgb = color
};
)GLSL";

constexpr const char* kLightingLoop = R"GLSL(
vec3 shade(vec3 albedo, vec3 N, vec3 worldPos, vec3 V) {
    vec3 acc = albedo * uAmbient;
    for (int i = 0; i < uLightCount && i < 64; ++i) {
        vec3 L = lightPosIntensity[i].xyz - worldPos;
        float d = length(L);
        L /= max(d, 1e-5);
        float atten = lightPosIntensity[i].w / (1.0 + 0.35 * d + 0.12 * d * d);
        float diff = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.4;
        acc += (albedo * diff + vec3(spec)) * lightColor[i].rgb * atten;
    }
    return acc;
}
)GLSL";

// C++ mirror of the std140 LightBlock (vec4 arrays, 16-byte stride).
struct LightBlock {
    math::Vec4 posIntensity[kGlslMaxLights];
    math::Vec4 color[kGlslMaxLights];
};

} // namespace fuhgl::render
