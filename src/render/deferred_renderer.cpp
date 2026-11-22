#include "fuhgl/render/deferred_renderer.hpp"
#include "fuhgl/render/mesh.hpp"

#include "fuhgl/gl/info.hpp"
#include "fuhgl/gl/state.hpp"

#include "lighting_glsl.hpp"
#include "postfx_glsl.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace fuhgl::render {
namespace {

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

// F32: TAA resolve — clamp the history to the current 3x3 neighborhood, then blend.
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
    // F39: capture capabilities once — used to gate effects (graceful degradation)
    {
        const gl::DeviceInfo di = gl::queryDeviceInfo();
        glMajor_ = di.version.major;
        glMinor_ = di.version.minor;
        hasBindless_ = di.hasExtension("GL_ARB_bindless_texture");
        hasCompute_ = di.features.compute;
    }
    if (!createPost(width, height)) return false;
    syncShadowTargets(); // F46: create the shadow targets matching the current filter/size
    return true;
}

void DeferredRenderer::syncShadowTargets() {
    if (!gl::isInitialized()) return;
    const bool wantMoments = shadowFilter == ShadowFilter::Vsm; // F46: VSM renders moments
    if (shadows_ && (shadowMap_.size() != shadowSize_ || shadowMap_.moments() != wantMoments)) {
        if (!shadowMap_.create(shadowSize_, wantMoments)) {
            std::fprintf(stderr, "DeferredRenderer: shadow map create failed\n");
            shadows_ = false;
        }
    }
    if (csmEnabled_ && (csm_.cascadeCount() != csmCascades_ || csm_.size() != shadowSize_ ||
                        csm_.moments() != wantMoments)) {
        if (!csm_.create(shadowSize_, csmCascades_, wantMoments)) {
            std::fprintf(stderr, "DeferredRenderer: CSM create failed\n");
            csmEnabled_ = false;
        }
    }
    if (pointShadows_ && (cubeShadow_.size() != pointShadowSize_ || cubeShadow_.moments() != wantMoments)) {
        if (!cubeShadow_.create(pointShadowSize_, wantMoments)) {
            std::fprintf(stderr, "DeferredRenderer: point shadow cube create failed\n");
            pointShadows_ = false;
        }
    }
}

bool DeferredRenderer::createPost(int w, int h) {
    const std::string ssaoSrc = std::string(kSsaoFragHead) + ssaoKernelGlsl(ssaoSamples) + kSsaoFragTail;
    const std::string volFogSrc = std::string(kVolFogFragHead) + kShadowUniforms + kShadowFunc + kVolFogFragTail;
    if (!compilePostFs(ssaoMat_, ssaoSrc, "ssao")) return false;
    if (!compilePostFs(blurMat_, kPostBlurFrag, "blur")) return false;
    if (!compilePostFs(brightMat_, kPostBrightFrag, "bright")) return false;
    if (!compilePostFs(raysMat_, kPostRaysFrag, "rays")) return false;
    if (!compilePostFs(compositeMat_, kPostCompositeFrag, "composite")) return false;
    if (!compilePostFs(taaMat_, kPostTaaFrag, "taa")) return false;
    if (!compilePostFs(dofMat_, kDofFrag, "dof")) return false;
    if (!compilePostFs(volfogMat_, volFogSrc, "volfog")) return false;
    if (!compilePostFs(ssrMat_, kSsrFrag, "ssr")) return false;
    if (!compilePostFs(copyMat_, kPostCopyFrag, "copy")) return false;
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
    csmCascades_ = cascades;
    csmEnabled_ = true;
    syncShadowTargets(); // F46: creates with moments when shadowFilter == Vsm
}

void DeferredRenderer::enableShadows(int mapSize) {
    shadowSize_ = mapSize;
    shadows_ = true;
    syncShadowTargets(); // F46: creates with moments when shadowFilter == Vsm
}

void DeferredRenderer::enablePointShadow(int lightIndex, int mapSize) {
    pointShadowLightIndex_ = lightIndex;
    pointShadowSize_ = mapSize;
    pointShadows_ = lightIndex >= 0;
    syncShadowTargets(); // F46
}

const Texture& DeferredRenderer::hdrTexture() const {
    const Texture* t = post_.texture("hdr");
    static const Texture fallback(Texture::Target::Tex2D);
    return t ? *t : fallback;
}

void DeferredRenderer::gate(Effect e, bool& flag) {
    if (flag && !effectSupported(e)) {
        const unsigned bit = 1u << static_cast<int>(e);
        if (!(warnedMask_ & bit)) {
            warnUnsupported("DeferredRenderer", e, glMajor_, glMinor_);
            warnedMask_ |= bit;
        }
        flag = false;
    }
}

