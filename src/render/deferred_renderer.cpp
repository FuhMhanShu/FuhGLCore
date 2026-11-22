#include "fuhgl/render/deferred_renderer.hpp"
#include "fuhgl/render/mesh.hpp"

#include "fuhgl/gl/state.hpp"

#include "lighting_glsl.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace fuhgl::render {
namespace {

constexpr const char* kFsVert = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kSsaoFragHead = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uPosition; // world pos, alpha = geometry flag
uniform sampler2D uNormal;
uniform mat4 uViewProj;
uniform mat4 uView;
uniform float uRadius;
)GLSL";

constexpr const char* kSsaoFragTail = R"GLSL(
void main() {
    vec4 pos = texture(uPosition, vUV);
    if (pos.a < 0.5) { FragColor = vec4(1.0); return; } // sky: no AO
    vec3 worldPos = pos.rgb;
    vec3 N = normalize(texture(uNormal, vUV).rgb * 2.0 - 1.0);
    vec3 up = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    float fragDepth = -((uView * vec4(worldPos, 1.0)).z);
    float occ = 0.0;
    for (int i = 0; i < SAMPLES; ++i) {
        vec3 s = worldPos + (T * KERNEL[i].x + B * KERNEL[i].y + N * KERNEL[i].z) * uRadius;
        vec4 clip = uViewProj * vec4(s, 1.0);
        if (clip.w <= 0.0) continue;
        vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        vec4 sp = texture(uPosition, suv);
        if (sp.a < 0.5) continue;
        float sd = -((uView * vec4(sp.rgb, 1.0)).z);      // stored geometry depth
        float pd = -((uView * vec4(s, 1.0)).z);           // sample depth
        if (sd < pd - 0.02 && abs(sd - fragDepth) < uRadius * 3.0) occ += 1.0;
    }
    FragColor = vec4(vec3(1.0 - occ / float(SAMPLES)), 1.0);
}
)GLSL";

constexpr const char* kBlurFrag = R"GLSL(#version 330 core
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

constexpr const char* kBrightFrag = R"GLSL(#version 330 core
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

constexpr const char* kRaysFrag = R"GLSL(#version 330 core
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

constexpr const char* kCompositeFrag = R"GLSL(#version 330 core
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

// F32: TAA resolve — clamp the history to the current 3x3 neighborhood, then blend.
constexpr const char* kTaaFrag = R"GLSL(#version 330 core
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

// F36: depth of field — CoC from the G-buffer depth (math::cocRadius), bokeh gather on a
// golden-angle spiral; in-focus pixels keep their weight so they stay sharp.
constexpr const char* kDofFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSrc;      // 0: HDR scene (taa output)
uniform sampler2D uPosition; // 1: world pos, alpha = geometry flag
uniform vec3 uCamPos;
uniform float uFocusDist;
uniform float uFocusRange;
uniform vec2 uMaxCoc;  // max CoC in UV units (x, y)
uniform int uTaps;
float cocAt(vec2 uv) {
    vec4 p = texture(uPosition, uv);
    float dist = p.a > 0.5 ? length(p.xyz - uCamPos) : 1.0e6; // sky: far -> max blur
    float t = clamp((dist - uFocusDist) / max(uFocusRange, 1.0e-4), -1.0, 1.0);
    return abs(t);
}
void main() {
    float coc = cocAt(vUV);                  // 0..1
    vec2 radius = uMaxCoc * coc;             // own blur radius in UV
    vec4 acc = texture(uSrc, vUV) * max(0.05, coc * 8.0);
    float wsum = max(0.05, coc * 8.0);
    for (int i = 0; i < 32; ++i) {
        if (i >= uTaps) break;
        float fi = float(i) + 0.5;
        float r = sqrt(fi / float(uTaps));
        float theta = fi * 2.39996323;       // golden angle
        vec2 uv2 = vUV + vec2(cos(theta), sin(theta)) * r * radius;
        float coc2 = cocAt(uv2);
        float w2 = max(0.05, max(coc2, coc) * 8.0);
        acc += texture(uSrc, uv2) * w2;
        wsum += w2;
    }
    FragColor = acc / wsum;
}
)GLSL";

