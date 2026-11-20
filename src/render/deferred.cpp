#include "fuhgl/render/deferred.hpp"
#include "lighting_glsl.hpp"

#include "fuhgl/gl/state.hpp"
#include "fuhgl/gl/error.hpp"

#include <cstdio>
#include <cstdlib>

namespace fuhgl::render {
namespace {

// --- G-buffer geometry pass ----------------------------------------------------
constexpr const char* kGBufferVert = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    gl_Position = uViewProj * wp;
}
)GLSL";

constexpr const char* kGBufferFrag = R"GLSL(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
void main() {
    oAlbedo = vec4(vColor, 1.0);
    oNormal = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    oPosition = vec4(vWorldPos, 1.0); // alpha = 1 marks "geometry present"
}
)GLSL";

// --- fullscreen lighting pass ----------------------------------------------------
constexpr const char* kLightVert = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kLightFragHead = R"GLSL(#version 330 core
in vec2 vUV;
uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uPosition;
uniform vec3 uSkyTop;
uniform vec3 uSkyBottom;
)GLSL";

constexpr const char* kLightFragTail = R"GLSL(
out vec4 FragColor;
void main() {
    vec4 pos = texture(uPosition, vUV);
    if (pos.a < 0.5) {
        // background: procedural sky gradient
        FragColor = vec4(mix(uSkyBottom, uSkyTop, vUV.y), 1.0);
        return;
    }
    vec3 worldPos = pos.rgb;
    vec3 N = normalize(texture(uNormal, vUV).rgb * 2.0 - 1.0);
    vec3 albedo = texture(uAlbedo, vUV).rgb;
    vec3 V = normalize(uCamPos - worldPos);
    FragColor = vec4(shade(albedo, N, worldPos, V), 1.0);
}
)GLSL";

void makeTarget(Texture& tex, gl::GLenum internal, int w, int h, int msaa) {
    if (msaa > 0) {
        tex = Texture(Texture::Target::Tex2DMultisample);
        tex.bind();
        tex.storageMultisample2D(msaa, internal, w, h);
    } else {
        tex = Texture(Texture::Target::Tex2D);
        tex.bind();
        tex.texStorage2D(1, internal, w, h);
        tex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
        tex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
        tex.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
        tex.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    }
}

} // namespace

struct DeferredPipeline::Impl {};

DeferredPipeline::DeferredPipeline() : impl_(std::make_unique<Impl>()) {}
DeferredPipeline::~DeferredPipeline() = default;
DeferredPipeline::DeferredPipeline(DeferredPipeline&&) noexcept = default;
DeferredPipeline& DeferredPipeline::operator=(DeferredPipeline&&) noexcept = default;

