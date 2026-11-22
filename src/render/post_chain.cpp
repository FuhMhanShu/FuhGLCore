#include "fuhgl/render/post_chain.hpp"

#include "fuhgl/render/mesh.hpp"
#include "fuhgl/render/shadow.hpp"

#include "fuhgl/gl/info.hpp"
#include "fuhgl/gl/state.hpp"

#include "lighting_glsl.hpp"
#include "postfx_glsl.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace fuhgl::render {
namespace {

// F42: DoF (F36) from the DEPTH texture instead of a G-buffer position — reconstruct the
// world position via the inverse view-projection; sky (depth 1) -> far -> max blur.
constexpr const char* kDofDepthFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSrc;   // 0: HDR input (taa output)
uniform sampler2D uDepth; // 1: window depth
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform float uFocusDist;
uniform float uFocusRange;
uniform vec2 uMaxCoc;  // max CoC in UV units (x, y)
uniform int uTaps;
float cocAt(vec2 uv) {
    float d = texture(uDepth, uv).r;
    vec4 wp = uInvViewProj * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    float dist = d >= 1.0 ? 1.0e6 : length(wp.xyz / wp.w - uCamPos); // sky: far -> max blur
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

// F42/F37: volumetric fog from the depth texture (world pos via inverse VP; sky -> far plane).
constexpr const char* kVolFogDepthFragHead = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSrc;   // 0: HDR input (dof output)
uniform sampler2D uDepth; // 1: window depth
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uDensity;
uniform vec3 uFogColor;
uniform float uAnisotropy;
uniform vec2 uHeightFog;
uniform int uSteps;
uniform float uMaxDist;
)GLSL";

constexpr const char* kVolFogDepthFragTail = R"GLSL(
float hgPhase(float cosT, float g) {
    float g2 = g * g;
    float d = max(1.0 + g2 - 2.0 * g * cosT, 1e-4);
    return (1.0 - g2) / (4.0 * 3.14159265 * d * sqrt(d));
}
void main() {
    float d = texture(uDepth, vUV).r;
    vec4 wp = uInvViewProj * vec4(vUV * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec3 toFar = wp.xyz / wp.w - uCamPos;
    float dist = d >= 1.0 ? length(toFar) : length(toFar); // sky handled by clamp below
    vec3 src = texture(uSrc, vUV).rgb;
    if (dist < 1e-3) { FragColor = vec4(src, 1.0); return; }
    vec3 dir = toFar / dist;
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

} // namespace

PostChain::PostChain() = default;
PostChain::~PostChain() = default;
PostChain::PostChain(PostChain&&) noexcept = default;
PostChain& PostChain::operator=(PostChain&&) noexcept = default;

void PostChain::captureCaps() {
    if (!gl::isInitialized()) return;
    const gl::DeviceInfo di = gl::queryDeviceInfo();
    glMajor_ = di.version.major;
    glMinor_ = di.version.minor;
    hasBindless_ = di.hasExtension("GL_ARB_bindless_texture");
    hasCompute_ = di.features.compute;
}

bool PostChain::effectSupported(Effect e) const {
    if (glMajor_ == 0 && gl::isInitialized()) const_cast<PostChain*>(this)->captureCaps();
    return render::effectSupported(e, glMajor_, glMinor_, hasBindless_, hasCompute_);
}

void PostChain::gate(Effect e, bool& flag) {
    if (flag && !effectSupported(e)) {
        const unsigned bit = 1u << static_cast<int>(e);
        if (!(warnedMask_ & bit)) {
            warnUnsupported("PostChain", e, glMajor_, glMinor_);
            warnedMask_ |= bit;
        }
        flag = false;
    }
}

void PostChain::applyCapabilities() {
    gate(Effect::Bloom, bloomEnabled);
    gate(Effect::GodRays, godRaysEnabled);
    gate(Effect::Taa, taaEnabled);
    gate(Effect::DoF, dofEnabled);
    gate(Effect::VolFog, volFogEnabled);
}

void PostChain::setEffectEnabled(Effect e, bool on) {
    switch (e) {
        case Effect::Bloom: bloomEnabled = on; break;
        case Effect::GodRays: godRaysEnabled = on; break;
        case Effect::Taa: taaEnabled = on; break;
        case Effect::DoF: dofEnabled = on; break;
        case Effect::VolFog: volFogEnabled = on; break;
        default: break; // not owned by the post chain
    }
    if (on && !effectSupported(e) && !((warnedMask_ >> static_cast<int>(e)) & 1u)) {
        warnUnsupported("PostChain", e, glMajor_, glMinor_);
        warnedMask_ |= 1u << static_cast<int>(e);
    }
    applyCapabilities();
}

bool PostChain::effectEnabled(Effect e) const {
    if (!effectSupported(e)) return false;
    switch (e) {
        case Effect::Bloom: return bloomEnabled;
        case Effect::GodRays: return godRaysEnabled;
        case Effect::Taa: return taaEnabled;
        case Effect::DoF: return dofEnabled;
        case Effect::VolFog: return volFogEnabled;
        default: return false;
    }
}

void PostChain::setAllEffects(bool on) {
    bloomEnabled = godRaysEnabled = taaEnabled = dofEnabled = volFogEnabled = on;
    applyCapabilities();
}

bool PostChain::create(int w, int h) {
    width_ = w;
    height_ = h;
    captureCaps();
    return createGraph(w, h);
}

void PostChain::resize(int w, int h) {
    if (w == width_ && h == height_) return;
    width_ = w;
    height_ = h;
    if (!createGraph(w, h)) width_ = 0;
}

bool PostChain::createGraph(int w, int h) {
    const std::string volFogSrc = std::string(kVolFogDepthFragHead) + kShadowUniforms + kShadowFunc + kVolFogDepthFragTail;
    if (!compilePostFs(copyMat_, kPostCopyFrag, "post copy")) return false;
    if (!compilePostFs(blurMat_, kPostBlurFrag, "post blur")) return false;
    if (!compilePostFs(brightMat_, kPostBrightFrag, "post bright")) return false;
    if (!compilePostFs(raysMat_, kPostRaysFrag, "post rays")) return false;
    if (!compilePostFs(compositeMat_, kPostCompositeFrag, "post composite")) return false;
    if (!compilePostFs(taaMat_, kPostTaaFrag, "post taa")) return false;
    if (!compilePostFs(dofMat_, kDofDepthFrag, "post dof")) return false;
    if (!compilePostFs(volfogMat_, volFogSrc, "post volfog")) return false;
    if (!fsVao_.create()) return false;
    fsVao_.bind();

    // dummies for the external resources + shadow samplers
    dummyHdr_.bind();
    dummyHdr_.texStorage2D(1, gl::GL_RGBA16F, 1, 1);
    dummyDepth_.bind();
    dummyDepth_.texStorage2D(1, gl::GL_DEPTH_COMPONENT24, 1, 1);
    dummyDepth_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    dummyDepth_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    dummyShadowTex_.bind();
    dummyShadowTex_.texStorage2D(1, gl::GL_DEPTH_COMPONENT24, 1, 1);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    dummyShadowArrayTex_.bind();
    dummyShadowArrayTex_.texStorage3D(1, gl::GL_DEPTH_COMPONENT24, 1, 1, 1);
    dummyShadowArrayTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    dummyShadowArrayTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);

    post_ = RenderGraph();
    post_.addExternal("hdr", &dummyHdr_, w, h);
    post_.addExternal("depth", &dummyDepth_, w, h);
    post_.addResource("taa", w, h, gl::GL_RGBA16F);
    post_.addResource("dof", w, h, gl::GL_RGBA16F);
    post_.addResource("volfog", w, h, gl::GL_RGBA16F);
    post_.addResource("bloomA", w, h, gl::GL_RGBA16F);
    post_.addResource("bloomB", w, h, gl::GL_RGBA16F);
    post_.addResource("rays", w, h, gl::GL_RGBA16F);

    const float tx = 1.f / static_cast<float>(w), ty = 1.f / static_cast<float>(h);
    post_.addPass("taa", {"hdr", "taa"}, {"taa"}, [this](RenderGraph::PassContext& ctx) { drawTAA(ctx); }, /*feedback=*/true);
    post_.addPass("dof", {"taa", "depth"}, {"dof"}, [this](RenderGraph::PassContext& ctx) { drawDoF(ctx); });
    post_.addPass("volfog", {"dof", "depth"}, {"volfog"}, [this](RenderGraph::PassContext& ctx) { drawVolFog(ctx); });
    post_.addPass("bright", {"taa"}, {"bloomA"}, [this](RenderGraph::PassContext& ctx) { drawBloomBright(ctx); });
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
        std::fprintf(stderr, "PostChain: graph compile failed\n");
        return false;
    }
    return true;
}