// F37: volumetric fog — raymarch the world-space view ray (G-buffer depth or far plane
// for sky) and accumulate single scattering from the sun with a Henyey-Greenstein phase
// function (math::henyeyGreenstein on the CPU side). The shadow uniforms/functions
// (kShadowUniforms + kShadowFunc) are spliced in between head and tail, so light shafts
// work with the single shadow map (F15) or the CSM array (F30) when volFogShadows is on.
constexpr const char* kVolFogFragHead = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSrc;      // 0: HDR scene (dof output)
uniform sampler2D uPosition; // 1: world pos, alpha = geometry flag
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec3 uSunDir;   // direction TO the sun
uniform vec3 uSunColor; // HDR sun radiance
uniform float uDensity;
uniform vec3 uFogColor; // scattering albedo tint
uniform float uAnisotropy;
uniform vec2 uHeightFog; // x = base height, y = falloff (y <= 0 disables height fog)
uniform int uSteps;
uniform float uMaxDist;
)GLSL";

constexpr const char* kVolFogFragTail = R"GLSL(
float hgPhase(float cosT, float g) {
    float g2 = g * g;
    float d = max(1.0 + g2 - 2.0 * g * cosT, 1e-4); // clamp: keeps sqrt() defined for |cosT| > 1
    return (1.0 - g2) / (4.0 * 3.14159265 * d * sqrt(d));
}
void main() {
    vec4 gp = texture(uPosition, vUV);
    vec3 toFar;
    float dist;
    if (gp.a > 0.5) { // geometry: march only up to the surface
        toFar = gp.rgb - uCamPos;
        dist = length(toFar);
    } else { // sky: march to the far plane
        vec4 wp = uInvViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
        toFar = wp.xyz / wp.w - uCamPos;
        dist = length(toFar);
    }
    vec3 src = texture(uSrc, vUV).rgb;
    if (dist < 1e-3) { FragColor = vec4(src, 1.0); return; }
    vec3 dir = toFar / dist; // unit length: cosT below stays in [-1, 1] even when clamped
    float maxDist = min(dist, uMaxDist);
    float stepLen = maxDist / float(uSteps);
    float phase = hgPhase(clamp(dot(dir, uSunDir), -1.0, 1.0), uAnisotropy);
    float T = 1.0;
    vec3 insc = vec3(0.0);
    for (int i = 0; i < 64; ++i) {
        if (i >= uSteps) break;
        vec3 p = uCamPos + dir * ((float(i) + 0.5) * stepLen);
        float sigma = uDensity;
        if (uHeightFog.y > 0.0)
            sigma *= exp(-max(p.y - uHeightFog.x, 0.0) * uHeightFog.y);
        float sh = 1.0;
        if (uShadowEnabled == 1)
            sh = shadowFactor(p, vec3(0.0, 1.0, 0.0), uSunDir);
        insc += uSunColor * (phase * uFogColor) * (sigma * sh) * T * stepLen;
        T *= exp(-sigma * stepLen);
    }
    FragColor = vec4(src * T + insc, 1.0);
}
)GLSL";