bool DeferredPipeline::create(int width, int height, int msaaSamples) {
    width_ = width;
    height_ = height;
    msaa_ = msaaSamples;

    // G-buffer textures (multisampled when requested)
    makeTarget(texAlbedo_, gl::GL_RGBA8, width, height, msaa_);
    makeTarget(texNormal_, gl::GL_RGBA8, width, height, msaa_);
    makeTarget(texPosition_, gl::GL_RGBA16F, width, height, msaa_);

    gbuffer_.create();
    gbuffer_.setSize(width, height);
    gbuffer_.bind();
    const gl::GLenum target = msaa_ > 0 ? gl::GL_TEXTURE_2D_MULTISAMPLE : gl::GL_TEXTURE_2D;
    (void)target;
    gbuffer_.attachTexture2D(0, texAlbedo_);
    gbuffer_.attachTexture2D(1, texNormal_);
    gbuffer_.attachTexture2D(2, texPosition_);
    if (msaa_ > 0) {
        depthMsaa_.create();
        depthMsaa_.bind();
        depthMsaa_.storageMultisample(msaa_, gl::GL_DEPTH24_STENCIL8, width, height);
        gbuffer_.attachRenderbuffer(Framebuffer::Attachment::Depth, depthMsaa_);
    } else {
        depth_.create();
        depth_.bind();
        depth_.storage(gl::GL_DEPTH_COMPONENT24, width, height);
        gbuffer_.attachRenderbuffer(Framebuffer::Attachment::Depth, depth_);
    }
    if (!gbuffer_.isComplete()) {
        std::fprintf(stderr, "DeferredPipeline: G-buffer incomplete (0x%x)\n", gbuffer_.status());
        return false;
    }
    std::fprintf(stderr, "DeferredPipeline: G-buffer ok\n");

    // resolve target (only for MSAA)
    if (msaa_ > 0) {
        makeTarget(texResolveAlbedo_, gl::GL_RGBA8, width, height, 0);
        makeTarget(texResolveNormal_, gl::GL_RGBA8, width, height, 0);
        makeTarget(texResolvePosition_, gl::GL_RGBA16F, width, height, 0);
        resolve_.create();
        resolve_.setSize(width, height);
        resolve_.bind();
        resolve_.attachTexture2D(0, texResolveAlbedo_);
        resolve_.attachTexture2D(1, texResolveNormal_);
        resolve_.attachTexture2D(2, texResolvePosition_);
        if (!resolve_.isComplete()) {
            std::fprintf(stderr, "DeferredPipeline: resolve FBO incomplete (0x%x)\n", resolve_.status());
            return false;
        }
    }

    // lighting output
    makeTarget(texOutput_, gl::GL_RGBA8, width, height, 0);
    output_.create();
    output_.setSize(width, height);
    output_.bind();
    output_.attachTexture2D(0, texOutput_);
    if (!output_.isComplete()) {
        std::fprintf(stderr, "DeferredPipeline: output FBO incomplete (0x%x)\n", output_.status());
        return false;
    }
    Framebuffer::unbind();
    std::fprintf(stderr, "DeferredPipeline: output FBO ok\n");

    // default G-buffer material
    if (!defaultMaterial_.create()) { std::fprintf(stderr, "DeferredPipeline: default material create failed\n"); return false; }
    {
        Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
        if (!vs.compile(kGBufferVert)) { std::fprintf(stderr, "DeferredPipeline: gbuf VS: %s\n", vs.infoLog().c_str()); return false; }
        if (!fs.compile(kGBufferFrag)) { std::fprintf(stderr, "DeferredPipeline: gbuf FS: %s\n", fs.infoLog().c_str()); return false; }
        defaultMaterial_.program().attach(vs);
        defaultMaterial_.program().attach(fs);
        if (!defaultMaterial_.program().link()) {
            std::fprintf(stderr, "DeferredPipeline: gbuf link: %s\n", defaultMaterial_.program().infoLog().c_str());
            return false;
        }
        defaultMaterial_.program().bindUniformBlock("LightBlock", 0); // not used by gbuf pass, harmless
    }
    std::fprintf(stderr, "DeferredPipeline: gbuffer material ok\n");

    // lighting material (fullscreen triangle)
    if (!lightingMaterial_.create()) return false;
    {
        Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
        if (!vs.compile(kLightVert)) { std::fprintf(stderr, "DeferredPipeline: light VS: %s\n", vs.infoLog().c_str()); return false; }
        const std::string fragSrc = std::string(kLightFragHead) + kLightingUniforms + kLightingLoop + kLightFragTail;
        if (!fs.compile(fragSrc.c_str())) { std::fprintf(stderr, "DeferredPipeline: light FS: %s\n", fs.infoLog().c_str()); return false; }
        lightingMaterial_.program().attach(vs);
        lightingMaterial_.program().attach(fs);
        if (!lightingMaterial_.program().link()) {
            std::fprintf(stderr, "DeferredPipeline: light link: %s\n", lightingMaterial_.program().infoLog().c_str());
            return false;
        }
        lightingMaterial_.program().bindUniformBlock("LightBlock", 0);
    }
    std::fprintf(stderr, "DeferredPipeline: lighting material ok\n");
    lightingMaterial_.setTexture(0, texAlbedo_);
    lightingMaterial_.setTexture(1, texNormal_);
    lightingMaterial_.setTexture(2, texPosition_);

    if (!lightsUbo_.create()) return false;
    lightsUbo_.bindBase(0);
    fullscreenVao_.create();
    fullscreenVao_.bind();
    return true;
}

void DeferredPipeline::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    // simplest correct approach: recreate everything
    *this = DeferredPipeline();
    create(width, height, msaa_);
}

