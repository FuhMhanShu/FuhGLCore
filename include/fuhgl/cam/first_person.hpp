#pragma once

/// @file first_person.hpp
/// @brief First-person camera: yaw/pitch mouse look (no roll), WASD + Space/Ctrl movement.

#include "camera.hpp"

#include <cmath>

namespace fuhgl {

/// @brief First-person camera. yaw = 0 looks down -Z; pitch is clamped to +/-~89.9 deg.
class FirstPersonCamera : public Camera {
public:
    math::Vec3 position{0.f, 0.f, 3.f};
    float yaw = 0.f; // yaw = 0 looks down -Z
    float pitch = 0.f;
    float moveSpeed = 5.f;        ///< units per second
    float sprintMultiplier = 3.f; ///< applied while LeftShift is held
    float lookSpeed = 0.0025f;    ///< radians per mouse pixel
    float fovY = math::radians(60.f);
    float nearPlane = 0.1f;
    float farPlane = 1000.f;

    /// @brief Unit forward vector from yaw/pitch.
    math::Vec3 forward() const {
        const float cp = std::cos(pitch);
        return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    }
    /// @brief Unit right vector (horizontal).
    math::Vec3 right() const {
        return math::normalize(math::cross(forward(), {0.f, 1.f, 0.f}));
    }

    math::Mat4 view() const override {
        return math::lookAt(position, position + forward(), {0.f, 1.f, 0.f});
    }
    math::Mat4 projection(float aspect) const override {
        return math::perspective(fovY, aspect, nearPlane, farPlane);
    }
    math::Vec3 eye() const override { return position; }

    void onMouseMove(double dx, double dy) override {
        yaw += static_cast<float>(dx) * lookSpeed;
        pitch -= static_cast<float>(dy) * lookSpeed;
        constexpr float kMaxPitch = math::kPi * 0.5f - 0.001f;
        pitch = math::clamp(pitch, -kMaxPitch, kMaxPitch);
    }

    void update(float dt) override {
        const float speed = moveSpeed * (isDown(Key::LeftShift) ? sprintMultiplier : 1.f);
        const math::Vec3 f = forward();
        const math::Vec3 fwd = math::normalize(math::Vec3{f.x, 0.f, f.z}); // ground-plane forward
        const math::Vec3 r = right();
        math::Vec3 dir{};
        if (isDown(Key::W)) dir += fwd;
        if (isDown(Key::S)) dir -= fwd;
        if (isDown(Key::D)) dir += r;
        if (isDown(Key::A)) dir -= r;
        if (isDown(Key::Space)) dir += math::Vec3{0.f, 1.f, 0.f};
        if (isDown(Key::LeftControl)) dir -= math::Vec3{0.f, 1.f, 0.f};
        if (math::lengthSq(dir) > 0.f)
            position += math::normalize(dir) * (speed * dt);
    }
};

} // namespace fuhgl