// F33: screen-space reflections — march the reflection ray in world space, compare
// against the G-buffer position texture, sample the HDR color at the hit point.
constexpr const char* kSsrFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHdr;      // 0: base HDR color
uniform sampler2D uPosition; // 1: world pos, alpha = geometry flag
uniform sampler2D uNormal;   // 2
uniform mat4 uView;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform float uStrength;
uniform int uSteps;
uniform float uThickness;
void main() {
    vec3 base = texture(uHdr, vUV).rgb;
    vec4 pos = texture(uPosition, vUV);
    if (pos.a < 0.5) { FragColor = vec4(base, 1.0); return; } // sky: no reflections
    vec3 worldPos = pos.rgb;
    vec3 N = normalize(texture(uNormal, vUV).rgb * 2.0 - 1.0);
    vec3 V = normalize(uCamPos - worldPos);
    vec3 R = reflect(-V, N);
    if (dot(R, N) <= 0.0) { FragColor = vec4(base, 1.0); return; }
    float stepLen = max(length(worldPos - uCamPos) * 0.02, 0.01);
    vec3 refl = vec3(0.0);
    for (int i = 1; i <= 64; ++i) {
        if (i > uSteps) break;
        vec3 rp = worldPos + R * (stepLen * float(i));
        vec4 clip = uViewProj * vec4(rp, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
        vec4 sp = texture(uPosition, uv);
        if (sp.a < 0.5) continue; // sky along the ray: keep marching
        float dSurf = length(sp.rgb - worldPos);
        float dRay = stepLen * float(i);
        if (dRay > dSurf && dRay - dSurf < uThickness) {
            float fade = 1.0 - float(i) / float(uSteps);
            refl = texture(uHdr, uv).rgb * fade;
            break;
        }
    }
    FragColor = vec4(base + refl * uStrength, 1.0);
}
)GLSL";

constexpr const char* kCopyFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() { FragColor = texture(uTex, vUV); }
)GLSL";

float lcg(unsigned& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
}

std::string ssaoKernelGlsl(int samples) {
    unsigned state = 0x12345678u;
    std::string k = "const int SAMPLES = " + std::to_string(samples) + ";\n";
    k += "const vec3 KERNEL[" + std::to_string(samples) + "] = vec3[](";
    for (int i = 0; i < samples; ++i) {
        const float x = lcg(state) * 2.f - 1.f;
        const float y = lcg(state) * 2.f - 1.f;
        const float z = lcg(state);
        const float len = std::sqrt(x * x + y * y + z * z) + 1e-6f;
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float scale = 0.1f + 0.9f * t * t; // concentrate samples near the fragment
        char buf[96];
        std::snprintf(buf, sizeof buf, "%svec3(%.4ff, %.4ff, %.4ff)", i ? ", " : "",
                      x / len * scale, y / len * scale, z / len * scale);
        k += buf;
    }
    k += ");\n";
    return k;
}

bool compileFs(Material& mat, const std::string& fragSrc, const char* what) {
    if (!mat.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kFsVert)) { std::fprintf(stderr, "DeferredRenderer: %s VS: %s\n", what, vs.infoLog().c_str()); return false; }
    if (!fs.compile(fragSrc.c_str())) { std::fprintf(stderr, "DeferredRenderer: %s FS: %s\n", what, fs.infoLog().c_str()); return false; }
    mat.program().attach(vs);
    mat.program().attach(fs);
    if (!mat.program().link()) { std::fprintf(stderr, "DeferredRenderer: %s link: %s\n", what, mat.program().infoLog().c_str()); return false; }
    return true;
}

} // namespace

struct DeferredRenderer::Impl {
    // F37: 1x1 dummies so the volfog pass's shadow samplers stay complete when shadows are off
    Texture dummyShadowTex{Texture::Target::Tex2D};
    Texture dummyShadowArrayTex{Texture::Target::Tex2DArray};
};

DeferredRenderer::DeferredRenderer() : impl_(std::make_unique<Impl>()) {}
DeferredRenderer::~DeferredRenderer() = default;
DeferredRenderer::DeferredRenderer(DeferredRenderer&&) noexcept = default;
DeferredRenderer& DeferredRenderer::operator=(DeferredRenderer&&) noexcept = default;

math::Vec2 sunScreenUV(const Camera& camera, const math::Vec3& sunDir, int width, int height) {
    if (width <= 0 || height <= 0) return {-1.f, -1.f};
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const math::Mat4 vp = camera.viewProj(aspect);
    const math::Vec3 p = camera.eye() + math::normalize(sunDir) * 1000.f;
    const math::Vec4 c = vp * math::Vec4(p, 1.f);
    if (c.w <= 0.f) return {-1.f, -1.f}; // behind the camera
    return {c.x / c.w * 0.5f + 0.5f, c.y / c.w * 0.5f + 0.5f};
}

