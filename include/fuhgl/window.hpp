#pragma once

/// @file window.hpp
/// @brief Native window with an OpenGL context — no GLFW/SDL required.
///
/// Window::create() opens a platform-native window (Win32/WGL, X11/GLX,
/// macOS Cocoa), creates an OpenGL context (3.3 core profile by default),
/// makes it current and calls fuhgl::gl::init() automatically.
/// Events are delivered through std::function callbacks set before/after
/// create(); pollEvents() dispatches them, swapBuffers() presents.

#include <functional>
#include <memory>
#include <string_view>

namespace fuhgl {

/// @brief Platform-independent key codes (mapped from the OS keysyms).
enum class Key : int {
    Unknown = 0,
    Escape, Space,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    Left, Right, Up, Down,
    LeftShift, RightShift, LeftControl, RightControl,
};

/// @brief Key/button action.
enum class Action : int { Press = 0, Release = 1, Repeat = 2 };

/// @brief Mouse buttons.
enum class MouseButton : int { Left = 0, Right = 1, Middle = 2 };

/// @brief Creation hints for Window::create().
struct WindowHints {
    int glMajor = 3;        ///< requested GL major version
    int glMinor = 3;        ///< requested GL minor version
    bool coreProfile = true;///< request a core profile context
    bool resizable = true;
    int samples = 0;        ///< MSAA samples (0 = off)
    bool vsync = true;
};

/// @brief Native window + OpenGL context (PIMPL, movable, non-copyable).
class Window {
public:
    using ResizeCallback = std::function<void(int width, int height)>;
    using KeyCallback = std::function<void(Key key, Action action)>;
    using MouseButtonCallback = std::function<void(MouseButton button, Action action)>;
    using CursorPosCallback = std::function<void(double xpos, double ypos)>;
    using ScrollCallback = std::function<void(double xoffset, double yoffset)>;
    using CloseCallback = std::function<void()>;

    Window();
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    /// @brief Creates the native window and its GL context and calls gl::init().
    /// @return false on failure (e.g. no display available in headless environments).
    bool create(std::string_view title, int width, int height, const WindowHints& hints = {});
    void destroy();

    bool isOpen() const;
    bool shouldClose() const;
    void setShouldClose(bool value);

    /// @brief Dispatches all pending OS events to the callbacks.
    void pollEvents();
    /// @brief Swaps the front/back buffers.
    void swapBuffers();
    /// @brief Makes the GL context of this window current on the calling thread.
    void makeCurrent();

    int width() const;  ///< window size in screen coordinates
    int height() const;
    int framebufferWidth() const;  ///< size in pixels (differs from width() on HiDPI)
    int framebufferHeight() const;
    /// @brief Seconds since create().
    double time() const;
    void setTitle(std::string_view title);

    void setResizeCallback(ResizeCallback cb);
    void setKeyCallback(KeyCallback cb);
    void setMouseButtonCallback(MouseButtonCallback cb);
    void setCursorPosCallback(CursorPosCallback cb);
    void setScrollCallback(ScrollCallback cb);
    void setCloseCallback(CloseCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
