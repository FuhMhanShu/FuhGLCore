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
        float diff = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.4;
        acc += (albedo * diff + vec3(spec)) * lightColor[i].rgb * atten;
    }
    return acc;
}
)GLSL";

} // namespace fuhgl::render