bool DeferredRenderer::create(int width, int height, int msaaSamples) {
    width_ = width;
    height_ = height;
    msaa_ = msaaSamples;
    if (!pipeline_.create(width, height, msaaSamples)) {
        std::fprintf(stderr, "DeferredRenderer: pipeline create failed\n");
        return false;
    }
    if (!createPost(width, height)) return false;
    if (shadows_ && !shadowMap_.create(shadowSize_)) {
        std::fprintf(stderr, "DeferredRenderer: shadow map create failed\n");
        return false;
    }
    return true;
}

bool DeferredRenderer::createPost(int w, int h) {
    const std::string ssaoSrc = std::string(kSsaoFragHead) + ssaoKernelGlsl(ssaoSamples) + kSsaoFragTail;
    const std::string volFogSrc = std::string(kVolFogFragHead) + kShadowUniforms + kShadowFunc + kVolFogFragTail;
    if (!compileFs(ssaoMat_, ssaoSrc, "ssao")) return false;
    if (!compileFs(blurMat_, kBlurFrag, "blur")) return false;
    if (!compileFs(brightMat_, kBrightFrag, "bright")) return false;
    if (!compileFs(raysMat_, kRaysFrag, "rays")) return false;
    if (!compileFs(compositeMat_, kCompositeFrag, "composite")) return false;
    if (!compileFs(taaMat_, kTaaFrag, "taa")) return false;
    if (!compileFs(dofMat_, kDofFrag, "dof")) return false;
    if (!compileFs(volfogMat_, volFogSrc, "volfog")) return false;
    if (!compileFs(ssrMat_, kSsrFrag, "ssr")) return false;
    if (!compileFs(copyMat_, kCopyFrag, "copy")) return false;
    if (!fsVao_.create()) return false;
    fsVao_.bind();
    // F37: 1x1 dummy depth textures keep the volfog shadow samplers complete when off
    impl_->dummyShadowTex.bind();
    impl_->dummyShadowTex.texStorage2D(1, gl::GL_DEPTH_COMPONENT24, 1, 1);
    impl_->dummyShadowTex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    impl_->dummyShadowTex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    impl_->dummyShadowTex.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    impl_->dummyShadowTex.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    impl_->dummyShadowArrayTex.bind();
    impl_->dummyShadowArrayTex.texStorage3D(1, gl::GL_DEPTH_COMPONENT24, 1, 1, 1);
    impl_->dummyShadowArrayTex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    impl_->dummyShadowArrayTex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);

    // --- build the post-processing render graph (F31) ---------------------------------
    post_ = RenderGraph();
    post_.addExternal("gAlbedo", &pipeline_.texAlbedo(), w, h);
    post_.addExternal("gNormal", &pipeline_.texNormal(), w, h);
    post_.addExternal("gPosition", &pipeline_.texPosition(), w, h);
    post_.addExternal("gEmissive", &pipeline_.texEmissive(), w, h);
    post_.addExternal("gPbr", &pipeline_.texPbr(), w, h);
    post_.addResource("ssao", w, h, gl::GL_RGBA8);
    post_.addResource("blur", w, h, gl::GL_RGBA8);
    post_.addResource("hdr", w, h, gl::GL_RGBA16F);
    post_.addResource("ssr", w, h, gl::GL_RGBA16F);
    post_.addResource("taa", w, h, gl::GL_RGBA16F);
    post_.addResource("bloomA", w, h, gl::GL_RGBA16F);
    post_.addResource("bloomB", w, h, gl::GL_RGBA16F);
    post_.addResource("rays", w, h, gl::GL_RGBA16F);

    const float tx = 1.f / static_cast<float>(w), ty = 1.f / static_cast<float>(h);
    post_.addPass("ssao", {"gPosition", "gNormal"}, {"ssao"}, [this](RenderGraph::PassContext& ctx) { drawSSAO(ctx); });
    post_.addPass("ssaoBlurH", {"ssao"}, {"blur"}, [this, tx](RenderGraph::PassContext& ctx) { drawBlur(ctx, "ssao", {tx, 0.f}); });
    post_.addPass("ssaoBlurV", {"blur"}, {"ssao"}, [this, ty](RenderGraph::PassContext& ctx) { drawBlur(ctx, "blur", {0.f, ty}); });
    post_.addPass("lighting", {"gAlbedo", "gNormal", "gPosition", "gEmissive", "gPbr"}, {"hdr"},
                  [this](RenderGraph::PassContext& ctx) { drawLighting(ctx); });
    post_.addPass("ssr", {"hdr", "gPosition", "gNormal"}, {"ssr"}, [this](RenderGraph::PassContext& ctx) { drawSSR(ctx); });
    post_.addPass("taa", {"ssr", "taa"}, {"taa"}, [this](RenderGraph::PassContext& ctx) { drawTAA(ctx); }, /*feedback=*/true);
    post_.addResource("dof", w, h, gl::GL_RGBA16F);
    post_.addPass("dof", {"taa", "gPosition"}, {"dof"}, [this](RenderGraph::PassContext& ctx) { drawDoF(ctx); });
    post_.addResource("volfog", w, h, gl::GL_RGBA16F);
    post_.addPass("volfog", {"dof", "gPosition"}, {"volfog"}, [this](RenderGraph::PassContext& ctx) { drawVolFog(ctx); });
    post_.addPass("bright", {"taa"}, {"bloomA"}, [this](RenderGraph::PassContext& ctx) { drawBloomBright(ctx); });
    // bloom blur ping-pong (2 passes per bloomBlurPasses)
    std::string bloomCur = "bloomA";
    for (int i = 0; i < bloomBlurPasses; ++i) {
        const std::string next = (bloomCur == "bloomA") ? "bloomB" : "bloomA";
        post_.addPass("bloomBlurH" + std::to_string(i), {bloomCur}, {next},
                      [this, bloomCur, tx](RenderGraph::PassContext& ctx) { drawBlur(ctx, bloomCur, {tx, 0.f}); });
        const std::string next2 = (next == "bloomA") ? "bloomB" : "bloomA";
        post_.addPass("bloomBlurV" + std::to_string(i), {next}, {next2},
                      [this, next, ty](RenderGraph::PassContext& ctx) { drawBlur(ctx, next, {0.f, ty}); });
        bloomCur = next2;
    }
    bloomLast_ = bloomCur;
    post_.addPass("rays", {"taa"}, {"rays"}, [this](RenderGraph::PassContext& ctx) { drawRays(ctx); });
    post_.addPass("composite", {"volfog", bloomLast_, "rays"}, {},
                  [this](RenderGraph::PassContext& ctx) { drawComposite(ctx, bloomLast_); });
    if (!post_.compile()) {
        std::fprintf(stderr, "DeferredRenderer: post graph compile failed\n");
        return false;
    }
    return true;
}

void DeferredRenderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    // simplest correct approach: recreate everything (also rebuilds the post graph)
    *this = DeferredRenderer();
    create(width, height, msaa_);
}

void DeferredRenderer::enableCsmShadows(int mapSize, int cascades) {
    shadowSize_ = mapSize;
    csmEnabled_ = true;
    if (gl::isInitialized() && csm_.cascadeCount() <= 0 && !csm_.create(mapSize, cascades)) {
        std::fprintf(stderr, "DeferredRenderer: CSM create failed\n");
        csmEnabled_ = false;
    }
}

void DeferredRenderer::enableShadows(int mapSize) {
    shadowSize_ = mapSize;
    shadows_ = true;
    if (gl::isInitialized() && shadowMap_.size() <= 0 && !shadowMap_.create(mapSize)) {
        std::fprintf(stderr, "DeferredRenderer: shadow map create failed\n");
        shadows_ = false;
    }
}

const Texture& DeferredRenderer::hdrTexture() const {
    const Texture* t = post_.texture("hdr");
    static const Texture fallback(Texture::Target::Tex2D);
    return t ? *t : fallback;
}

void DeferredRenderer::drawFs(Material& mat, const std::function<void(Program&)>& setUniforms) {
    gl::disable(gl::Cap::DepthTest);
    gl::disable(gl::Cap::Blend);
    mat.bind();
    if (setUniforms) setUniforms(mat.program());
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
}

