#include "fuhgl/render/forward_renderer.hpp"
#include "fuhgl/render/mesh.hpp"

#include "fuhgl/gl/info.hpp"
#include "fuhgl/gl/state.hpp"

#include <cstdio>

namespace fuhgl::render {

struct ForwardRenderer::Impl {};

ForwardRenderer::ForwardRenderer() : impl_(std::make_unique<Impl>()) {}
ForwardRenderer::~ForwardRenderer() = default;
ForwardRenderer::ForwardRenderer(ForwardRenderer&&) noexcept = default;
ForwardRenderer& ForwardRenderer::operator=(ForwardRenderer&&) noexcept = default;

bool ForwardRenderer::create(int width, int height) {
    if (!pipeline_.create(width, height)) {
        std::fprintf(stderr, "ForwardRenderer: pipeline create failed\n");
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
    width_ = width;
    height_ = height;
    if (width > 0 && height > 0) {
        targetTex_.bind();
        targetTex_.texStorage2D(1, gl::GL_RGBA8, width, height);
        targetDepth_.create();
        targetDepth_.bind();
        targetDepth_.storage(gl::GL_DEPTH_COMPONENT24, width, height);
        target_.create();
        target_.setSize(width, height);
        target_.bind();
        target_.attachTexture2D(0, targetTex_);
        target_.attachRenderbuffer(Framebuffer::Attachment::Depth, targetDepth_);
        if (!target_.isComplete()) {
            std::fprintf(stderr, "ForwardRenderer: target FBO incomplete (0x%x)\n", target_.status());
            Framebuffer::unbind();
            return false;
        }
        Framebuffer::unbind();
        pipeline_.setTarget(&target_);
    } else {
        pipeline_.setTarget(nullptr);
    }
    if (shadows_ && !shadowMap_.create(shadowSize_)) {
        std::fprintf(stderr, "ForwardRenderer: shadow map create failed\n");
        return false;
    }
    return true;
}

void ForwardRenderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    // recreate the own target (simplest correct approach)
    target_ = Framebuffer();
    targetTex_ = Texture(Texture::Target::Tex2D);
    targetDepth_ = Renderbuffer();
    create(width, height);
}

void ForwardRenderer::enableShadows(int mapSize) {
    shadowSize_ = mapSize;
    shadows_ = true;
    if (gl::isInitialized() && !shadowMap_.size() && !shadowMap_.create(mapSize)) {
        std::fprintf(stderr, "ForwardRenderer: shadow map create failed\n");
        shadows_ = false;
    }
}

void ForwardRenderer::enableCsmShadows(int mapSize, int cascades) {
    shadowSize_ = mapSize;
    csmCascades_ = cascades;
    csmEnabled_ = true;
    if (gl::isInitialized() && csm_.cascadeCount() <= 0 && !csm_.create(mapSize, cascades)) {
        std::fprintf(stderr, "ForwardRenderer: CSM create failed\n");
        csmEnabled_ = false;
    }
}

void ForwardRenderer::gate(Effect e, bool& flag) {
    if (flag && !effectSupported(e)) {
        const unsigned bit = 1u << static_cast<int>(e);
        if (!(warnedMask_ & bit)) {
            warnUnsupported("ForwardRenderer", e, glMajor_, glMinor_);
            warnedMask_ |= bit;
        }
        flag = false;
    }
}

void ForwardRenderer::applyCapabilities() {
    gate(Effect::Shadows, shadows_);
    gate(Effect::CsmShadows, csmEnabled_);
    gate(Effect::Lod, lodEnabled); // CPU-side: always supported, gate is a no-op
}

void ForwardRenderer::setEffectEnabled(Effect e, bool on) {
    switch (e) {
        case Effect::Shadows: shadows_ = on; break;
        case Effect::CsmShadows: csmEnabled_ = on; break;
        case Effect::Lod: lodEnabled = on; break;
        default: {
            const unsigned bit = 1u << static_cast<int>(e);
            if (on) requestedMask_ |= bit;
            else requestedMask_ &= ~bit;
            break;
        }
    }
    if (on && !effectSupported(e) && !((warnedMask_ >> static_cast<int>(e)) & 1u)) {
        warnUnsupported("ForwardRenderer", e, glMajor_, glMinor_);
        warnedMask_ |= 1u << static_cast<int>(e);
    }
    applyCapabilities();
}

bool ForwardRenderer::effectEnabled(Effect e) const {
    if (!effectSupported(e)) return false;
    switch (e) {
        case Effect::Shadows: return shadows_;
        case Effect::CsmShadows: return csmEnabled_;
        case Effect::Lod: return lodEnabled;
        default: return ((requestedMask_ >> static_cast<int>(e)) & 1u) != 0u;
    }
}

void ForwardRenderer::setAllEffects(bool on) {
    shadows_ = csmEnabled_ = lodEnabled = on;
    requestedMask_ = on ? ((1u << kEffectCount) - 1u) : 0u;
    applyCapabilities();
}

void ForwardRenderer::render(const Camera& camera, const std::vector<DrawItem>& items, int width, int height) {
    SceneView scene;
    scene.camera = &camera;
    // F39: graceful degradation — effects the GPU cannot run are skipped (warned once)
    applyCapabilities();
    if (env_ && !effectSupported(Effect::Ibl)) {
        const unsigned bit = 1u << static_cast<int>(Effect::Ibl);
        if (!(warnedMask_ & bit)) {
            warnUnsupported("ForwardRenderer", Effect::Ibl, glMajor_, glMinor_);
            warnedMask_ |= bit;
        }
        scene.env = nullptr;
    } else {
        scene.env = env_; // F29
    }
    scene.width = width;
    scene.height = height;
    scene.aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.f;
    scene.clearColor = clearColor_;
    scene.ambient = ambient_;
    scene.fogColor = fogColor_;
    scene.fogDensity = fogDensity_;
    scene.items = items;
    scene.lights = lights_;

    // shadow depth pass from the first directional/spot light (single map) or
    // first directional light (CSM, F30)
    if (csmEnabled_ && csm_.cascadeCount() > 0) {
        for (size_t i = 0; i < lights_.size(); ++i) {
            if (lights_[i].type == LightType::Directional) {
                csm_.render(scene, camera, lights_[i], shadowCenter_, shadowRadius_, shadowZNear_, shadowZFar_);
                scene.cascades = &csm_;
                scene.shadowLightIndex = static_cast<int>(i);
                scene.shadowStrength = shadowStrength_;
                break;
            }
        }
    } else if (shadows_ && shadowMap_.size() > 0) {
        for (size_t i = 0; i < lights_.size(); ++i) {
            if (lights_[i].type == LightType::Directional || lights_[i].type == LightType::Spot) {
                scene.lightVP = lightViewProj(lights_[i], shadowCenter_, shadowRadius_);
                shadowMap_.render(scene, scene.lightVP);
                scene.shadowMap = &shadowMap_;
                scene.shadowLightIndex = static_cast<int>(i);
                scene.shadowStrength = shadowStrength_;
                break;
            }
        }
    }

    if (width_ > 0 && height_ > 0 && (width != width_ || height != height_)) resize(width, height);
    scene.lodEnabled = lodEnabled; // F41
    pipeline_.render(scene);
    lastLodLevel_ = scene.lastLodLevel;
    scene.shadowMap = nullptr;
}

void ForwardRenderer::blitToScreen() {
    if (width_ > 0) pipeline_.blitToScreen();
}

} // namespace fuhgl::render
