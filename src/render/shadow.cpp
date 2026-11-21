#include "fuhgl/render/shadow.hpp"
#include "fuhgl/render/mesh.hpp"

#include "fuhgl/gl/state.hpp"

#include <cmath>

#include <cstdio>

namespace fuhgl::render {
namespace {

constexpr const char* kDepthVert = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in mat4 aInstanceModel; // F24 instancing (unused when uInstanced == 0)
uniform mat4 uModel;
uniform mat4 uLightVP;
uniform int uInstanced;
void main() {
    mat4 model = (uInstanced == 1) ? (uModel * aInstanceModel) : uModel;
    gl_Position = uLightVP * model * vec4(aPos, 1.0);
}
)GLSL";

constexpr const char* kDepthFrag = R"GLSL(#version 330 core
void main() {}
)GLSL";

math::Vec3 safeUp(const math::Vec3& dir) {
    // avoid a degenerate lookAt when the light looks straight down/up
    return std::fabs(dir.y) > 0.99f ? math::Vec3{1.f, 0.f, 0.f} : math::Vec3{0.f, 1.f, 0.f};
}

} // namespace

struct ShadowMap::Impl {};

ShadowMap::ShadowMap() : impl_(std::make_unique<Impl>()) {}
ShadowMap::~ShadowMap() = default;
ShadowMap::ShadowMap(ShadowMap&&) noexcept = default;
ShadowMap& ShadowMap::operator=(ShadowMap&&) noexcept = default;

bool ShadowMap::create(int size) {
    size_ = size;
    depthTex_.bind();
    depthTex_.texStorage2D(1, gl::GL_DEPTH_COMPONENT24, size, size);
    depthTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR); // HW 2x2 PCF on top of the shader 3x3
    depthTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    depthTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    depthTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);

    fbo_.create();
    fbo_.setSize(size, size);
    fbo_.bind();
    fbo_.attachTexture2D(Framebuffer::Attachment::Depth, depthTex_);
    gl::drawBuffer(gl::GL_NONE); // depth-only FBO
    gl::readBuffer(gl::GL_NONE);
    if (!fbo_.isComplete()) {
        std::fprintf(stderr, "ShadowMap: FBO incomplete (0x%x)\n", fbo_.status());
        Framebuffer::unbind();
        return false;
    }
    Framebuffer::unbind();

    if (!depthMaterial_.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kDepthVert)) { std::fprintf(stderr, "ShadowMap: depth VS: %s\n", vs.infoLog().c_str()); return false; }
    if (!fs.compile(kDepthFrag)) { std::fprintf(stderr, "ShadowMap: depth FS: %s\n", fs.infoLog().c_str()); return false; }
    depthMaterial_.program().attach(vs);
    depthMaterial_.program().attach(fs);
    if (!depthMaterial_.program().link()) {
        std::fprintf(stderr, "ShadowMap: depth link: %s\n", depthMaterial_.program().infoLog().c_str());
        return false;
    }
    return true;
}

void ShadowMap::render(const SceneView& scene, const math::Mat4& lightVP) {
    if (!fbo_.id() || size_ <= 0) return;
    lightVP_ = lightVP;

    fbo_.bind(Framebuffer::Target::Draw);
    gl::viewport(0, 0, size_, size_);
    gl::clear(gl::ClearBit::Depth);

    gl::enable(gl::Cap::DepthTest);
    gl::depthFunc(gl::CompareFunc::Less);
    gl::depthMask(true);
    gl::enable(gl::Cap::CullFace);
    gl::cullFace(gl::Face::Front); // draw back faces: less shadow acne
    gl::frontFace(true);

    depthMaterial_.bind();
    Program& p = depthMaterial_.program();
    p.setUniform("uLightVP", lightVP);
    for (const DrawItem& item : scene.items) {
        if (!item.mesh) continue;
        p.setUniform("uModel", item.model);
        p.setUniform("uInstanced", item.mesh->instanceCount() > 0 ? 1 : 0);
        item.mesh->draw();
    }

    gl::cullFace(gl::Face::Back);
    Framebuffer::unbind(Framebuffer::Target::Draw);
}

math::Mat4 lightView(const Light& light, const math::Vec3& sceneCenter, float sceneRadius, float nearPlane) {
    if (light.type == LightType::Directional) {
        const math::Vec3 dir = math::normalize(light.direction);
        const math::Vec3 eye = sceneCenter - dir * (sceneRadius + nearPlane);
        return math::lookAt(eye, sceneCenter, safeUp(dir));
    }
    // spot (point lights would need a cube map — not supported here)
    const math::Vec3 dir = math::normalize(light.direction);
    return math::lookAt(light.position, light.position + dir, safeUp(dir));
}

math::Mat4 lightProjection(const Light& light, float aspect, float nearPlane, float farPlane) {
    if (light.type == LightType::Directional) {
        // set by the caller via lightViewProj (needs the radius); keep a sane default here
        return math::ortho(-1.f, 1.f, -1.f, 1.f, nearPlane, farPlane > 0.f ? farPlane : 100.f);
    }
    const float fovY = 2.f * light.coneOuter;
    const float far = farPlane > 0.f ? farPlane : (light.range > 0.f ? light.range : 100.f);
    return math::perspective(fovY, aspect, nearPlane, far);
}

math::Mat4 lightViewProj(const Light& light, const math::Vec3& sceneCenter, float sceneRadius,
                         float nearPlane, float farPlane) {
    if (light.type == LightType::Directional) {
        const float far = farPlane > 0.f ? farPlane : 2.f * sceneRadius + nearPlane;
        const math::Vec3 dir = math::normalize(light.direction);
        const math::Vec3 eye = sceneCenter - dir * (sceneRadius + nearPlane);
        return math::ortho(-sceneRadius, sceneRadius, -sceneRadius, sceneRadius, nearPlane, far) *
               math::lookAt(eye, sceneCenter, safeUp(dir));
    }
    return lightProjection(light, 1.f, nearPlane, farPlane) * lightView(light, sceneCenter, sceneRadius, nearPlane);
}

} // namespace fuhgl::render
