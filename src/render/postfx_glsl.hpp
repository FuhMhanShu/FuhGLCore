#pragma once

// Shared GLSL for the post-processing chains (DeferredRenderer + render::PostChain, F42).
// Fullscreen-triangle vertex shader + the generic passes: copy, separable blur,
// bloom bright-pass, crepuscular rays, TAA resolve, ACES composite.

#include "fuhgl/render/material.hpp"

#include "fuhgl/gl/shader.hpp"

#include <cstdio>
#include <string>

namespace fuhgl::render {

constexpr const char* kPostFsVert = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kPostBlurFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uDir; // (texel, 0) or (0, texel)
void main() {
    vec3 acc = texture(uTex, vUV).rgb * 0.227027;
    vec2 o1 = uDir * 1.3846153846, o2 = uDir * 3.2307692308;
    acc += (texture(uTex, vUV + o1).rgb + texture(uTex, vUV - o1).rgb) * 0.3162162162;
    acc += (texture(uTex, vUV + o2).rgb + texture(uTex, vUV - o2).rgb) * 0.0702702703;
    FragColor = vec4(acc, 1.0);
}
)GLSL";

constexpr const char* kPostBrightFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHdr;
uniform float uThreshold;
void main() {
    vec3 c = texture(uHdr, vUV).rgb;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(c * smoothstep(uThreshold, uThreshold + 0.5, l), 1.0);
}
)GLSL";

constexpr const char* kPostRaysFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHdr;
uniform vec2 uSunUV;
uniform float uDensity;
uniform float uDecay;
uniform float uWeight;
void main() {
    vec2 delta = (vUV - uSunUV) * uDensity / 48.0;
    vec2 uv = vUV;
    float illum = 0.0;
    float w = 1.0;
    for (int i = 0; i < 48; ++i) {
        uv -= delta;
        vec3 c = texture(uHdr, clamp(uv, 0.001, 0.999)).rgb;
        float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
        illum += clamp(l - 0.7, 0.0, 1.0) * w;
        w *= uDecay;
    }
    FragColor = vec4(vec3(illum / 48.0 * uWeight), 1.0);
}
)GLSL";

constexpr const char* kPostCompositeFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uRays;
uniform float uExposure;
uniform float uBloomStrength;
uniform float uRayScale;
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main() {
    vec3 c = texture(uScene, vUV).rgb * uExposure
           + texture(uBloom, vUV).rgb * uBloomStrength
           + texture(uRays, vUV).rgb * uRayScale;
    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));
    FragColor = vec4(c, 1.0);
}
)GLSL";

constexpr const char* kPostTaaFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uCur;   // current (jittered) frame
uniform sampler2D uHist;  // previous accumulated frame
uniform float uAlpha;     // 1 on the first frame, taaBlend afterwards
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uCur, vUV).rgb;
    vec3 h = texture(uHist, vUV).rgb;
    vec3 mn = c, mx = c;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            vec3 s = texture(uCur, vUV + vec2(float(x), float(y)) * uTexel).rgb;
            mn = min(mn, s);
            mx = max(mx, s);
        }
    h = clamp(h, mn, mx);
    FragColor = vec4(mix(h, c, uAlpha), 1.0);
}
)GLSL";

constexpr const char* kPostCopyFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() { FragColor = texture(uTex, vUV); }
)GLSL";

/// @brief Compiles a fullscreen material (kPostFsVert + fragSrc) for a post pass.
inline bool compilePostFs(Material& mat, const std::string& fragSrc, const char* what) {
    if (!mat.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kPostFsVert)) { std::fprintf(stderr, "%s VS: %s\n", what, vs.infoLog().c_str()); return false; }
    if (!fs.compile(fragSrc.c_str())) { std::fprintf(stderr, "%s FS: %s\n", what, fs.infoLog().c_str()); return false; }
    mat.program().attach(vs);
    mat.program().attach(fs);
    if (!mat.program().link()) { std::fprintf(stderr, "%s link: %s\n", what, mat.program().infoLog().c_str()); return false; }
    return true;
}

} // namespace fuhgl::render
