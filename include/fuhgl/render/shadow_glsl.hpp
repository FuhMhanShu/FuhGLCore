#pragma once

/// @file shadow_glsl.hpp
/// @brief F45 — public, composable GLSL snippets for advanced shadow sampling.
///
/// Compose your own shaders from these pieces (GLSL 330):
///   fragment:  "#version 330 core\n" + "out vec4 FragColor;\n"
///              + kSkUniforms + kSkFuncs + "void main() { ... }"
///   depth pass (hard shadows): kSkDepthVert + kSkDepthFrag (complete programs)
///   depth pass (VSM moments):  kSkDepthVert + kSkMomentsFrag
///
/// The kit covers: hard depth compare, poisson-disk PCF (2D + array), PCSS
/// (blocker search + penumbra-sized filter), VSM (Chebyshev, 2D + array + cube),
/// cube-map sampling for point lights, cascade selection with cross-fade blending,
/// slope-scaled + normal-offset bias, and a screen-space raymarched contact shadow.
/// All samplers/params are plain uniforms — bind what you use.

namespace fuhgl::render {

// --- depth-pass programs (complete, include #version) ------------------------------

/// @brief Depth-pass vertex shader (instancing-aware, like the pipelines' internal one).
inline constexpr const char* kSkDepthVert = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in mat4 aInstanceModel; // instancing (unused when uInstanced == 0)
uniform mat4 uModel;
uniform mat4 uLightVP;
uniform int uInstanced;
void main() {
    mat4 model = (uInstanced == 1) ? (uModel * aInstanceModel) : uModel;
    gl_Position = uLightVP * model * vec4(aPos, 1.0);
}
)GLSL";

/// @brief Depth-pass fragment shader for hard shadows (depth only, no color output).
inline constexpr const char* kSkDepthFrag = R"GLSL(#version 330 core
void main() {}
)GLSL";

/// @brief Depth-pass fragment shader for VSM: writes moments (d, d^2) with
/// depth-derivative padding into an RGBA32F color attachment.
inline constexpr const char* kSkMomentsFrag = R"GLSL(#version 330 core
out vec4 oMoments;
void main() {
    float d = gl_FragCoord.z;
    float dx = dFdx(d);
    float dy = dFdy(d);
    float m2 = d * d + 0.25 * (dx * dx + dy * dy);
    oMoments = vec4(d, m2, 0.0, 1.0);
}
)GLSL";

// --- sampling kit (no #version — concatenate after your own prologue) --------------

/// @brief All uniforms the kit functions may use (unused ones are optimized out).
inline constexpr const char* kSkUniforms = R"GLSL(
uniform sampler2D uShadowMap;             // hard depth, 2D
uniform sampler2D uShadowMoments;         // VSM moments, 2D (RGBA32F)
uniform sampler2DArray uShadowMapArray;   // hard depth, array (atlas / cascades)
uniform sampler2DArray uShadowMomentsArray; // VSM moments, array
uniform samplerCube uShadowCube;          // hard depth, cube (point light)
uniform samplerCube uShadowCubeMoments;   // VSM moments, cube
uniform mat4 uLightVP;                    // single 2D map VP
uniform mat4 uLightVPs[8];                // per-layer / per-cascade VPs
uniform vec3 uLightPos;                   // point-light position (cube maps)
uniform float uShadowFar;                 // point-light far plane (cube depth normalize)
uniform mat4 uView;                       // camera view (view-space depth for cascades)
uniform int uCascadeCount;
uniform float uCascadeSplits[9];          // count+1 view-space far planes, ascending
uniform float uShadowBias;                // slope-scaled bias base
uniform float uNormalOffset;              // normal-offset bias in world units
uniform float uLightSize;                 // world-space light size (PCSS penumbra)
uniform float uShadowWorldWidth;          // world width covered by the map (PCSS search)
uniform float uMinVariance;               // VSM min variance (light-bleeding clamp)
uniform sampler2D uSceneDepth;            // contact shadows: scene depth texture
uniform mat4 uProj;                       // contact shadows: camera projection
uniform float uCsZNear, uCsZFar;          // contact shadows: camera near/far
)GLSL";

