#pragma once

/// @file camera.hpp
/// @brief Common camera interface and input plumbing (Window event attachment).

#include "../math.hpp"
#include "../window.hpp"

#include <array>

namespace fuhgl {

/// @brief Base class of all camera controllers.
///
/// Cameras are plain value types (no PIMPL): they hold only math state.
/// Call attach(window) once to feed Window events into the camera; the camera
/// then tracks pressed keys itself and update(dt) moves it.
class Camera {
public:
    virtual ~Camera() = default;

    /// @brief View matrix (world -> camera).
    virtual math::Mat4 view() const = 0;
    /// @brief Projection matrix for the given viewport aspect (width / height).
    virtual math::Mat4 projection(float aspect) const = 0;
    /// @brief Combined view-projection matrix (handy for frustum extraction / UBO upload).
    math::Mat4 viewProj(float aspect) const { return projection(aspect) * view(); }
    /// @brief Advance the camera by dt seconds using currently pressed keys.
    virtual void update(float dt) = 0;

    /// @brief Key event handler (default: tracks key-down state).
    virtual void onKey(Key key, Action action) { keys_[indexOf(key)] = action != Action::Release; }
    /// @brief Relative mouse movement in pixels.
    virtual void onMouseMove(double /*dx*/, double /*dy*/) {}
    /// @brief Scroll wheel movement.
    virtual void onScroll(double /*dx*/, double /*dy*/) {}
    /// @brief Mouse button event.
    virtual void onMouseButton(MouseButton /*button*/, Action /*action*/) {}

    /// @brief True while the key is held down.
    bool isDown(Key key) const { return keys_[indexOf(key)]; }

    /// @brief Connects the camera to a Window's input callbacks.
    /// @note This overwrites the Window's key/cursor/button/scroll callbacks.
    void attach(Window& window) {
        window.setKeyCallback([this](Key k, Action a) { onKey(k, a); });
        window.setCursorPosCallback([this](double x, double y) {
            const double dx = x - lastX_;
            const double dy = y - lastY_;
            lastX_ = x;
            lastY_ = y;
            if (hasLast_) onMouseMove(dx, dy);
            hasLast_ = true;
        });
        window.setMouseButtonCallback([this](MouseButton b, Action a) { onMouseButton(b, a); });
        window.setScrollCallback([this](double dx, double dy) { onScroll(dx, dy); });
    }

protected:
    static constexpr std::size_t indexOf(Key key) { return static_cast<std::size_t>(key); }

    std::array<bool, 64> keys_{};
    double lastX_ = 0.0, lastY_ = 0.0;
    bool hasLast_ = false;
};

} // namespace fuhgl