void DeferredRenderer::applyCapabilities() {
    gate(Effect::Ssao, ssaoEnabled);
    gate(Effect::Bloom, bloomEnabled);
    gate(Effect::GodRays, godRaysEnabled);
    gate(Effect::Taa, taaEnabled);
    gate(Effect::Ssr, ssrEnabled);
    gate(Effect::DoF, dofEnabled);
    gate(Effect::VolFog, volFogEnabled);
    gate(Effect::Shadows, shadows_);
    gate(Effect::CsmShadows, csmEnabled_);
    gate(Effect::PointShadows, pointShadows_);   // F46
    gate(Effect::ContactShadows, contactShadowsEnabled); // F46
    gate(Effect::Lod, lodEnabled); // CPU-side: always supported, gate is a no-op
}

void DeferredRenderer::setEffectEnabled(Effect e, bool on) {
    switch (e) {
        case Effect::Ssao: ssaoEnabled = on; break;
        case Effect::Bloom: bloomEnabled = on; break;
        case Effect::GodRays: godRaysEnabled = on; break;
        case Effect::Taa: taaEnabled = on; break;
        case Effect::Ssr: ssrEnabled = on; break;
        case Effect::DoF: dofEnabled = on; break;
        case Effect::VolFog: volFogEnabled = on; break;
        case Effect::Shadows: shadows_ = on; break;
        case Effect::CsmShadows: csmEnabled_ = on; break;
        case Effect::PointShadows: pointShadows_ = on && pointShadowLightIndex_ >= 0; break; // F46
        case Effect::ContactShadows: contactShadowsEnabled = on; break;                      // F46
        case Effect::Lod: lodEnabled = on; break;
        default: {
            const unsigned bit = 1u << static_cast<int>(e);
            if (on) requestedMask_ |= bit;
            else requestedMask_ &= ~bit;
            break;
        }
    }
    if (on && !effectSupported(e) && !((warnedMask_ >> static_cast<int>(e)) & 1u)) {
        warnUnsupported("DeferredRenderer", e, glMajor_, glMinor_);
        warnedMask_ |= 1u << static_cast<int>(e);
    }
    applyCapabilities();
}

bool DeferredRenderer::effectEnabled(Effect e) const {
    if (!effectSupported(e)) return false;
    switch (e) {
        case Effect::Ssao: return ssaoEnabled;
        case Effect::Bloom: return bloomEnabled;
        case Effect::GodRays: return godRaysEnabled;
        case Effect::Taa: return taaEnabled;
        case Effect::Ssr: return ssrEnabled;
        case Effect::DoF: return dofEnabled;
        case Effect::VolFog: return volFogEnabled;
        case Effect::Shadows: return shadows_;
        case Effect::CsmShadows: return csmEnabled_;
        case Effect::PointShadows: return pointShadows_;        // F46
        case Effect::ContactShadows: return contactShadowsEnabled; // F46
        case Effect::Lod: return lodEnabled;
        default: return ((requestedMask_ >> static_cast<int>(e)) & 1u) != 0u;
    }
}

void DeferredRenderer::setAllEffects(bool on) {
    ssaoEnabled = bloomEnabled = godRaysEnabled = taaEnabled = ssrEnabled = dofEnabled =
        volFogEnabled = shadows_ = csmEnabled_ = contactShadowsEnabled = lodEnabled = on;
    pointShadows_ = on && pointShadowLightIndex_ >= 0; // F46
    requestedMask_ = on ? ((1u << kEffectCount) - 1u) : 0u;
    applyCapabilities();
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
        p.setUniform("uBloomStrength", bloomEnabled ? bloomStrength : 0.f);
        p.setUniform("uRayScale", godRaysEnabled ? 1.f : 0.f);
    });
    Framebuffer::unbind();
}