void PostChain::drawFs(Material& mat, const std::function<void(Program&)>& setUniforms) {
    gl::disable(gl::Cap::DepthTest);
    gl::disable(gl::Cap::Blend);
    mat.bind();
    if (setUniforms) setUniforms(mat.program());
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
}

// binds a per-frame external (hdr/depth) that the graph only knows as a dummy resource
static void bindExternal(int unit, const Texture* t) {
    gl::activeTexture(gl::GL_TEXTURE0 + unit);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D, t ? t->id() : 0);
}

void PostChain::drawCopy(RenderGraph::PassContext& ctx, std::string_view in) {
    if (in == "hdr") bindExternal(0, curHdr_);
    else ctx.bindInput(0, in);
    drawFs(copyMat_, [](Program& p) { p.setUniform(p.uniformLocation("uTex"), 0); });
}

void PostChain::drawTAA(RenderGraph::PassContext& ctx) {
    if (!taaEnabled) {
        drawCopy(ctx, "hdr");
        return;
    }
    bindExternal(0, curHdr_);
    ctx.bindInput(1, "taa"); // previous accumulated frame (scratch copy)
    drawFs(taaMat_, [&](Program& p) {
        p.setUniform("uAlpha", frameIndex_ == 0 ? 1.f : taaBlend);
        p.setUniform("uTexel", math::Vec2(1.f / static_cast<float>(width_), 1.f / static_cast<float>(height_)));
        p.setUniform(p.uniformLocation("uCur"), 0);
        p.setUniform(p.uniformLocation("uHist"), 1);
    });
}

