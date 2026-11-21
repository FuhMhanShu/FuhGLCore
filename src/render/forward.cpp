#include "fuhgl/render/pipeline.hpp"
#include "fuhgl/render/shadow.hpp"
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
layout(location = 4) in mat4 aInstanceModel; // F24 instancing (unused when uInstanced == 0)
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform int uInstanced;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;
void main() {
    mat4 model = (uInstanced == 1) ? (uModel * aInstanceModel) : uModel;
    vec4 wp = model * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = mat3(model) * aNormal;
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
uniform vec3 uFogColor;
uniform float uFogDensity;
)GLSL";

constexpr const char* kForwardFragTail = R"GLSL(
out vec4 FragColor;
void main() {
    vec3 albedo = uAlbedo * vColor;
    if (uUseTex == 1) albedo *= texture(uMatTex, vUV).rgb;
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 lit = shade(albedo, N, vWorldPos, V) + uEmissive;
    if (uFogDensity > 0.0) {
        float d = length(vWorldPos - uCamPos);
        lit = mix(lit, uFogColor, 1.0 - exp(-uFogDensity * d));
    }
    FragColor = vec4(lit, 1.0);
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
    const std::string fragSrc = std::string(kForwardFrag) + kLightingUniforms + kShadowUniforms + kShadowFunc +
                                kLightingLoop + kForwardFragTail;
    if (!fs.compile(fragSrc.c_str())) return false;
    defaultMaterial_.program().attach(vs);
    defaultMaterial_.program().attach(fs);
    if (!defaultMaterial_.program().link()) return false;
    defaultMaterial_.program().bindUniformBlock("LightBlock", 0);
    if (!lightsUbo_.create()) return false;
    lightsUbo_.bindBase(0);
    // 1x1 dummy depth texture: keeps the uShadowMap sampler bound even when shadows are off
    dummyShadowTex_.bind();
    dummyShadowTex_.texStorage2D(1, gl::GL_DEPTH_COMPONENT24, 1, 1);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
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
    const int n = packLightBlock(scene.lights, block);
    lightsUbo_.upload(block);
    lightsUbo_.bindBase(0);

    if (target_ && target_->id()) {
        target_->bind(Framebuffer::Target::Draw);
    }
    gl::viewport(0, 0, scene.width, scene.height);
    gl::clearColor(scene.clearColor.x, scene.clearColor.y, scene.clearColor.z, 1.f);
    gl::clear(gl::ClearBit::Color | gl::ClearBit::Depth);

    const math::Mat4 viewProj = scene.camera->viewProj(scene.aspect);
    // shadow map on texture unit 4 (dummy 1x1 when disabled so the sampler stays bound)
    const gl::GLuint shadowTexId = scene.shadowMap ? scene.shadowMap->textureId() : dummyShadowTex_.id();
    gl::activeTexture(gl::GL_TEXTURE0 + 4);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D, shadowTexId);
    for (const DrawItem& item : scene.items) {
        if (!item.mesh) continue;
        detail::bindItem(defaultMaterial_, item, item.model, viewProj);
        Program& prog = const_cast<Program&>(
            (item.material && item.material->builtinShader) ? defaultMaterial_.program()
                                                            : (item.material ? *item.material : defaultMaterial_).program());
        prog.setUniform("uCamPos", scene.camera->eye());
        prog.setUniform("uAmbient", scene.ambient);
        prog.setUniform(prog.uniformLocation("uLightCount"), n);
        prog.setUniform(prog.uniformLocation("uShadowEnabled"), scene.shadowMap ? 1 : 0);
        prog.setUniform(prog.uniformLocation("uShadowMap"), 4);
        prog.setUniform("uLightVP", scene.lightVP);
        prog.setUniform(prog.uniformLocation("uShadowLightIndex"), scene.shadowLightIndex);
        prog.setUniform("uShadowStrength", scene.shadowStrength);
        prog.setUniform("uFogColor", scene.fogColor);
        prog.setUniform("uFogDensity", scene.fogDensity);
        item.mesh->draw();
    }

    if (target_ && target_->id()) Framebuffer::unbind(Framebuffer::Target::Draw);
}

} // namespace fuhgl::render
