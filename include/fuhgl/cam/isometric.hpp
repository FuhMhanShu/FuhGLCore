#pragma once

/// @file isometric.hpp
/// @brief Isometric camera: fixed 45 deg yaw / ~35.264 deg pitch, orthographic
/// projection, pan (right-drag or arrow keys) and zoom (scroll wheel).

#include "camera.hpp"

#include <cmath>

namespace fuhgl {

/// @brief Isometric/orthographic camera looking at `target` from a fixed angle.
class IsometricCamera : public Camera {
public:
    math::Vec3 target{0.f, 0.f, 0.f};
    float distance = 20.f;   ///< eye distance from target (affects near/far range only)
    float zoom = 1.f;         ///< > 1 zooms in
    float baseScale = 10.f;   ///< orthographic half-height at zoom == 1
    float yaw = math::radians(45.f);
    float pitch = 0.6154797086703873f; ///< atan(1/sqrt(2)) ~= 35.264 deg
    float panSpeed = 0.01f;   ///< world units per pixel, scaled by 1/zoom
    float keyPanSpeed = 10.f; ///< world units per second for arrow keys
    float nearPlane = -40.f;  ///< ortho near (negative: geometry behind eye allowed)
    float farPlane = 40.f;

    /// @brief Unit vector from target towards the eye.
    math::Vec3 eyeDir() const {
        const float cp = std::cos(pitch);
        return {std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
    }
    math::Vec3 eye() const { return target + eyeDir() * distance; }
    math::Vec3 forward() const { return -eyeDir(); }
    math::Vec3 right() const { return math::normalize(math::cross(forward(), {0.f, 1.f, 0.f})); }
    math::Vec3 up() const { return math::normalize(math::cross(right(), forward())); }

    math::Mat4 view() const override { return math::lookAt(eye(), target, {0.f, 1.f, 0.f}); }
    math::Mat4 projection(float aspect) const override {
        const float halfH = baseScale / zoom;
        const float halfW = halfH * aspect;
        return math::ortho(-halfW, halfW, -halfH, halfH, nearPlane, farPlane);
    }

    void onMouseButton(MouseButton button, Action action) override {
        rightDragging_ = button == MouseButton::Right && action != Action::Release;
    }
    void onMouseMove(double dx, double dy) override {
        if (!rightDragging_) return;
        const float s = panSpeed / zoom;
        target -= right() * (static_cast<float>(dx) * s);
        target += up() * (static_cast<float>(dy) * s);
    }
    void onScroll(double /*dx*/, double dy) override {
        zoom *= std::exp(static_cast<float>(dy) * 0.15f); // wheel up (dy > 0) zooms in
        zoom = math::clamp(zoom, 0.05f, 100.f);
    }

    void update(float dt) override {
        const float s = keyPanSpeed * dt / zoom;
        if (isDown(Key::Left)) target -= right() * s;
        if (isDown(Key::Right)) target += right() * s;
        if (isDown(Key::Up)) target += up() * s;
        if (isDown(Key::Down)) target -= up() * s;
    }

private:
    bool rightDragging_ = false;
};

} // namespace fuhgl