void PostChain::drawDoF(RenderGraph::PassContext& ctx) {
    if (!dofEnabled) {
        drawCopy(ctx, "taa");
        return;
    }
    const SceneView& s = *curScene_;
    ctx.bindInput(0, "taa");
    bindExternal(1, curDepth_);
    drawFs(dofMat_, [&](Program& p) {
        p.setUniform("uCamPos", s.camera->eye());
        p.setUniform("uInvViewProj", s.camera->viewProj(s.aspect).inverse());
        p.setUniform("uFocusDist", dofFocusDist);
        p.setUniform("uFocusRange", dofFocusRange);
        p.setUniform("uMaxCoc", math::Vec2(dofStrength / static_cast<float>(width_),
                                            dofStrength / static_cast<float>(height_)));
        p.setUniform(p.uniformLocation("uTaps"), math::clamp(dofTaps, 1, 32));
        p.setUniform(p.uniformLocation("uSrc"), 0);
        p.setUniform(p.uniformLocation("uDepth"), 1);
    });
}

void PostChain::drawVolFog(RenderGraph::PassContext& ctx) {
    if (!volFogEnabled) {
        drawCopy(ctx, "dof");
        return;
    }
    const SceneView& s = *curScene_;
    ctx.bindInput(0, "dof");
    bindExternal(1, curDepth_);
    const bool csm = s.cascades && s.cascades->cascadeCount() > 0;
    const bool shadowed = volFogShadows && (csm || s.shadowMap);
    gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitShadowMap);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D,
                                  s.shadowMap ? s.shadowMap->textureId() : dummyShadowTex_.id());
    gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitShadowMapArray);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D_ARRAY,
                                  csm ? s.cascades->textureId() : dummyShadowArrayTex_.id());
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
        p.setUniform(p.uniformLocation("uDepth"), 1);
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

void PostChain::drawBlur(RenderGraph::PassContext& ctx, std::string_view in, const math::Vec2& dir) {
    ctx.bindInput(0, in);
    drawFs(blurMat_, [&](Program& p) {
        p.setUniform("uDir", dir);
        p.setUniform(p.uniformLocation("uTex"), 0);
    });
}

void PostChain::drawBloomBright(RenderGraph::PassContext& ctx) {
    ctx.bindInput(0, "taa");
    drawFs(brightMat_, [&](Program& p) {
        p.setUniform("uThreshold", bloomThreshold);
        p.setUniform(p.uniformLocation("uHdr"), 0);
    });
}

void PostChain::drawRays(RenderGraph::PassContext& ctx) {
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

void PostChain::drawComposite(RenderGraph::PassContext& ctx, std::string_view bloomRes) {
    (void)ctx;
    out_->bind();
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

void PostChain::execute(const SceneView& scene, const Texture& hdr, const Texture& depth, Framebuffer& out) {
    if (width_ <= 0 || !scene.camera) return;
    applyCapabilities();
    curScene_ = &scene;
    curHdr_ = &hdr;
    curDepth_ = &depth;
    out_ = &out;
    post_.execute();
    if (taaEnabled) ++frameIndex_;
    else frameIndex_ = 0;
    curScene_ = nullptr;
    curHdr_ = nullptr;
    curDepth_ = nullptr;
    out_ = nullptr;
}

} // namespace fuhgl::render
