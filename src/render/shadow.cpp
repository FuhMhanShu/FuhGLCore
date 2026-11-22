#include "fuhgl/render/shadow.hpp"

#include "fuhgl/cam/camera.hpp"

#include <cmath>

#include <cstdio>

namespace fuhgl::render {
namespace {

math::Vec3 safeUp(const math::Vec3& dir) {
    // avoid a degenerate lookAt when the light looks straight down/up
    return std::fabs(dir.y) > 0.99f ? math::Vec3{1.f, 0.f, 0.f} : math::Vec3{0.f, 1.f, 0.f};
}

} // namespace

// F46: ShadowMap/CascadeShadowMap are thin wrappers over ShadowTarget (shadow_kit.cpp)

bool ShadowMap::create(int size, bool moments) {
    return target_.create(ShadowTargetKind::Tex2D, size, 1, moments);
}

void ShadowMap::render(const SceneView& scene, const math::Mat4& lightVP) {
    lightVP_ = lightVP;
    target_.render(scene, lightVP, 0);
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

bool CascadeShadowMap::create(int size, int cascades, bool moments) {
    return target_.create(ShadowTargetKind::Tex2DArray, size, cascades, moments);
}

void CascadeShadowMap::render(const SceneView& scene, const Camera& camera, const Light& light,
                              const math::Vec3& sceneCenter, float sceneRadius, float zNear, float zFar) {
    const int cascades = cascadeCount();
    if (target_.textureId() == 0 || cascades <= 0) return;
    splits_ = cascadeSplits(zNear, zFar, cascades);
    const float aspect = scene.height > 0 ? static_cast<float>(scene.width) / scene.height : 1.f;
    for (int i = 0; i < cascades; ++i) {
        vps_[i] = cascadeViewProj(light, camera, aspect, splits_[i], splits_[i + 1], sceneCenter, sceneRadius);
        if (snapToTexels) // F46: stabilize the cascade on the texel grid (less swimming)
            vps_[i] = snapLightVPToTexels(vps_[i], target_.size(), sceneCenter);
    }
    for (int i = 0; i < cascades; ++i)
        target_.render(scene, vps_[i], i);
}

} // namespace fuhgl::render
