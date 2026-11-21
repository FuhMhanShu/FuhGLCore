#pragma once

/// @file free_camera.hpp
/// @brief Free 6-DOF fly camera: full 3D movement, roll via Q/E.

#include "camera.hpp"

#include <cmath>

namespace fuhgl {

/// @brief Free-fly camera with roll. Movement is along the camera's own axes:
/// W/S forward/back (3D), A/D strafe, Space/LeftControl up/down along view up,
/// Q/E roll. Mouse look = yaw + pitch.
class FreeCamera : public Camera {
public:
    math::Vec3 position{0.f, 0.f, 3.f};
    float yaw = 0.f; // yaw = 0 looks down -Z
    float pitch = 0.f;
    float roll = 0.f;
    float moveSpeed = 5.f;
    float sprintMultiplier = 3.f;
    float rollSpeed = 1.5f;      ///< radians per second while Q/E held
    float lookSpeed = 0.0025f;
    float fovY = math::radians(60.f);
    float nearPlane = 0.1f;
    float farPlane = 1000.f;

    /// @brief Unit forward vector from yaw/pitch (roll does not change it).
    math::Vec3 forward() const {
        const float cp = std::cos(pitch);
        return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    }
    math::Vec3 right() const { return math::normalize(math::cross(forward(), {0.f, 1.f, 0.f})); }
    math::Vec3 up() const { return math::normalize(math::cross(right(), forward())); }

    /// @brief View matrix: inverse of T * Rz(roll) * Ry(yaw) * Rx(pitch).
    math::Mat4 view() const override {
        return math::rotateX(-pitch) * math::rotateY(-yaw) * math::rotateZ(-roll) *
               math::translate(-position);
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
        if (isDown(Key::Q)) roll -= rollSpeed * dt;
        if (isDown(Key::E)) roll += rollSpeed * dt;

        const float speed = moveSpeed * (isDown(Key::LeftShift) ? sprintMultiplier : 1.f);
        const math::Vec3 f = forward();
        const math::Vec3 r = right();
        const math::Vec3 u = up();
        math::Vec3 dir{};
        if (isDown(Key::W)) dir += f;
        if (isDown(Key::S)) dir -= f;
        if (isDown(Key::D)) dir += r;
        if (isDown(Key::A)) dir -= r;
        if (isDown(Key::Space)) dir += u;
        if (isDown(Key::LeftControl)) dir -= u;
        if (math::lengthSq(dir) > 0.f)
            position += math::normalize(dir) * (speed * dt);
    }
};

} // namespace fuhgl
