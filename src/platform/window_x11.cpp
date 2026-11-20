// X11/GLX backend for fuhgl::Window. libX11 and libGL are dlopen'ed at runtime,
// so the library builds and links without any X11/OpenGL dev packages.

#include "fuhgl/window.hpp"
#include "fuhgl/gl/loader.hpp"

#include "x11_min.hpp"

#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <string>

namespace fuhgl {
namespace {

struct X11Api {
    void* libX11 = nullptr;
    void* libGL = nullptr;
    XOpenDisplayFn XOpenDisplay = nullptr;
    XCloseDisplayFn XCloseDisplay = nullptr;
    XDefaultScreenFn XDefaultScreen = nullptr;
    XRootWindowFn XRootWindow = nullptr;
    XCreateColormapFn XCreateColormap = nullptr;
    XCreateWindowFn XCreateWindow = nullptr;
    XDestroyWindowFn XDestroyWindow = nullptr;
    XMapWindowFn XMapWindow = nullptr;
    XStoreNameFn XStoreName = nullptr;
    XSelectInputFn XSelectInput = nullptr;
    XPendingFn XPending = nullptr;
    XNextEventFn XNextEvent = nullptr;
    XInternAtomFn XInternAtom = nullptr;
    XSetWMProtocolsFn XSetWMProtocols = nullptr;
    XFreeFn XFree = nullptr;
    XLookupKeysymFn XLookupKeysym = nullptr;
    GlXChooseFBConfigFn glXChooseFBConfig = nullptr;
    GlXGetVisualFromFBConfigFn glXGetVisualFromFBConfig = nullptr;
    GlXCreateContextFn glXCreateContext = nullptr;
    GlXCreateContextAttribsFn glXCreateContextAttribsARB = nullptr;
    GlXMakeCurrentFn glXMakeCurrent = nullptr;
    GlXSwapBuffersFn glXSwapBuffers = nullptr;
    GlXDestroyContextFn glXDestroyContext = nullptr;
    GlXGetProcAddressFn glXGetProcAddress = nullptr;
    GlXSwapIntervalEXTFn glXSwapIntervalEXT = nullptr;
    GlXSwapIntervalSGIFn glXSwapIntervalSGI = nullptr;
    bool loaded = false;
};

template <typename T>
void load(void* lib, const char* name, T& out) {
    out = reinterpret_cast<T>(dlsym(lib, name));
}

bool loadX11Api(X11Api& api) {
    if (api.loaded) return api.libX11 != nullptr;
    api.libX11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_GLOBAL);
    if (!api.libX11) api.libX11 = dlopen("libX11.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!api.libX11) return false;
    api.libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!api.libGL) api.libGL = dlopen("libGL.so", RTLD_LAZY | RTLD_GLOBAL);

    load(api.libX11, "XOpenDisplay", api.XOpenDisplay);
    load(api.libX11, "XCloseDisplay", api.XCloseDisplay);
    load(api.libX11, "XDefaultScreen", api.XDefaultScreen);
    load(api.libX11, "XRootWindow", api.XRootWindow);
    load(api.libX11, "XCreateColormap", api.XCreateColormap);
    load(api.libX11, "XCreateWindow", api.XCreateWindow);
    load(api.libX11, "XDestroyWindow", api.XDestroyWindow);
    load(api.libX11, "XMapWindow", api.XMapWindow);
    load(api.libX11, "XStoreName", api.XStoreName);
    load(api.libX11, "XSelectInput", api.XSelectInput);
    load(api.libX11, "XPending", api.XPending);
    load(api.libX11, "XNextEvent", api.XNextEvent);
    load(api.libX11, "XInternAtom", api.XInternAtom);
    load(api.libX11, "XSetWMProtocols", api.XSetWMProtocols);
    load(api.libX11, "XFree", api.XFree);
    load(api.libX11, "XLookupKeysym", api.XLookupKeysym);

