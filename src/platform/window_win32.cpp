// Win32/WGL backend for fuhgl::Window. Uses only system libraries
// (user32/gdi32/opengl32); wglCreateContextAttribsARB is obtained at runtime
// via wglGetProcAddress through fuhgl::gl::procAddress().

#include "fuhgl/window.hpp"
#include "fuhgl/gl/loader.hpp"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <string>

namespace fuhgl {
namespace {

constexpr const wchar_t* kClassName = L"FuhGLCoreWindow";

Key mapVKey(WPARAM vk) {
    switch (vk) {
        case VK_ESCAPE: return Key::Escape;
        case VK_SPACE: return Key::Space;
        case VK_LEFT: return Key::Left;
        case VK_UP: return Key::Up;
        case VK_RIGHT: return Key::Right;
        case VK_DOWN: return Key::Down;
        case VK_LSHIFT: return Key::LeftShift;
        case VK_RSHIFT: return Key::RightShift;
        case VK_LCONTROL: return Key::LeftControl;
        case VK_RCONTROL: return Key::RightControl;
        default: break;
    }
    if (vk >= 'A' && vk <= 'Z') return static_cast<Key>(static_cast<int>(Key::A) + (vk - 'A'));
    if (vk >= '0' && vk <= '9') return static_cast<Key>(static_cast<int>(Key::Digit0) + (vk - '0'));
    return Key::Unknown;
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* impl = reinterpret_cast<Window::Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!impl) return DefWindowProcW(hwnd, msg, wParam, lParam);
    switch (msg) {
        case WM_CLOSE:
            impl->closeRequested = true;
            if (impl->onClose) impl->onClose();
            return 0;
        case WM_SIZE:
            impl->width = LOWORD(lParam);
            impl->height = HIWORD(lParam);
            if (impl->onResize) impl->onResize(impl->width, impl->height);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP: {
            const Key key = mapVKey(wParam);
            if (key != Key::Unknown && impl->onKey) {
                Action action = msg == WM_KEYUP ? Action::Release
                                                : ((lParam & (1 << 30)) ? Action::Repeat : Action::Press);
                impl->onKey(key, action);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: {
            if (impl->onMouseButton) {
                MouseButton b = msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ? MouseButton::Left
                              : msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ? MouseButton::Right
                                                                           : MouseButton::Middle;
                const bool press = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
                impl->onMouseButton(b, press ? Action::Press : Action::Release);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (impl->onCursorPos)
                impl->onCursorPos(static_cast<double>(GET_X_LPARAM(lParam)), static_cast<double>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_MOUSEWHEEL:
            if (impl->onScroll)
                impl->onScroll(0.0, static_cast<double>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void registerClass(HINSTANCE inst) {
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    done = true;
}

} // namespace

struct Window::Impl {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC context = nullptr;
    int width = 0, height = 0;
    bool open = false;
    bool closeRequested = false;
    std::chrono::steady_clock::time_point startTime;

    ResizeCallback onResize;
    KeyCallback onKey;
    MouseButtonCallback onMouseButton;
    CursorPosCallback onCursorPos;
    ScrollCallback onScroll;
    CloseCallback onClose;
};

Window::Window() : impl_(std::make_unique<Impl>()) {}
Window::~Window() { destroy(); }
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::create(std::string_view title, int width, int height, const WindowHints& hints) {
    destroy();
    HINSTANCE inst = GetModuleHandleW(nullptr);
    registerClass(inst);

    std::wstring wtitle(title.begin(), title.end());
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!hints.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

    RECT rc{0, 0, width, height};
    AdjustWindowRect(&rc, style, FALSE);
    impl_->hwnd = CreateWindowExW(0, kClassName, wtitle.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                                  rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
    if (!impl_->hwnd) return false;
    SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));

    impl_->hdc = GetDC(impl_->hwnd);
    if (!impl_->hdc) return false;

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int pf = ChoosePixelFormat(impl_->hdc, &pfd);
    if (!pf || !SetPixelFormat(impl_->hdc, pf, &pfd)) return false;

    // Temporary legacy context so wglGetProcAddress can hand us
    // wglCreateContextAttribsARB before any modern context exists.
    HGLRC temp = wglCreateContext(impl_->hdc);
    if (!temp) return false;
    wglMakeCurrent(impl_->hdc, temp);

    using WglCreateContextAttribsFn = HGLRC (*)(HDC, HGLRC, const int*);
    auto wglCreateContextAttribsARB = reinterpret_cast<WglCreateContextAttribsFn>(
        gl::procAddress("wglCreateContextAttribsARB"));

    constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
    constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
    constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
    constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
    constexpr int WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002;

    if (wglCreateContextAttribsARB) {
        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, hints.glMajor,
            WGL_CONTEXT_MINOR_VERSION_ARB, hints.glMinor,
            WGL_CONTEXT_PROFILE_MASK_ARB, hints.coreProfile ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB
                                                            : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            0,
        };
        impl_->context = wglCreateContextAttribsARB(impl_->hdc, nullptr, attribs);
    }
    if (!impl_->context) impl_->context = wglCreateContext(impl_->hdc); // legacy fallback
    wglMakeCurrent(impl_->hdc, impl_->context);
    wglDeleteContext(temp);
    if (!impl_->context) return false;

    using WglSwapIntervalFn = BOOL (*)(int);
    if (auto wglSwapIntervalEXT = reinterpret_cast<WglSwapIntervalFn>(gl::procAddress("wglSwapIntervalEXT")))
        wglSwapIntervalEXT(hints.vsync ? 1 : 0);

    impl_->width = width;
    impl_->height = height;
    impl_->open = true;
    impl_->closeRequested = false;
    impl_->startTime = std::chrono::steady_clock::now();

    ShowWindow(impl_->hwnd, SW_SHOW);
    UpdateWindow(impl_->hwnd);

    gl::init();
    return true;
}

void Window::destroy() {
    if (!impl_) return;
    if (impl_->context) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(impl_->context);
        impl_->context = nullptr;
    }
    if (impl_->hwnd && impl_->hdc) ReleaseDC(impl_->hwnd, impl_->hdc);
    if (impl_->hwnd) DestroyWindow(impl_->hwnd);
    impl_->hwnd = nullptr;
    impl_->hdc = nullptr;
    impl_->open = false;
}

bool Window::isOpen() const { return impl_->open; }
bool Window::shouldClose() const { return impl_->closeRequested; }
void Window::setShouldClose(bool value) { impl_->closeRequested = value; }

void Window::pollEvents() {
    if (!impl_->hwnd) return;
    MSG msg{};
    while (PeekMessageW(&msg, impl_->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Window::swapBuffers() {
    if (impl_->hdc) SwapBuffers(impl_->hdc);
}

void Window::makeCurrent() {
    if (impl_->hdc && impl_->context) wglMakeCurrent(impl_->hdc, impl_->context);
}

int Window::width() const { return impl_->width; }
int Window::height() const { return impl_->height; }
int Window::framebufferWidth() const {
    if (!impl_->hwnd) return impl_->width;
    RECT rc{};
    GetClientRect(impl_->hwnd, &rc);
    return rc.right - rc.left;
}
int Window::framebufferHeight() const {
    if (!impl_->hwnd) return impl_->height;
    RECT rc{};
    GetClientRect(impl_->hwnd, &rc);
    return rc.bottom - rc.top;
}

double Window::time() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - impl_->startTime).count();
}

void Window::setTitle(std::string_view title) {
    if (!impl_->hwnd) return;
    std::wstring w(title.begin(), title.end());
    SetWindowTextW(impl_->hwnd, w.c_str());
}

void Window::setResizeCallback(ResizeCallback cb) { impl_->onResize = std::move(cb); }
void Window::setKeyCallback(KeyCallback cb) { impl_->onKey = std::move(cb); }
void Window::setMouseButtonCallback(MouseButtonCallback cb) { impl_->onMouseButton = std::move(cb); }
void Window::setCursorPosCallback(CursorPosCallback cb) { impl_->onCursorPos = std::move(cb); }
void Window::setScrollCallback(ScrollCallback cb) { impl_->onScroll = std::move(cb); }
void Window::setCloseCallback(CloseCallback cb) { impl_->onClose = std::move(cb); }

} // namespace fuhgl