void DeferredRenderer::drawSSAO(RenderGraph::PassContext& ctx) {
    const SceneView& s = *curScene_;
    ctx.bindInput(0, "gPosition");
    ctx.bindInput(1, "gNormal");
    drawFs(ssaoMat_, [&](Program& p) {
        p.setUniform("uViewProj", math::jitterViewProj(s.camera->viewProj(s.aspect), s.jitter));
        p.setUniform("uView", s.camera->view());
        p.setUniform("uRadius", ssaoRadius);
        p.setUniform(p.uniformLocation("uPosition"), 0);
        p.setUniform(p.uniformLocation("uNormal"), 1);
    });
}

void DeferredRenderer::drawBlur(RenderGraph::PassContext& ctx, std::string_view in, const math::Vec2& dir) {
    ctx.bindInput(0, in);
    drawFs(blurMat_, [&](Program& p) {
        p.setUniform("uDir", dir);
        p.setUniform(p.uniformLocation("uTex"), 0);
    });
}

void DeferredRenderer::drawLighting(RenderGraph::PassContext& ctx) {
    pipeline_.renderLighting(*curScene_, &ctx.framebuffer());
}

void DeferredRenderer::drawSSR(RenderGraph::PassContext& ctx) {
    if (!ssrEnabled) {
        drawCopy(ctx, "hdr");
        return;
    }
    const SceneView& s = *curScene_;
    ctx.bindInput(0, "hdr");
    ctx.bindInput(1, "gPosition");
    ctx.bindInput(2, "gNormal");
    drawFs(ssrMat_, [&](Program& p) {
        p.setUniform("uView", s.camera->view());
        p.setUniform("uViewProj", math::jitterViewProj(s.camera->viewProj(s.aspect), s.jitter));
        p.setUniform("uCamPos", s.camera->eye());
        p.setUniform("uStrength", ssrStrength);
        p.setUniform(p.uniformLocation("uSteps"), ssrMaxSteps);
        p.setUniform("uThickness", ssrThickness);
        p.setUniform(p.uniformLocation("uHdr"), 0);
        p.setUniform(p.uniformLocation("uPosition"), 1);
        p.setUniform(p.uniformLocation("uNormal"), 2);
    });
}

void DeferredRenderer::drawTAA(RenderGraph::PassContext& ctx) {
    if (!taaEnabled) {
        drawCopy(ctx, "ssr");
        return;
    }
    ctx.bindInput(0, "ssr");
    ctx.bindInput(1, "taa"); // previous accumulated frame (scratch copy, see RenderGraph)
    drawFs(taaMat_, [&](Program& p) {
        p.setUniform("uAlpha", frameIndex_ == 0 ? 1.f : taaBlend);
        p.setUniform("uTexel", math::Vec2(1.f / static_cast<float>(width_), 1.f / static_cast<float>(height_)));
        p.setUniform(p.uniformLocation("uCur"), 0);
        p.setUniform(p.uniformLocation("uHist"), 1);
    });
}

void DeferredRenderer::drawDoF(RenderGraph::PassContext& ctx) {
    if (!dofEnabled) {
        drawCopy(ctx, "taa");
        return;
    }
    const SceneView& s = *curScene_;
    ctx.bindInput(0, "taa");
    ctx.bindInput(1, "gPosition");
    drawFs(dofMat_, [&](Program& p) {
        p.setUniform("uCamPos", s.camera->eye());
        p.setUniform("uFocusDist", dofFocusDist);
        p.setUniform("uFocusRange", dofFocusRange);
        p.setUniform("uMaxCoc", math::Vec2(dofStrength / static_cast<float>(width_),
                                            dofStrength / static_cast<float>(height_)));
        p.setUniform(p.uniformLocation("uTaps"), dofTaps);
        p.setUniform(p.uniformLocation("uSrc"), 0);
        p.setUniform(p.uniformLocation("uPosition"), 1);
    });
}

