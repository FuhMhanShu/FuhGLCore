#include "fuhgl/render/pipeline.hpp"
#include "lighting_glsl.hpp"

#include "fuhgl/gl/state.hpp"

namespace fuhgl::render {
namespace {

constexpr const char* kForwardVert = R"GLSL(#version 330 core
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

constexpr const char* kForwardFrag = R"GLSL(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
)GLSL";

constexpr const char* kForwardFragTail = R"GLSL(
out vec4 FragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    FragColor = vec4(shade(vColor, N, vWorldPos, V), 1.0);
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
        const Material& mat = item.material ? *item.material : defaultMaterial_;
        mat.bind();
        Program& prog = const_cast<Program&>(mat.program());
        prog.setUniform("uModel", item.model);
        prog.setUniform("uViewProj", viewProj);
        prog.setUniform("uCamPos", scene.camera->eye());
        prog.setUniform("uAmbient", scene.ambient);
        prog.setUniform(prog.uniformLocation("uLightCount"), n);
        item.mesh->draw();
    }

    if (target_ && target_->id()) Framebuffer::unbind(Framebuffer::Target::Draw);
}

} // namespace fuhgl::render