/// @brief Kit functions: projection, bias helpers, poisson PCF, PCSS, VSM, cube,
/// cascade select + blend, contact shadow. Depends on kSkUniforms.
inline constexpr const char* kSkFuncs = R"GLSL(
// projects worldPos into light clip space -> [0,1]^3 (uv.xy, depth)
vec3 shadowNdc(mat4 vp, vec3 worldPos) {
    vec4 lp = vp * vec4(worldPos, 1.0);
    return lp.xyz / lp.w * 0.5 + 0.5;
}
bool shadowInBounds(vec3 p) {
    return p.z <= 1.0 && p.x >= 0.0 && p.x <= 1.0 && p.y >= 0.0 && p.y <= 1.0;
}
// normal-offset bias: shifts the sampling position along the surface normal
vec3 shadowWorldPos(vec3 worldPos, vec3 N, float normalOffset) {
    return worldPos + N * normalOffset;
}
// slope-scaled bias (the classic term used by the built-in pipelines)
float shadowBiasSlope(vec3 N, vec3 L, float maxBias, float minBias) {
    return max(maxBias * (1.0 - dot(N, L)), minBias);
}
// Vogel disk tap i of `count`, rotated by `rotation` (radians) — deterministic
vec2 shadowPoissonTap(int i, int count, float rotation) {
    float a = 2.39996323 * float(i) + rotation;
    float r = sqrt((float(i) + 0.5) / float(max(count, 1)));
    return vec2(cos(a), sin(a)) * r;
}
// poisson-disk PCF on a 2D depth map; radiusTexels = kernel radius in texels
float pcfPoisson2D(sampler2D map, vec2 uv, float z, float bias, float radiusTexels, int taps, float rotation) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0));
    float sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        vec2 off = shadowPoissonTap(i, taps, rotation) * radiusTexels * texel;
        float d = texture(map, uv + off).r;
        sum += (z - bias <= d) ? 1.0 : 0.0;
    }
    return sum / float(max(taps, 1));
}
float pcfPoissonArray(sampler2DArray map, vec2 uv, float z, float bias, float radiusTexels, int layer, int taps, float rotation) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0).xy);
    float sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        vec2 off = shadowPoissonTap(i, taps, rotation) * radiusTexels * texel;
        float d = texture(map, vec3(uv + off, float(layer))).r;
        sum += (z - bias <= d) ? 1.0 : 0.0;
    }
    return sum / float(max(taps, 1));
}
// PCSS blocker search: average depth of the occluders around uv (light-space z);
// returns z when no blocker is found (-> zero penumbra)
float pcssBlocker2D(sampler2D map, vec2 uv, float z, float bias, float searchTexels, int taps, float rotation) {
    vec2 texel = 1.0 / vec2(textureSize(map, 0));
    float sum = 0.0, cnt = 0.0;
    for (int i = 0; i < taps; ++i) {
        vec2 off = shadowPoissonTap(i, taps, rotation) * searchTexels * texel;
        float d = texture(map, uv + off).r;
        if (z - bias > d) { sum += d; cnt += 1.0; }
    }
    return cnt > 0.0 ? sum / cnt : z;
}
// full PCSS: penumbra width from blocker distance, search radius from the world-space
// light size; z = light-space depth of the receiver (ortho: linear in [0,1])
float pcss2D(sampler2D map, vec2 uv, float z, float bias, float lightSize, float worldWidth,
             int blockerTaps, int filterTaps, float rotation) {
    float mapSize = float(textureSize(map, 0).x);
    float searchTexels = clamp(lightSize / max(worldWidth, 1e-6) * mapSize, 1.0, 64.0);
    float blocker = pcssBlocker2D(map, uv, z, bias, searchTexels, blockerTaps, rotation);
    float penumbraTexels = searchTexels * (z - blocker) / max(blocker, 1e-4);
    return pcfPoisson2D(map, uv, z, bias, clamp(penumbraTexels, 1.0, 64.0), filterTaps, rotation);
}
// VSM (Chebyshev upper bound) on an RGBA32F moments map; minVariance clamps light bleeding
float vsm2D(sampler2D moments, vec2 uv, float z, float minVariance) {
    vec2 m = texture(moments, uv).rg;
    if (z <= m.x) return 1.0;
    float var = max(m.y - m.x * m.x, 0.0);
    float d = z - m.x;
    float pMax = var / max(var + d * d, 1e-8);
    return clamp((pMax - minVariance) / max(1.0 - minVariance, 1e-6), 0.0, 1.0);
}
float vsmArray2D(sampler2DArray moments, vec2 uv, float z, float minVariance, int layer) {
    vec2 m = texture(moments, vec3(uv, float(layer))).rg;
    if (z <= m.x) return 1.0;
    float var = max(m.y - m.x * m.x, 0.0);
    float d = z - m.x;
    float pMax = var / max(var + d * d, 1e-8);
    return clamp((pMax - minVariance) / max(1.0 - minVariance, 1e-6), 0.0, 1.0);
}
// cube-map shadow for point lights; radiusWorld = kernel radius in world units at the receiver
float cubeShadow(samplerCube map, vec3 worldPos, vec3 lightPos, float farPlane, float bias, float radiusWorld, int taps) {
    vec3 dir = worldPos - lightPos;
    float z = length(dir) / max(farPlane, 1e-6);
    if (z > 1.0) return 1.0;
    vec3 up = abs(dir.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 a = normalize(cross(dir, up));
    vec3 b = cross(dir, a);
    float sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        vec2 t = shadowPoissonTap(i, taps, 0.7) * radiusWorld;
        vec3 d2 = normalize(dir + a * t.x + b * t.y);
        sum += (z - bias <= texture(map, d2).r) ? 1.0 : 0.0;
    }
    return sum / float(max(taps, 1));
}
float cubeShadowMoments(samplerCube moments, vec3 worldPos, vec3 lightPos, float farPlane, float minVariance) {
    vec3 dir = worldPos - lightPos;
    float z = length(dir) / max(farPlane, 1e-6);
    if (z > 1.0) return 1.0;
    vec2 m = texture(moments, dir).rg;
    if (z <= m.x) return 1.0;
    float var = max(m.y - m.x * m.x, 0.0);
    float d = z - m.x;
    float pMax = var / max(var + d * d, 1e-8);
    return clamp((pMax - minVariance) / max(1.0 - minVariance, 1e-6), 0.0, 1.0);
}
// cascade index for a view-space depth (uses uCascadeCount / uCascadeSplits)
int cascadeSelect(float viewDepth) {
    int c = uCascadeCount - 1;
    for (int i = 0; i < uCascadeCount; ++i)
        if (viewDepth <= uCascadeSplits[i + 1]) { c = i; break; }
    return c;
}
// full cascade sampling with optional cross-fade into the next cascade near the split
// (blendFrac = fraction of the cascade span used for blending, 0 = hard switch)
float shadowFactorArray(sampler2DArray map, vec3 worldPos, float viewDepth, float bias,
                        float radiusTexels, int taps, float rotation, float blendFrac) {
    int c = cascadeSelect(viewDepth);
    vec3 p = shadowNdc(uLightVPs[c], worldPos);
    if (!shadowInBounds(p)) return 1.0;
    float sh = pcfPoissonArray(map, p.xy, p.z, bias, radiusTexels, c, taps, rotation);
    if (blendFrac > 0.0 && c < uCascadeCount - 1) {
        float span = max(uCascadeSplits[c + 1] - uCascadeSplits[c], 1e-6);
        float t = clamp((viewDepth - (uCascadeSplits[c + 1] - blendFrac * span)) / max(blendFrac * span, 1e-6), 0.0, 1.0);
        if (t > 0.0) {
            vec3 p2 = shadowNdc(uLightVPs[c + 1], worldPos);
            if (shadowInBounds(p2))
                sh = mix(sh, pcfPoissonArray(map, p2.xy, p2.z, bias, radiusTexels, c + 1, taps, rotation), t);
        }
    }
    return sh;
}
// linearize gl_FragCoord.z-style depth to positive view distance (perspective)
float linearizeDepth(float d) {
    return 2.0 * uCsZNear * uCsZFar / max(uCsZFar + uCsZNear - (2.0 * d - 1.0) * (uCsZFar - uCsZNear), 1e-6);
}
// screen-space raymarched contact shadow: marches viewPos along dirView (view space,
// camera looks down -Z) against uSceneDepth; returns visibility in [0,1]
float contactShadow(vec3 viewPos, vec3 dirView, float maxDist, float thickness, int steps, float jitter) {
    float acc = 1.0;
    float stepLen = maxDist / float(max(steps, 1));
    for (int i = 1; i <= steps; ++i) {
        vec3 sp = viewPos + dirView * ((float(i) + jitter) * stepLen);
        vec4 c = uProj * vec4(sp, 1.0);
        if (c.w <= 0.0) continue;
        vec2 uv = c.xy / c.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || sp.z >= -uCsZNear) continue;
        float sceneDepth = linearizeDepth(texture(uSceneDepth, uv).r);
        float d = -sp.z;
        float vis = (sceneDepth >= d) ? 1.0 : clamp((d - sceneDepth) / max(thickness, 1e-5), 0.0, 1.0);
        acc = min(acc, vis);
    }
    return acc;
}
)GLSL";

} // namespace fuhgl::render