void DeferredPipeline::render(const SceneView& scene) {
    if (!scene.camera || width_ <= 0) return;
    auto& f = gl::functions();

    // upload lights
    LightBlock block{};
    const int n = scene.lights.size() > kMaxLights ? kMaxLights : static_cast<int>(scene.lights.size());
    for (int i = 0; i < n; ++i) {
        block.posIntensity[i] = math::Vec4(scene.lights[i].position, scene.lights[i].intensity);
        block.color[i] = math::Vec4(scene.lights[i].color, 1.f);
    }
    lightsUbo_.upload(block);
    lightsUbo_.bindBase(0);

    // --- pass 1: G-buffer ---
    gbuffer_.bind();
    const gl::GLenum drawBufs[3] = {gl::GL_COLOR_ATTACHMENT0, gl::GL_COLOR_ATTACHMENT1, gl::GL_COLOR_ATTACHMENT2};
    gl::drawBuffers(3, drawBufs);
    gl::viewport(0, 0, width_, height_);
    const float clearZero[4] = {0.f, 0.f, 0.f, 0.f};
    const float clearAlbedo[4] = {scene.clearColor.x, scene.clearColor.y, scene.clearColor.z, 1.f};
    gl::clearBufferfv(gl::GL_COLOR, 0, clearAlbedo);
    gl::clearBufferfv(gl::GL_COLOR, 1, clearZero);
    gl::clearBufferfv(gl::GL_COLOR, 2, clearZero); // position alpha 0 = background
    gl::clear(gl::ClearBit::Depth);

    const math::Mat4 viewProj = scene.camera->viewProj(scene.aspect);
    for (const DrawItem& item : scene.items) {
        if (!item.mesh) continue;
        const Material& mat = item.material ? *item.material : defaultMaterial_;
        mat.bind();
        Program& prog = const_cast<Program&>(mat.program());
        prog.setUniform("uModel", item.model);
        prog.setUniform("uViewProj", viewProj);
        item.mesh->draw();
    }

    // --- MSAA resolve ---
    if (msaa_ > 0) {
        for (int i = 0; i < 3; ++i) {
            const gl::GLenum att = gl::GL_COLOR_ATTACHMENT0 + i;
            f.glReadBuffer(att);
            const gl::GLenum dst[1] = {att};
            // bind resolve as draw fb, gbuffer as read fb
            f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, gbuffer_.id());
            f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, resolve_.id());
            gl::drawBuffers(1, dst);
            f.glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, gl::GL_COLOR_BUFFER_BIT, gl::GL_NEAREST);
        }
        f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, 0);
        f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, 0);
        // lighting samples the resolved textures
        lightingMaterial_.setTexture(0, texResolveAlbedo_);
        lightingMaterial_.setTexture(1, texResolveNormal_);
        lightingMaterial_.setTexture(2, texResolvePosition_);
    }

    // --- pass 2: lighting (fullscreen) ---
    output_.bind();
    const gl::GLenum outBuf[1] = {gl::GL_COLOR_ATTACHMENT0};
    gl::drawBuffers(1, outBuf);
    gl::viewport(0, 0, width_, height_);
    const float outClear[4] = {scene.clearColor.x, scene.clearColor.y, scene.clearColor.z, 1.f};
    gl::clearBufferfv(gl::GL_COLOR, 0, outClear);

    lightingMaterial_.bind(); // binds program + gbuffer textures (units 0..2)
    Program& lp = lightingMaterial_.program();
    lp.setUniform(lp.uniformLocation("uAlbedo"), 0);
    lp.setUniform(lp.uniformLocation("uNormal"), 1);
    lp.setUniform(lp.uniformLocation("uPosition"), 2);
    lp.setUniform("uCamPos", scene.camera->eye());
    lp.setUniform("uAmbient", scene.ambient);
    lp.setUniform(lp.uniformLocation("uLightCount"), n);
    lp.setUniform("uSkyTop", skyTop);
    lp.setUniform("uSkyBottom", skyBottom);
    if (std::getenv("FUHGL_DEBUG")) {
        const gl::GLint loc = lp.uniformLocation("uLightCount");
        const gl::GLuint blockIdx = f.glGetUniformBlockIndex ? f.glGetUniformBlockIndex(lp.id(), "LightBlock") : 999u;
        gl::GLint locVal = -1;
        using GetIvFn = void (*)(gl::GLuint, gl::GLint, gl::GLint*);
        if (auto p = reinterpret_cast<GetIvFn>(gl::procAddress("glGetUniformiv")))
            p(lp.id(), loc, &locVal);
        std::fprintf(stderr, "DeferredPipeline[debug]: uLightCount loc=%d value=%d blockIdx=%u err=0x%x ubo=%u\n",
                     loc, locVal, blockIdx, gl::lastError(), lightsUbo_.id());
    }
    fullscreenVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);

    Framebuffer::unbind();
}

} // namespace fuhgl::render