void DeferredRenderer::render(const SceneView& scene) {
    if (!scene.camera || width_ <= 0) return;
    SceneView s = scene;
    // F39: graceful degradation — effects the GPU cannot run are skipped (warned once)
    applyCapabilities();
    if (s.env && !effectSupported(Effect::Ibl)) {
        const unsigned bit = 1u << static_cast<int>(Effect::Ibl);
        if (!(warnedMask_ & bit)) {
            warnUnsupported("DeferredRenderer", Effect::Ibl, glMajor_, glMinor_);
            warnedMask_ |= bit;
        }
        s.env = nullptr;
    }

    // shadow depth pass: CSM (F30, first directional light) or single map (first directional/spot)
    syncShadowTargets(); // F46: (re)create targets when size/filter/moments changed
    if (csmEnabled_ && csm_.cascadeCount() > 0) {
        csm_.snapToTexels = shadowSnapToTexels; // F46: stabilize the cascades on the texel grid
        for (size_t i = 0; i < s.lights.size(); ++i) {
            if (s.lights[i].type == LightType::Directional && s.camera) {
                csm_.render(s, *s.camera, s.lights[i], shadowCenter_, shadowRadius_, shadowZNear_, shadowZFar_);
                s.cascades = &csm_;
                s.shadowLightIndex = static_cast<int>(i);
                s.shadowStrength = shadowStrength_;
                // F46: shadow-kit sampling config for the lighting pass
                s.shadowFilterMode = static_cast<int>(shadowFilter);
                s.shadowMomentsArrayTex = csm_.momentsTextureId();
                s.shadowCascadeBlend = shadowCascadeBlend;
                s.shadowLightSize = shadowLightSize;
                s.shadowWorldWidth = 2.f * shadowRadius_;
                s.shadowMinVariance = shadowMinVariance;
                break;
            }
        }
    } else if (shadows_ && shadowMap_.size() > 0) {
        for (size_t i = 0; i < s.lights.size(); ++i) {
            if (s.lights[i].type == LightType::Directional || s.lights[i].type == LightType::Spot) {
                s.lightVP = lightViewProj(s.lights[i], shadowCenter_, shadowRadius_);
                if (shadowSnapToTexels) // F46: snap the single map to the texel grid too
                    s.lightVP = snapLightVPToTexels(s.lightVP, shadowSize_, shadowCenter_);
                shadowMap_.render(s, s.lightVP);
                s.shadowMap = &shadowMap_;
                s.shadowLightIndex = static_cast<int>(i);
                s.shadowStrength = shadowStrength_;
                s.shadowFilterMode = static_cast<int>(shadowFilter);
                s.shadowMomentsTex = shadowMap_.momentsTextureId();
                s.shadowLightSize = shadowLightSize;
                s.shadowWorldWidth = 2.f * shadowRadius_;
                s.shadowMinVariance = shadowMinVariance;
                break;
            }
        }
    }
    // F46: cube shadow map for one point light
    if (pointShadows_ && pointShadowLightIndex_ >= 0 && pointShadowLightIndex_ < static_cast<int>(s.lights.size()) &&
        s.lights[pointShadowLightIndex_].type == LightType::Point) {
        cubeShadow_.render(s, s.lights[pointShadowLightIndex_], pointShadowNear_, pointShadowFar_);
        s.pointShadows = 1;
        s.pointShadowLightIndex = pointShadowLightIndex_;
        s.shadowCubeTex = cubeShadow_.textureId();
        s.shadowCubeMomentsTex = cubeShadow_.momentsTextureId();
        s.pointLightPos = s.lights[pointShadowLightIndex_].position;
        s.pointShadowFar = cubeShadow_.farPlane();
        s.pointShadowStrength = shadowStrength_;
    } else {
        s.pointShadows = 0;
    }
    // F46: contact shadows need a sampleable G-buffer depth texture (non-MSAA only)
    if (contactShadowsEnabled && pipeline_.ensureSampleableDepth() && pipeline_.depthTexture()) {
        s.contactShadows = 1;
        s.contactShadowStrength = contactShadowStrength;
        s.contactShadowMaxDist = contactShadowMaxDist;
        s.contactShadowThickness = contactShadowThickness;
        s.contactShadowSteps = contactShadowSteps;
        s.sceneDepthTex = pipeline_.depthTexture()->id();
    } else {
        s.contactShadows = 0;
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

    s.lodEnabled = lodEnabled; // F41
    curScene_ = &s;
    pipeline_.renderGBuffer(s);
    lastLodLevel_ = s.lastLodLevel;
    post_.setPassEnabled("ssao", ssaoEnabled);
    post_.setPassEnabled("ssaoBlurH", ssaoEnabled);
    post_.setPassEnabled("ssaoBlurV", ssaoEnabled);
    // bloom stays in the graph when disabled (unwritten resources could hold NaN);
    // the composite just multiplies by 0 — see drawComposite
    pipeline_.aoTexture = ssaoEnabled ? post_.texture("ssao") : nullptr;
    pipeline_.aoStrength = ssaoStrength;
    post_.execute();
    // advance after the post chain so the TAA pass sees frameIndex_ == 0 on the first
    // frame and resolves with uAlpha = 1 (clean history reset)
    if (taaEnabled) ++frameIndex_;
    curScene_ = nullptr;
}

} // namespace fuhgl::render
