#include "fuhgl/render/shadow.hpp"
#include "fuhgl/render/mesh.hpp"

#include "fuhgl/cam/camera.hpp"
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

// --- F30: cascaded shadow maps ----------------------------------------------------

std::array<float, kMaxCascades + 1> cascadeSplits(float zNear, float zFar, int count, float lambda) {
    std::array<float, kMaxCascades + 1> s{};
    if (count < 1) count = 1;
    if (count > kMaxCascades) count = kMaxCascades;
    if (zFar <= zNear) zFar = zNear + 1.f;
    const float l = math::clamp(lambda, 0.f, 1.f);
    for (int i = 0; i <= count; ++i) {
        const float t = static_cast<float>(i) / count;
        const float logSplit = zNear * std::pow(zFar / zNear, t);
        const float uniSplit = zNear + (zFar - zNear) * t;
        s[i] = l * logSplit + (1.f - l) * uniSplit;
    }
    s[0] = zNear;
    s[count] = zFar;
    return s;
}

math::Mat4 cascadeViewProj(const Light& light, const Camera& camera, float aspect,
                           float splitNear, float splitFar,
                           const math::Vec3& sceneCenter, float sceneRadius) {
    if (light.type != LightType::Directional)
        return lightViewProj(light, sceneCenter, sceneRadius); // spot: one map for the whole range

    const math::Mat4 view = camera.view();
    const math::Mat4 proj = camera.projection(aspect);
    const math::Mat4 invView = view.inverse();
    const float tanHalfFovY = proj(1, 1) != 0.f ? 1.f / proj(1, 1) : 0.5f;
    const float a = (proj(0, 0) != 0.f && proj(1, 1) != 0.f) ? proj(1, 1) / proj(0, 0) : aspect;

    // 8 frustum-slice corners in world space
    const auto w2v = [](const math::Vec4& v) { return math::Vec3{v.x, v.y, v.z}; };
    math::Vec3 corners[8];
    int n = 0;
    for (float d : {splitNear, splitFar}) {
        const float hh = tanHalfFovY * d;
        const float hw = hh * a;
        for (float sx : {-1.f, 1.f})
            for (float sy : {-1.f, 1.f})
                corners[n++] = w2v(invView * math::Vec4{sx * hw, sy * hh, -d, 1.f});
    }

    const math::Vec3 dir = math::normalize(light.direction);
    const math::Vec3 eye = sceneCenter - dir * (sceneRadius + splitNear);
    const math::Mat4 lv = math::lookAt(eye, sceneCenter, safeUp(dir));
    math::Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (const math::Vec3& c : corners) {
        const math::Vec3 p = w2v(lv * math::Vec4{c, 1.f});
        mn = math::Vec3{std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
        mx = math::Vec3{std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
    }
    const float pad = 0.5f;
    return math::ortho(mn.x - pad, mx.x + pad, mn.y - pad, mx.y + pad, -mx.z, -mn.z + pad) * lv;
}

struct CascadeShadowMap::Impl {};

CascadeShadowMap::CascadeShadowMap() : impl_(std::make_unique<Impl>()) {}
CascadeShadowMap::~CascadeShadowMap() = default;
CascadeShadowMap::CascadeShadowMap(CascadeShadowMap&&) noexcept = default;
CascadeShadowMap& CascadeShadowMap::operator=(CascadeShadowMap&&) noexcept = default;

bool CascadeShadowMap::create(int size, int cascades) {
    size_ = size;
    cascades_ = math::clamp(cascades, 1, kMaxCascades);

    depthArray_.bind();
    depthArray_.texStorage3D(1, gl::GL_DEPTH_COMPONENT24, size, size, cascades_);
    depthArray_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    depthArray_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    depthArray_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    depthArray_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);

    fbo_.create();
    fbo_.setSize(size, size);
    fbo_.bind();
    gl::framebufferTextureLayer(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT, depthArray_.id(), 0, 0);
    gl::drawBuffer(gl::GL_NONE);
    gl::readBuffer(gl::GL_NONE);
    if (!fbo_.isComplete()) {
        std::fprintf(stderr, "CascadeShadowMap: FBO incomplete (0x%x)\n", fbo_.status());
        Framebuffer::unbind();
        return false;
    }
    Framebuffer::unbind();

    if (!depthMaterial_.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kDepthVert)) { std::fprintf(stderr, "CascadeShadowMap: depth VS: %s\n", vs.infoLog().c_str()); return false; }
    if (!fs.compile(kDepthFrag)) { std::fprintf(stderr, "CascadeShadowMap: depth FS: %s\n", fs.infoLog().c_str()); return false; }
    depthMaterial_.program().attach(vs);
    depthMaterial_.program().attach(fs);
    if (!depthMaterial_.program().link()) {
        std::fprintf(stderr, "CascadeShadowMap: depth link: %s\n", depthMaterial_.program().infoLog().c_str());
        return false;
    }
    return true;
}

void CascadeShadowMap::render(const SceneView& scene, const Camera& camera, const Light& light,
                              const math::Vec3& sceneCenter, float sceneRadius, float zNear, float zFar) {
    if (!fbo_.id() || size_ <= 0 || cascades_ <= 0) return;
    splits_ = cascadeSplits(zNear, zFar, cascades_);
    const float aspect = scene.height > 0 ? static_cast<float>(scene.width) / scene.height : 1.f;
    for (int i = 0; i < cascades_; ++i)
        vps_[i] = cascadeViewProj(light, camera, aspect, splits_[i], splits_[i + 1], sceneCenter, sceneRadius);

    gl::enable(gl::Cap::DepthTest);
    gl::depthFunc(gl::CompareFunc::Less);
    gl::depthMask(true);
    gl::enable(gl::Cap::CullFace);
    gl::cullFace(gl::Face::Front); // back faces: less shadow acne
    gl::frontFace(true);

    fbo_.bind(Framebuffer::Target::Draw);
    depthMaterial_.bind();
    Program& p = depthMaterial_.program();
    for (int i = 0; i < cascades_; ++i) {
        gl::framebufferTextureLayer(gl::GL_DRAW_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT, depthArray_.id(), 0, i);
        gl::viewport(0, 0, size_, size_);
        gl::clear(gl::ClearBit::Depth);
        p.setUniform("uLightVP", vps_[i]);
        for (const DrawItem& item : scene.items) {
            if (!item.mesh) continue;
            p.setUniform("uModel", item.model);
            p.setUniform("uInstanced", item.mesh->instanceCount() > 0 ? 1 : 0);
            item.mesh->draw();
        }
    }
    gl::cullFace(gl::Face::Back);
    Framebuffer::unbind(Framebuffer::Target::Draw);
}

} // namespace fuhgl::render
