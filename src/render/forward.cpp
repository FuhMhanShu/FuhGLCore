#include "fuhgl/render/pipeline.hpp"
#include "lighting_glsl.hpp"
#include "pipeline_internal.hpp"

#include "fuhgl/gl/state.hpp"

namespace fuhgl::render {
namespace {

constexpr const char* kForwardVert = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    vUV = aUV;
    gl_Position = uViewProj * wp;
}
)GLSL";

constexpr const char* kForwardFrag = R"GLSL(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;
uniform vec3 uAlbedo;
uniform vec3 uEmissive;
uniform int uUseTex;
uniform sampler2D uMatTex;
)GLSL";

constexpr const char* kForwardFragTail = R"GLSL(
out vec4 FragColor;
void main() {
    vec3 albedo = uAlbedo * vColor;
    if (uUseTex == 1) albedo *= texture(uMatTex, vUV).rgb;
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    FragColor = vec4(shade(albedo, N, vWorldPos, V) + uEmissive, 1.0);
}
)GLSL";

} // namespace

struct ForwardPipeline::Impl {};

ForwardPipeline::ForwardPipeline() : impl_(std::make_unique<Impl>()) {}
ForwardPipeline::~ForwardPipeline() = default;
ForwardPipeline::ForwardPipeline(ForwardPipeline&&) noexcept = default;
ForwardPipeline& ForwardPipeline::operator=(ForwardPipeline&&) noexcept = default;

bool ForwardPipeline::create() {
    if (!defaultMaterial_.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kForwardVert)) return false;
    const std::string fragSrc = std::string(kForwardFrag) + kLightingUniforms + kLightingLoop + kForwardFragTail;
    if (!fs.compile(fragSrc.c_str())) return false;
    defaultMaterial_.program().attach(vs);
    defaultMaterial_.program().attach(fs);
    if (!defaultMaterial_.program().link()) return false;
    defaultMaterial_.program().bindUniformBlock("LightBlock", 0);
    if (!lightsUbo_.create()) return false;
    lightsUbo_.bindBase(0);
    return true;
}

void ForwardPipeline::blitToScreen() {
    if (!target_ || !target_->id()) return;
    gl::GLint vp[4] = {0, 0, 0, 0};
    gl::functions().glGetIntegerv(gl::GL_VIEWPORT, vp);
    Framebuffer::blitToDefault(*target_, vp[2] > 0 ? vp[2] : target_->width(),
                               vp[3] > 0 ? vp[3] : target_->height());
}

void ForwardPipeline::render(const SceneView& scene) {
    if (!scene.camera) return;

    // The pipeline owns the GL state it needs: depth test + CCW back-face culling.
    gl::enable(gl::Cap::DepthTest);
    gl::depthFunc(gl::CompareFunc::Less);
    gl::depthMask(true);
    gl::enable(gl::Cap::CullFace);
    gl::cullFace(gl::Face::Back);
    gl::frontFace(true);

    // upload lights
    LightBlock block{};
    const int n = scene.lights.size() > kMaxLights ? kMaxLights : static_cast<int>(scene.lights.size());
    for (int i = 0; i < n; ++i) {
        block.posIntensity[i] = math::Vec4(scene.lights[i].position, scene.lights[i].intensity);
        block.color[i] = math::Vec4(scene.lights[i].color, 1.f);
    }
    lightsUbo_.upload(block);
    lightsUbo_.bindBase(0);

    if (target_ && target_->id()) {
        target_->bind(Framebuffer::Target::Draw);
    }
    gl::viewport(0, 0, scene.width, scene.height);
    gl::clearColor(scene.clearColor.x, scene.clearColor.y, scene.clearColor.z, 1.f);
    gl::clear(gl::ClearBit::Color | gl::ClearBit::Depth);

    const math::Mat4 viewProj = scene.camera->viewProj(scene.aspect);
    for (const DrawItem& item : scene.items) {
        if (!item.mesh) continue;
        detail::bindItem(defaultMaterial_, item, item.model, viewProj);
        Program& prog = const_cast<Program&>(
            (item.material && item.material->builtinShader) ? defaultMaterial_.program()
                                                            : (item.material ? *item.material : defaultMaterial_).program());
        prog.setUniform("uCamPos", scene.camera->eye());
        prog.setUniform("uAmbient", scene.ambient);
        prog.setUniform(prog.uniformLocation("uLightCount"), n);
        item.mesh->draw();
    }

    if (target_ && target_->id()) Framebuffer::unbind(Framebuffer::Target::Draw);
}

} // namespace fuhgl::render