void DeferredRenderer::drawVolFog(RenderGraph::PassContext& ctx) {
    if (!volFogEnabled) {
        drawCopy(ctx, "dof");
        return;
    }
    const SceneView& s = *curScene_;
    ctx.bindInput(0, "dof");
    ctx.bindInput(1, "gPosition");
    // shadow samplers on the same units as the lighting pass (dummies when off)
    const bool csm = s.cascades && s.cascades->cascadeCount() > 0;
    const bool shadowed = volFogShadows && (csm || s.shadowMap);
    gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitShadowMap);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D,
                                  s.shadowMap ? s.shadowMap->textureId() : impl_->dummyShadowTex.id());
    gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitShadowMapArray);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D_ARRAY,
                                  csm ? s.cascades->textureId() : impl_->dummyShadowArrayTex.id());
    drawFs(volfogMat_, [&](Program& p) {
        p.setUniform("uCamPos", s.camera->eye());
        p.setUniform("uInvViewProj", s.camera->viewProj(s.aspect).inverse());
        p.setUniform("uView", s.camera->view());
        p.setUniform("uSunDir", s.sunDirection);
        p.setUniform("uSunColor", s.sunColor);
        p.setUniform("uDensity", volFogDensity);
        p.setUniform("uFogColor", volFogColor);
        p.setUniform("uAnisotropy", volFogAnisotropy);
        p.setUniform("uHeightFog", math::Vec2(volFogHeight, volFogHeight > 0.f ? volFogFalloff : 0.f));
        p.setUniform(p.uniformLocation("uSteps"), math::clamp(volFogSteps, 1, 64));
        p.setUniform("uMaxDist", volFogMaxDist);
        p.setUniform(p.uniformLocation("uSrc"), 0);
        p.setUniform(p.uniformLocation("uPosition"), 1);
        p.setUniform(p.uniformLocation("uShadowMap"), kTextureUnitShadowMap);
        p.setUniform(p.uniformLocation("uShadowMapArray"), kTextureUnitShadowMapArray);
        if (shadowed) {
            p.setUniform(p.uniformLocation("uShadowEnabled"), 1);
            if (csm) {
                p.setUniform(p.uniformLocation("uShadowMode"), 1);
                p.setUniform(p.uniformLocation("uCascadeCount"), s.cascades->cascadeCount());
                p.setUniform(p.uniformLocation("uCascadeSplits"), s.cascades->cascadeCount() + 1,
                             s.cascades->splits().data());
                p.setUniform(p.uniformLocation("uLightVPs"), s.cascades->cascadeCount(),
                             &s.cascades->cascadeVP(0));
            } else {
                p.setUniform(p.uniformLocation("uShadowMode"), 0);
                p.setUniform("uLightVP", s.lightVP);
            }
        } else {
            p.setUniform(p.uniformLocation("uShadowEnabled"), 0);
            p.setUniform(p.uniformLocation("uShadowMode"), 0);
        }
    });
}

void DeferredRenderer::drawCopy(RenderGraph::PassContext& ctx, std::string_view in) {
    ctx.bindInput(0, in);
    drawFs(copyMat_, [](Program& p) { p.setUniform(p.uniformLocation("uTex"), 0); });
}

void DeferredRenderer::drawBloomBright(RenderGraph::PassContext& ctx) {
    ctx.bindInput(0, "taa");
    drawFs(brightMat_, [&](Program& p) {
        p.setUniform("uThreshold", bloomThreshold);
        p.setUniform(p.uniformLocation("uHdr"), 0);
    });
}

void DeferredRenderer::drawRays(RenderGraph::PassContext& ctx) {
    const SceneView& s = *curScene_;
    const math::Vec2 suv = sunScreenUV(*s.camera, s.sunDirection, width_, height_);
    const bool visible = suv.x >= 0.f;
    ctx.bindInput(0, "taa");
    drawFs(raysMat_, [&](Program& p) {
        p.setUniform("uSunUV", visible ? suv : math::Vec2(0.5f, 1.5f));
        p.setUniform("uDensity", rayDensity);
        p.setUniform("uDecay", rayDecay);
        p.setUniform("uWeight", (visible && godRaysEnabled) ? rayWeight : 0.f);
        p.setUniform(p.uniformLocation("uHdr"), 0);
    });
}

