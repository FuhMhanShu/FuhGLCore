// Fallback backend for platforms without a native window implementation.
// Window::create() always returns false here.

#include "fuhgl/window.hpp"

namespace fuhgl {

struct Window::Impl {};

Window::Window() : impl_(std::make_unique<Impl>()) {}
Window::~Window() = default;
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::create(std::string_view, int, int, const WindowHints&) { return false; }
void Window::destroy() {}
bool Window::isOpen() const { return false; }
bool Window::shouldClose() const { return true; }
void Window::setShouldClose(bool) {}
void Window::pollEvents() {}
void Window::swapBuffers() {}
void Window::makeCurrent() {}
int Window::width() const { return 0; }
int Window::height() const { return 0; }
int Window::framebufferWidth() const { return 0; }
int Window::framebufferHeight() const { return 0; }
double Window::time() const { return 0.0; }
void Window::setTitle(std::string_view) {}
void Window::setResizeCallback(ResizeCallback) {}
void Window::setKeyCallback(KeyCallback) {}
void Window::setMouseButtonCallback(MouseButtonCallback) {}
void Window::setCursorPosCallback(CursorPosCallback) {}
void Window::setScrollCallback(ScrollCallback) {}
void Window::setCloseCallback(CloseCallback) {}

} // namespace fuhgl