    if (api.libGL) {
        load(api.libGL, "glXChooseFBConfig", api.glXChooseFBConfig);
        load(api.libGL, "glXGetVisualFromFBConfig", api.glXGetVisualFromFBConfig);
        load(api.libGL, "glXCreateContext", api.glXCreateContext);
        load(api.libGL, "glXCreateContextAttribsARB", api.glXCreateContextAttribsARB);
        load(api.libGL, "glXMakeCurrent", api.glXMakeCurrent);
        load(api.libGL, "glXSwapBuffers", api.glXSwapBuffers);
        load(api.libGL, "glXDestroyContext", api.glXDestroyContext);
        load(api.libGL, "glXGetProcAddressARB", api.glXGetProcAddress);
        if (!api.glXGetProcAddress) load(api.libGL, "glXGetProcAddress", api.glXGetProcAddress);
        if (api.glXGetProcAddress) {
            api.glXSwapIntervalEXT = reinterpret_cast<GlXSwapIntervalEXTFn>(
                api.glXGetProcAddress(reinterpret_cast<const unsigned char*>("glXSwapIntervalEXT")));
            api.glXSwapIntervalSGI = reinterpret_cast<GlXSwapIntervalSGIFn>(
                api.glXGetProcAddress(reinterpret_cast<const unsigned char*>("glXSwapIntervalSGI")));
        }
    }
    api.loaded = true;
    return api.XOpenDisplay && api.XCreateWindow && api.glXChooseFBConfig && api.glXMakeCurrent && api.glXSwapBuffers;
}

Key mapKey(unsigned long keysym) {
    switch (keysym) {
        case XK_Escape_: return Key::Escape;
        case XK_space_: return Key::Space;
        case XK_Left_: return Key::Left;
        case XK_Up_: return Key::Up;
        case XK_Right_: return Key::Right;
        case XK_Down_: return Key::Down;
        case XK_Shift_L_: return Key::LeftShift;
        case XK_Shift_R_: return Key::RightShift;
        case XK_Control_L_: return Key::LeftControl;
        case XK_Control_R_: return Key::RightControl;
        default: break;
    }
    if (keysym >= 'a' && keysym <= 'z') return static_cast<Key>(static_cast<int>(Key::A) + (keysym - 'a'));
    if (keysym >= 'A' && keysym <= 'Z') return static_cast<Key>(static_cast<int>(Key::A) + (keysym - 'A'));
    if (keysym >= '0' && keysym <= '9') return static_cast<Key>(static_cast<int>(Key::Digit0) + (keysym - '0'));
    return Key::Unknown;
}

} // namespace

struct Window::Impl {
    X11Api api;
    Display* display = nullptr;
    XWindow window = 0;
    GLXContext context = nullptr;
    GLXFBConfig fbConfig = nullptr;
    Atom wmDelete = 0;
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
    if (!loadX11Api(impl_->api)) return false;
    auto& api = impl_->api;

    impl_->display = api.XOpenDisplay(nullptr);
    if (!impl_->display) return false; // headless environment
    const int screen = api.XDefaultScreen(impl_->display);
    const XWindow root = api.XRootWindow(impl_->display, screen);

    int fbAttribs[] = {
        GLX_DOUBLEBUFFER, 1,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8,
        GLX_SAMPLE_BUFFERS, hints.samples > 0 ? 1 : 0,
        GLX_SAMPLES, hints.samples > 0 ? hints.samples : 0,
        GLX_NONE,
    };
    int fbCount = 0;
    GLXFBConfig* configs = api.glXChooseFBConfig(impl_->display, screen, fbAttribs, &fbCount);
    if (!configs || fbCount <= 0) return false;
    impl_->fbConfig = configs[0];
    XVisualInfo* visual = api.glXGetVisualFromFBConfig(impl_->display, impl_->fbConfig);
    if (!visual) { api.XFree(configs); return false; }

    Colormap colormap = api.XCreateColormap(impl_->display, root, visual->visual, X11_AllocNone);
    XSetWindowAttributes swa{};
    swa.colormap = colormap;
    swa.event_mask = X11_KeyPressMask | X11_KeyReleaseMask | X11_ButtonPressMask |
                     X11_ButtonReleaseMask | X11_PointerMotionMask | X11_StructureNotifyMask;

    impl_->window = api.XCreateWindow(impl_->display, root, 0, 0, static_cast<unsigned int>(width),
                                      static_cast<unsigned int>(height), 0, visual->depth, X11_InputOutput,
                                      visual->visual, X11_CWColormap | X11_CWEventMask, &swa);
    api.XFree(visual);
    api.XFree(configs);
    if (!impl_->window) return false;

    std::string titleStr(title);
    api.XStoreName(impl_->display, impl_->window, titleStr.c_str());
    api.XSelectInput(impl_->display, impl_->window, swa.event_mask);
    impl_->wmDelete = api.XInternAtom(impl_->display, "WM_DELETE_WINDOW", 0);
    api.XSetWMProtocols(impl_->display, impl_->window, &impl_->wmDelete, 1);
    api.XMapWindow(impl_->display, impl_->window);

    // Create the GL context (core profile if requested, with legacy fallback).
    int ctxAttribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, hints.glMajor,
        GLX_CONTEXT_MINOR_VERSION_ARB, hints.glMinor,
        GLX_CONTEXT_PROFILE_MASK_ARB, hints.coreProfile ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB
                                                         : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        GLX_NONE,
    };
    if (api.glXCreateContextAttribsARB)
        impl_->context = api.glXCreateContextAttribsARB(impl_->display, impl_->fbConfig, nullptr, 1, ctxAttribs);
    if (!impl_->context) {
        XVisualInfo* vis2 = api.glXGetVisualFromFBConfig(impl_->display, impl_->fbConfig);
        if (vis2 && api.glXCreateContext) {
            impl_->context = api.glXCreateContext(impl_->display, vis2, nullptr, 1);
            api.XFree(vis2);
        }
    }
    if (!impl_->context) return false;

    api.glXMakeCurrent(impl_->display, impl_->window, impl_->context);

    // Vsync (best effort).
    if (api.glXSwapIntervalEXT) api.glXSwapIntervalEXT(impl_->display, impl_->window, hints.vsync ? 1 : 0);
    else if (api.glXSwapIntervalSGI) api.glXSwapIntervalSGI(hints.vsync ? 1 : 0);

    impl_->width = impl_->height = 0;
    XEvent ev{};
    // Drain the initial ConfigureNotify so width/height are correct.
    api.XNextEvent(impl_->display, &ev); // MapNotify
    while (api.XPending(impl_->display)) {
        api.XNextEvent(impl_->display, &ev);
        if (ev.type == X11_ConfigureNotify) {
            impl_->width = ev.xconfigure.width;
            impl_->height = ev.xconfigure.height;
        }
    }
    if (impl_->width <= 0) impl_->width = width;
    if (impl_->height <= 0) impl_->height = height;

    impl_->open = true;
    impl_->closeRequested = false;
    impl_->startTime = std::chrono::steady_clock::now();

    gl::init(); // load the GL function table now that a context is current
    return true;
}