void DeferredRenderer::drawComposite(RenderGraph::PassContext& ctx, std::string_view bloomRes) {
    (void)ctx;
    pipeline_.output().bind();
    const gl::GLenum buf[1] = {gl::GL_COLOR_ATTACHMENT0};
    gl::drawBuffers(1, buf);
    gl::viewport(0, 0, width_, height_);
    compositeMat_.setTexture(0, *post_.texture("volfog"));
    compositeMat_.setTexture(1, *post_.texture(bloomRes));
    compositeMat_.setTexture(2, *post_.texture("rays"));
    drawFs(compositeMat_, [&](Program& p) {
        p.setUniform(p.uniformLocation("uScene"), 0);
        p.setUniform(p.uniformLocation("uBloom"), 1);
        p.setUniform(p.uniformLocation("uRays"), 2);
        p.setUniform("uExposure", exposure);
        p.setUniform("uBloomStrength", bloomStrength);
        p.setUniform("uRayScale", godRaysEnabled ? 1.f : 0.f);
    });
    Framebuffer::unbind();
}

void DeferredRenderer::render(const SceneView& scene) {
    if (!scene.camera || width_ <= 0) return;
    SceneView s = scene;

    // shadow depth pass: CSM (F30, first directional light) or single map (first directional/spot)
    if (csmEnabled_ && csm_.cascadeCount() > 0) {
        for (size_t i = 0; i < s.lights.size(); ++i) {
            if (s.lights[i].type == LightType::Directional && s.camera) {
                csm_.render(s, *s.camera, s.lights[i], shadowCenter_, shadowRadius_, shadowZNear_, shadowZFar_);
                s.cascades = &csm_;
                s.shadowLightIndex = static_cast<int>(i);
                s.shadowStrength = shadowStrength_;
                break;
            }
        }
    } else if (shadows_ && shadowMap_.size() > 0) {
        for (size_t i = 0; i < s.lights.size(); ++i) {
            if (s.lights[i].type == LightType::Directional || s.lights[i].type == LightType::Spot) {
                s.lightVP = lightViewProj(s.lights[i], shadowCenter_, shadowRadius_);
                shadowMap_.render(s, s.lightVP);
                s.shadowMap = &shadowMap_;
                s.shadowLightIndex = static_cast<int>(i);
                s.shadowStrength = shadowStrength_;
                break;
            }
        }
    }
    // sky sun disk + god-ray source from the first directional light
    for (const Light& l : s.lights) {
        if (l.type == LightType::Directional) {
            s.sunDirection = -math::normalize(l.direction);
            s.sunColor = l.color * (l.intensity * 4.f);
            break;
        }
    }
    // F32: Halton subpixel jitter (NDC units) for TAA — applied by the G-buffer pass
    if (taaEnabled) {
        const math::Vec2 hpt = math::halton(frameIndex_);
        s.jitter = {(hpt.x - 0.5f) * 2.f / static_cast<float>(width_),
                    (hpt.y - 0.5f) * 2.f / static_cast<float>(height_)};
    } else {
        s.jitter = {0.f, 0.f};
        frameIndex_ = 0;
    }

    curScene_ = &s;
    pipeline_.renderGBuffer(s);
    post_.setPassEnabled("ssao", ssaoEnabled);
    post_.setPassEnabled("ssaoBlurH", ssaoEnabled);
    post_.setPassEnabled("ssaoBlurV", ssaoEnabled);
    pipeline_.aoTexture = ssaoEnabled ? post_.texture("ssao") : nullptr;
    pipeline_.aoStrength = ssaoStrength;
    post_.execute();
    // advance after the post chain so the TAA pass sees frameIndex_ == 0 on the first
    // frame and resolves with uAlpha = 1 (clean history reset)
    if (taaEnabled) ++frameIndex_;
    curScene_ = nullptr;
}

} // namespace fuhgl::render
