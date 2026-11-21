#include "fuhgl/render/light.hpp"

#include <algorithm>
#include <cmath>

namespace fuhgl::render {

Light Light::point(const math::Vec3& pos, const math::Vec3& color, float intensity, float range) {
    Light l;
    l.type = LightType::Point;
    l.position = pos;
    l.color = color;
    l.intensity = intensity;
    l.range = range;
    return l;
}

Light Light::directional(const math::Vec3& dir, const math::Vec3& color, float intensity) {
    Light l;
    l.type = LightType::Directional;
    l.direction = math::normalize(dir);
    l.color = color;
    l.intensity = intensity;
    return l;
}

Light Light::spot(const math::Vec3& pos, const math::Vec3& dir, const math::Vec3& color, float intensity,
                  float coneInner, float coneOuter) {
    Light l;
    l.type = LightType::Spot;
    l.position = pos;
    l.direction = math::normalize(dir);
    l.color = color;
    l.intensity = intensity;
    l.coneInner = coneInner;
    l.coneOuter = coneOuter;
    return l;
}

float Light::attenuation(float distance) const {
    if (type == LightType::Directional) return intensity;
    const float d = std::max(distance, 0.f);
    float atten = intensity / std::max(attenuationConstant + attenuationLinear * d + attenuationQuadratic * d * d, 1e-5f);
    if (range > 0.f) {
        const float t = math::clamp(d / range, 0.f, 1.f);
        atten *= 1.f - t * t * t * t; // smooth window to zero at the range limit
    }
    return atten;
}

float Light::coneFactor(const math::Vec3& toLight) const {
    if (type != LightType::Spot) return 1.f;
    const float c = math::dot(-toLight, math::normalize(direction));
    // cos decreases with angle, so cos(outer) < cos(inner)
    return math::smoothstep(std::cos(coneOuter), std::cos(coneInner), c);
}

int packLightBlock(const std::vector<Light>& lights, LightBlock& block) {
    const int n = std::min(static_cast<int>(lights.size()), kMaxLights);
    for (int i = 0; i < n; ++i) {
        const Light& l = lights[static_cast<size_t>(i)];
        block.posIntensity[i] = math::Vec4(l.position, l.intensity);
        block.color[i] = math::Vec4(l.color, 1.f);
        block.dirType[i] = math::Vec4(l.direction, static_cast<float>(static_cast<int>(l.type)));
        block.params0[i] = math::Vec4(l.attenuationConstant, l.attenuationLinear, l.attenuationQuadratic, l.range);
        block.params1[i] = math::Vec4(std::cos(l.coneInner), std::cos(l.coneOuter), 0.f, 0.f);
    }
    return n;
}

} // namespace fuhgl::render
