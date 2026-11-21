#include "fuhgl/render/forward_renderer.hpp"
#include "fuhgl/render/mesh.hpp"

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

void ForwardRenderer::render(const Camera& camera, const std::vector<DrawItem>& items, int width, int height) {
    SceneView scene;
    scene.camera = &camera;
    scene.width = width;
    scene.height = height;
    scene.aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.f;
    scene.clearColor = clearColor_;
    scene.ambient = ambient_;
    scene.fogColor = fogColor_;
    scene.fogDensity = fogDensity_;
    scene.items = items;
    scene.lights = lights_;
    scene.env = env_; // F29

    // shadow depth pass from the first directional/spot light
    if (shadows_ && shadowMap_.size() > 0) {
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
    pipeline_.render(scene);
    scene.shadowMap = nullptr;
}

void ForwardRenderer::blitToScreen() {
    if (width_ > 0) pipeline_.blitToScreen();
}

} // namespace fuhgl::render