void Window::destroy() {
    if (!impl_) return;
    if (impl_->display && impl_->context) {
        impl_->api.glXMakeCurrent(impl_->display, 0, nullptr);
        impl_->api.glXDestroyContext(impl_->display, impl_->context);
    }
    if (impl_->display && impl_->window) impl_->api.XDestroyWindow(impl_->display, impl_->window);
    if (impl_->display) impl_->api.XCloseDisplay(impl_->display);
    impl_->display = nullptr;
    impl_->window = 0;
    impl_->context = nullptr;
    impl_->open = false;
}

bool Window::isOpen() const { return impl_->open; }
bool Window::shouldClose() const { return impl_->closeRequested; }
void Window::setShouldClose(bool value) { impl_->closeRequested = value; }

void Window::pollEvents() {
    if (!impl_->display) return;
    auto& api = impl_->api;
    XEvent ev{};
    while (api.XPending(impl_->display)) {
        api.XNextEvent(impl_->display, &ev);
        switch (ev.type) {
            case X11_ConfigureNotify:
                impl_->width = ev.xconfigure.width;
                impl_->height = ev.xconfigure.height;
                if (impl_->onResize) impl_->onResize(impl_->width, impl_->height);
                break;
            case X11_ClientMessage:
                if (static_cast<Atom>(ev.xclient.data.l[0]) == impl_->wmDelete) {
                    impl_->closeRequested = true;
                    if (impl_->onClose) impl_->onClose();
                }
                break;
            case X11_KeyPress:
            case X11_KeyRelease: {
                const unsigned long sym = api.XLookupKeysym ? api.XLookupKeysym(&ev.xkey, 0) : 0;
                const Key key = mapKey(sym);
                if (key != Key::Unknown && impl_->onKey)
                    impl_->onKey(key, ev.type == X11_KeyPress ? Action::Press : Action::Release);
                break;
            }
            case X11_ButtonPress:
            case X11_ButtonRelease: {
                const bool press = ev.type == X11_ButtonPress;
                if (ev.xbutton.button == X11_Button4 || ev.xbutton.button == X11_Button5) {
                    if (impl_->onScroll)
                        impl_->onScroll(0.0, ev.xbutton.button == X11_Button4 ? 1.0 : -1.0);
                } else if (impl_->onMouseButton) {
                    MouseButton b = MouseButton::Left;
                    if (ev.xbutton.button == X11_Button2) b = MouseButton::Middle;
                    else if (ev.xbutton.button == X11_Button3) b = MouseButton::Right;
                    impl_->onMouseButton(b, press ? Action::Press : Action::Release);
                }
                break;
            }
            case X11_MotionNotify:
                if (impl_->onCursorPos) impl_->onCursorPos(ev.xmotion.x, ev.xmotion.y);
                break;
            default:
                break;
        }
    }
}

void Window::swapBuffers() {
    if (impl_->display && impl_->window) impl_->api.glXSwapBuffers(impl_->display, impl_->window);
}

void Window::makeCurrent() {
    if (impl_->display && impl_->context) impl_->api.glXMakeCurrent(impl_->display, impl_->window, impl_->context);
}

int Window::width() const { return impl_->width; }
int Window::height() const { return impl_->height; }
int Window::framebufferWidth() const { return impl_->width; }  // X11: 1:1 unless WM scales
int Window::framebufferHeight() const { return impl_->height; }

double Window::time() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - impl_->startTime).count();
}

void Window::setTitle(std::string_view title) {
    if (impl_->display && impl_->window) {
        std::string s(title);
        impl_->api.XStoreName(impl_->display, impl_->window, s.c_str());
    }
}

void Window::setResizeCallback(ResizeCallback cb) { impl_->onResize = std::move(cb); }
void Window::setKeyCallback(KeyCallback cb) { impl_->onKey = std::move(cb); }
void Window::setMouseButtonCallback(MouseButtonCallback cb) { impl_->onMouseButton = std::move(cb); }
void Window::setCursorPosCallback(CursorPosCallback cb) { impl_->onCursorPos = std::move(cb); }
void Window::setScrollCallback(ScrollCallback cb) { impl_->onScroll = std::move(cb); }
void Window::setCloseCallback(CloseCallback cb) { impl_->onClose = std::move(cb); }

} // namespace fuhgl
