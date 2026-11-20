// Linux EGL backend for fuhgl::Window. libEGL and libwayland-client are
// dlopen'ed at runtime, so no EGL/Wayland dev packages are needed at build time.
//
// Modes (chosen at runtime):
//   - WAYLAND_DISPLAY set (or FUHGL_BACKEND=wayland): Wayland xdg-shell window
//   - otherwise (or FUHGL_BACKEND=surfaceless): EGL surfaceless context —
//     headless, rendering happens into FBOs only (used by the CI smoke test).
//
// Build with -DFUHGL_LINUX_BACKEND=EGL (default on Linux is X11).
//
// NOTE: the Wayland wire protocol is hand-marshalled below (opcode tables follow
// wayland.xml / stable xdg-shell.xml); it is compile-verified here but should get
// a real-compositor smoke test before being relied on.

#include "fuhgl/window.hpp"
#include "fuhgl/gl/loader.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>

#include <dlfcn.h>
#include <poll.h>
#include <unistd.h>

namespace fuhgl {
namespace {

// ---------------------------------------------------------------------------
// minimal wayland-client declarations (ABI-stable)
// ---------------------------------------------------------------------------
struct wl_proxy;
struct wl_display;
struct wl_interface;
struct wl_message {
    const char* name;
    const char* signature;
    const wl_interface* const* types;
};
struct wl_interface {
    const char* name;
    int version;
    int method_count;
    const wl_message* methods;
    int event_count;
    const wl_message* events;
};
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wl_array {
    size_t size;
    size_t alloc;
    void* data;
};
using wl_fixed_t = int32_t;
inline double wl_fixed_to_double(wl_fixed_t f) { return static_cast<double>(f) / 256.0; }

using wl_display_connect_fn = wl_display* (*)(const char*);
using wl_display_disconnect_fn = void (*)(wl_display*);
using wl_display_get_registry_fn = wl_registry* (*)(wl_display*);
using wl_display_roundtrip_fn = int (*)(wl_display*);
using wl_display_dispatch_pending_fn = int (*)(wl_display*);
using wl_display_get_fd_fn = int (*)(wl_display*);
using wl_display_prepare_read_fn = int (*)(wl_display*);
using wl_display_read_events_fn = int (*)(wl_display*);
using wl_display_cancel_read_fn = void (*)(wl_display*);
using wl_proxy_marshal_flags_fn = wl_proxy* (*)(wl_proxy*, uint32_t opcode, const wl_interface*, uint32_t version, uint32_t flags, ...);
using wl_proxy_destroy_fn = void (*)(wl_proxy*);
using wl_proxy_add_listener_fn = int (*)(wl_proxy*, void (**implementation)(void), void* data);
using wl_proxy_get_version_fn = uint32_t (*)(wl_proxy*);

// interface objects we must define ourselves (not exported by libwayland-client)
const wl_message kWlRegistryMethods[] = {{"bind", "usun", nullptr}};
const wl_message kWlRegistryEvents[] = {{"global", "usu", nullptr}, {"global_remove", "u", nullptr}};
const wl_interface kWlRegistryIface = {"wl_registry", 1, 1, kWlRegistryMethods, 2, kWlRegistryEvents};

const wl_message kWlCompositorMethods[] = {{"create_surface", "n", nullptr}, {"create_region", "n", nullptr}};
const wl_interface kWlCompositorIface = {"wl_compositor", 4, 2, kWlCompositorMethods, 0, nullptr};

const wl_message kWlSurfaceMethods[] = {
    {"destroy", "", nullptr}, {"attach", "o?ii", nullptr}, {"damage", "iiii", nullptr},
    {"frame", "n", nullptr}, {"set_opaque_region", "o?", nullptr}, {"set_input_region", "o?", nullptr},
    {"commit", "", nullptr}, {"set_buffer_transform", "i", nullptr}, {"set_buffer_scale", "i", nullptr},
    {"damage_buffer", "iiii", nullptr},
};
const wl_interface kWlSurfaceIface = {"wl_surface", 4, 10, kWlSurfaceMethods, 0, nullptr};

const wl_message kWlSeatMethods[] = {{"get_pointer", "n", nullptr}, {"get_keyboard", "n", nullptr}, {"release", "", nullptr}};
const wl_message kWlSeatEvents[] = {{"capabilities", "u", nullptr}, {"name", "s", nullptr}};
const wl_interface kWlSeatIface = {"wl_seat", 5, 3, kWlSeatMethods, 2, kWlSeatEvents};

const wl_message kWlPointerMethods[] = {{"set_cursor", "uo?ii", nullptr}, {"release", "", nullptr}};
const wl_message kWlPointerEvents[] = {
    {"enter", "uoff", nullptr}, {"leave", "uo", nullptr}, {"motion", "uff", nullptr},
    {"button", "uuuu", nullptr}, {"axis", "uuf", nullptr}, {"frame", "", nullptr},
    {"axis_source", "u", nullptr}, {"axis_stop", "uuf", nullptr}, {"axis_discrete", "uf", nullptr},
};
const wl_interface kWlPointerIface = {"wl_pointer", 5, 2, kWlPointerMethods, 9, kWlPointerEvents};

const wl_message kWlKeyboardMethods[] = {{"release", "", nullptr}};
const wl_message kWlKeyboardEvents[] = {
    {"keymap", "uhu", nullptr}, {"enter", "uoa", nullptr}, {"leave", "uo", nullptr},
    {"key", "uuuu", nullptr}, {"modifiers", "uuuuu", nullptr}, {"repeat_info", "ii", nullptr},
};
const wl_interface kWlKeyboardIface = {"wl_keyboard", 5, 1, kWlKeyboardMethods, 6, kWlKeyboardEvents};

const wl_message kXdgWmBaseMethods[] = {{"destroy", "", nullptr}, {"get_xdg_surface", "no", nullptr}, {"pong", "u", nullptr}};
const wl_message kXdgWmBaseEvents[] = {{"ping", "u", nullptr}};
const wl_interface kXdgWmBaseIface = {"xdg_wm_base", 1, 3, kXdgWmBaseMethods, 1, kXdgWmBaseEvents};

const wl_message kXdgSurfaceMethods[] = {
    {"destroy", "", nullptr}, {"get_toplevel", "n", nullptr}, {"set_parent", "o?", nullptr},
    {"set_title", "s", nullptr}, {"set_app_id", "s", nullptr}, {"show_window_menu", "ouii", nullptr},
    {"set_maximized", "", nullptr}, {"unset_maximized", "", nullptr}, {"set_fullscreen", "o?", nullptr},
    {"unset_fullscreen", "", nullptr}, {"set_minimized", "", nullptr}, {"ack_configure", "u", nullptr},
};
const wl_message kXdgSurfaceEvents[] = {{"configure", "u", nullptr}};
const wl_interface kXdgSurfaceIface = {"xdg_surface", 1, 12, kXdgSurfaceMethods, 1, kXdgSurfaceEvents};

const wl_message kXdgToplevelMethods[] = {
    {"destroy", "", nullptr}, {"set_parent", "o?", nullptr}, {"set_title", "s", nullptr},
    {"set_app_id", "s", nullptr}, {"show_window_menu", "ouii", nullptr}, {"set_maximized", "", nullptr},
    {"unset_maximized", "", nullptr}, {"set_fullscreen", "o?", nullptr}, {"unset_fullscreen", "", nullptr},
    {"set_minimized", "", nullptr},
};
const wl_message kXdgToplevelEvents[] = {{"configure", "iia", nullptr}, {"close", "", nullptr}, {"configure_bounds", "ii", nullptr}};
const wl_interface kXdgToplevelIface = {"xdg_toplevel", 1, 10, kXdgToplevelMethods, 3, kXdgToplevelEvents};

// opcodes (per wayland.xml / xdg-shell.xml request order)
enum : uint32_t {
    WL_REGISTRY_BIND = 0,
    WL_COMPOSITOR_CREATE_SURFACE = 0,
    WL_SURFACE_COMMIT = 6,
    WL_SEAT_GET_POINTER = 0,
    WL_SEAT_GET_KEYBOARD = 1,
    XDG_WM_BASE_GET_XDG_SURFACE = 1,
    XDG_WM_BASE_PONG = 2,
    XDG_SURFACE_GET_TOPLEVEL = 1,
    XDG_SURFACE_ACK_CONFIGURE = 11,
    XDG_TOPLEVEL_SET_TITLE = 2,
};

// ---------------------------------------------------------------------------
// minimal EGL declarations
// ---------------------------------------------------------------------------
using EGLDisplay = void*;
using EGLContext = void*;
using EGLSurface = void*;
using EGLConfig = void*;
using EGLint = int;
using EGLenum = unsigned int;
using EGLBoolean = unsigned int;
constexpr EGLenum EGL_OPENGL_API = 0x30A2;
constexpr EGLenum EGL_SURFACE_TYPE = 0x3033;
constexpr EGLenum EGL_PBUFFER_BIT = 0x0001;
constexpr EGLenum EGL_WINDOW_BIT = 0x0004;
constexpr EGLenum EGL_RENDERABLE_TYPE = 0x3040;
constexpr EGLenum EGL_OPENGL_BIT = 0x0008;
constexpr EGLenum EGL_RED_SIZE = 0x3021;
constexpr EGLenum EGL_GREEN_SIZE = 0x3022;
constexpr EGLenum EGL_BLUE_SIZE = 0x3023;
constexpr EGLenum EGL_ALPHA_SIZE = 0x3024;
constexpr EGLenum EGL_DEPTH_SIZE = 0x3025;
constexpr EGLenum EGL_STENCIL_SIZE = 0x3026;
constexpr EGLenum EGL_SAMPLES = 0x3031;
constexpr EGLenum EGL_SAMPLE_BUFFERS = 0x3032;
constexpr EGLenum EGL_NONE = 0x3038;
constexpr EGLenum EGL_WIDTH = 0x3057;
constexpr EGLenum EGL_HEIGHT = 0x3056;
constexpr EGLenum EGL_CONTEXT_MAJOR_VERSION = 0x3098;
constexpr EGLenum EGL_CONTEXT_MINOR_VERSION = 0x30FB;
constexpr EGLenum EGL_CONTEXT_OPENGL_PROFILE_MASK = 0x30FD;
constexpr EGLenum EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT = 0x00000001;
constexpr EGLenum EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT = 0x00000002;
constexpr EGLenum EGL_PLATFORM_WAYLAND_EXT = 0x31D8;
constexpr EGLenum EGL_PLATFORM_SURFACELESS_MESA = 0x31DD;
constexpr EGLDisplay EGL_NO_DISPLAY = nullptr;
constexpr EGLSurface EGL_NO_SURFACE = nullptr;
constexpr EGLContext EGL_NO_CONTEXT = nullptr;

using eglGetPlatformDisplayEXT_fn = EGLDisplay (*)(EGLenum, void*, const EGLint*);
using eglGetDisplay_fn = EGLDisplay (*)(void*);
using eglInitialize_fn = EGLBoolean (*)(EGLDisplay, EGLint*, EGLint*);
using eglBindAPI_fn = EGLBoolean (*)(EGLenum);
using eglChooseConfig_fn = EGLBoolean (*)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
using eglCreateContext_fn = EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
using eglCreatePlatformWindowSurfaceEXT_fn = EGLSurface (*)(EGLDisplay, EGLConfig, void*, const EGLint*);
using eglMakeCurrent_fn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
using eglSwapBuffers_fn = EGLBoolean (*)(EGLDisplay, EGLSurface);
using eglSwapInterval_fn = EGLBoolean (*)(EGLDisplay, EGLint);
using eglDestroySurface_fn = EGLBoolean (*)(EGLDisplay, EGLSurface);
using eglDestroyContext_fn = EGLBoolean (*)(EGLDisplay, EGLContext);
using eglTerminate_fn = EGLBoolean (*)(EGLDisplay);
using eglGetError_fn = EGLint (*)(void);
using eglGetProcAddress_fn = void* (*)(const char*);

struct EglApi {
    void* lib = nullptr;
    eglGetPlatformDisplayEXT_fn eglGetPlatformDisplayEXT = nullptr;
    eglGetDisplay_fn eglGetDisplay = nullptr;
    eglInitialize_fn eglInitialize = nullptr;
    eglBindAPI_fn eglBindAPI = nullptr;
    eglChooseConfig_fn eglChooseConfig = nullptr;
    eglCreateContext_fn eglCreateContext = nullptr;
    eglCreatePlatformWindowSurfaceEXT_fn eglCreatePlatformWindowSurfaceEXT = nullptr;
    eglMakeCurrent_fn eglMakeCurrent = nullptr;
    eglSwapBuffers_fn eglSwapBuffers = nullptr;
    eglSwapInterval_fn eglSwapInterval = nullptr;
    eglDestroySurface_fn eglDestroySurface = nullptr;
    eglDestroyContext_fn eglDestroyContext = nullptr;
    eglTerminate_fn eglTerminate = nullptr;
    eglGetError_fn eglGetError = nullptr;
    eglGetProcAddress_fn eglGetProcAddress = nullptr;
    bool loaded = false;
};

struct WlApi {
    void* lib = nullptr;
    wl_display_connect_fn connect = nullptr;
    wl_display_disconnect_fn disconnect = nullptr;
    wl_display_get_registry_fn get_registry = nullptr;
    wl_display_roundtrip_fn roundtrip = nullptr;
    wl_display_dispatch_pending_fn dispatch_pending = nullptr;
    wl_display_get_fd_fn get_fd = nullptr;
    wl_display_prepare_read_fn prepare_read = nullptr;
    wl_display_read_events_fn read_events = nullptr;
    wl_display_cancel_read_fn cancel_read = nullptr;
    wl_proxy_marshal_flags_fn marshal_flags = nullptr;
    wl_proxy_destroy_fn proxy_destroy = nullptr;
    wl_proxy_add_listener_fn proxy_add_listener = nullptr;
    wl_proxy_get_version_fn proxy_get_version = nullptr;
    bool loaded = false;
};

template <typename T>
void loadSym(void* lib, const char* name, T& out) { out = reinterpret_cast<T>(dlsym(lib, name)); }

bool loadEgl(EglApi& e) {
    if (e.loaded) return e.lib != nullptr;
    e.lib = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!e.lib) e.lib = dlopen("libEGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!e.lib) return false;
    // eglGetPlatformDisplayEXT / eglCreatePlatformWindowSurfaceEXT are extension
    // entry points: they are NOT exported symbols, only reachable via eglGetProcAddress.
    loadSym(e.lib, "eglGetProcAddress", e.eglGetProcAddress);
    if (e.eglGetProcAddress) {
        e.eglGetPlatformDisplayEXT = reinterpret_cast<eglGetPlatformDisplayEXT_fn>(e.eglGetProcAddress("eglGetPlatformDisplayEXT"));
        e.eglCreatePlatformWindowSurfaceEXT = reinterpret_cast<eglCreatePlatformWindowSurfaceEXT_fn>(e.eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
    }
    loadSym(e.lib, "eglGetDisplay", e.eglGetDisplay);
    loadSym(e.lib, "eglInitialize", e.eglInitialize);
    loadSym(e.lib, "eglBindAPI", e.eglBindAPI);
    loadSym(e.lib, "eglChooseConfig", e.eglChooseConfig);
    loadSym(e.lib, "eglCreateContext", e.eglCreateContext);
    loadSym(e.lib, "eglMakeCurrent", e.eglMakeCurrent);
    loadSym(e.lib, "eglSwapBuffers", e.eglSwapBuffers);
    loadSym(e.lib, "eglSwapInterval", e.eglSwapInterval);
    loadSym(e.lib, "eglDestroySurface", e.eglDestroySurface);
    loadSym(e.lib, "eglDestroyContext", e.eglDestroyContext);
    loadSym(e.lib, "eglTerminate", e.eglTerminate);
    loadSym(e.lib, "eglGetError", e.eglGetError);
    e.loaded = true;
    return e.eglInitialize && e.eglChooseConfig && e.eglCreateContext && e.eglMakeCurrent;
}

bool loadWl(WlApi& w) {
    if (w.loaded) return w.lib != nullptr;
    w.lib = dlopen("libwayland-client.so.0", RTLD_LAZY | RTLD_GLOBAL);
    if (!w.lib) w.lib = dlopen("libwayland-client.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!w.lib) return false;
    loadSym(w.lib, "wl_display_connect", w.connect);
    loadSym(w.lib, "wl_display_disconnect", w.disconnect);
    loadSym(w.lib, "wl_display_get_registry", w.get_registry);
    loadSym(w.lib, "wl_display_roundtrip", w.roundtrip);
    loadSym(w.lib, "wl_display_dispatch_pending", w.dispatch_pending);
    loadSym(w.lib, "wl_display_get_fd", w.get_fd);
    loadSym(w.lib, "wl_display_prepare_read", w.prepare_read);
    loadSym(w.lib, "wl_display_read_events", w.read_events);
    loadSym(w.lib, "wl_display_cancel_read", w.cancel_read);
    loadSym(w.lib, "wl_proxy_marshal_flags", w.marshal_flags);
    loadSym(w.lib, "wl_proxy_destroy", w.proxy_destroy);
    loadSym(w.lib, "wl_proxy_add_listener", w.proxy_add_listener);
    loadSym(w.lib, "wl_proxy_get_version", w.proxy_get_version);
    w.loaded = true;
    return w.connect && w.marshal_flags && w.proxy_add_listener;
}

// evdev keycode -> fuhgl::Key (wl_keyboard sends evdev codes)
Key mapEvdev(uint32_t evdev) {
    switch (evdev) {
        case 1: return Key::Escape;
        case 57: return Key::Space;
        case 105: return Key::Left;
        case 103: return Key::Up;
        case 106: return Key::Right;
        case 108: return Key::Down;
        case 42: return Key::LeftShift;
        case 54: return Key::RightShift;
        case 29: return Key::LeftControl;
        case 97: return Key::RightControl;
        default: break;
    }
    // letters: evdev KEY_Q..KEY_P = 16..25, KEY_A..KEY_L = 30..38, KEY_Z..KEY_M = 44..50
    if (evdev >= 16 && evdev <= 25) return static_cast<Key>(static_cast<int>(Key::Q) + (evdev - 16));
    if (evdev >= 30 && evdev <= 38) return static_cast<Key>(static_cast<int>(Key::A) + (evdev - 30));
    if (evdev >= 44 && evdev <= 50) return static_cast<Key>(static_cast<int>(Key::Z) + (evdev - 44));
    // digits: KEY_1..KEY_9 = 2..10, KEY_0 = 11
    if (evdev >= 2 && evdev <= 10) return static_cast<Key>(static_cast<int>(Key::Digit1) + (evdev - 2));
    if (evdev == 11) return Key::Digit0;
    return Key::Unknown;
}

} // namespace

struct Window::Impl {
    EglApi egl;
    WlApi wl;
    bool useWayland = false;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    wl_display* wlDisplay = nullptr;
    wl_compositor* compositor = nullptr;
    xdg_wm_base* wmBase = nullptr;
    wl_seat* seat = nullptr;
    wl_surface* wlSurface = nullptr;
    xdg_surface* xdgSurface = nullptr;
    xdg_toplevel* xdgToplevel = nullptr;
    wl_pointer* pointer = nullptr;
    wl_keyboard* keyboard = nullptr;
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

// wayland listeners (C callbacks) ------------------------------------------------
namespace {

void registryGlobal(void* data, wl_registry* reg, uint32_t name, const char* iface, uint32_t version) {
    auto* impl = static_cast<Window::Impl*>(data);
    auto& w = impl->wl;
    if (std::strcmp(iface, "wl_compositor") == 0 && !impl->compositor) {
        impl->compositor = reinterpret_cast<wl_compositor*>(
            w.marshal_flags(reinterpret_cast<wl_proxy*>(reg), WL_REGISTRY_BIND, &kWlCompositorIface,
                            version < 4 ? version : 4, 0, name, "wl_compositor", version, nullptr));
    } else if (std::strcmp(iface, "xdg_wm_base") == 0 && !impl->wmBase) {
        impl->wmBase = reinterpret_cast<xdg_wm_base*>(
            w.marshal_flags(reinterpret_cast<wl_proxy*>(reg), WL_REGISTRY_BIND, &kXdgWmBaseIface,
                            version < 1 ? version : 1, 0, name, "xdg_wm_base", version, nullptr));
    } else if (std::strcmp(iface, "wl_seat") == 0 && !impl->seat) {
        impl->seat = reinterpret_cast<wl_seat*>(
            w.marshal_flags(reinterpret_cast<wl_proxy*>(reg), WL_REGISTRY_BIND, &kWlSeatIface,
                            version < 5 ? version : 5, 0, name, "wl_seat", version, nullptr));
    }
}
void registryGlobalRemove(void*, wl_registry*, uint32_t) {}

void wmBasePing(void* data, xdg_wm_base* base, uint32_t serial) {
    auto* impl = static_cast<Window::Impl*>(data);
    impl->wl.marshal_flags(reinterpret_cast<wl_proxy*>(base), XDG_WM_BASE_PONG, &kXdgWmBaseIface,
                           impl->wl.proxy_get_version(reinterpret_cast<wl_proxy*>(base)), 0, serial);
}

void xdgSurfaceConfigure(void* data, xdg_surface* surf, uint32_t serial) {
    auto* impl = static_cast<Window::Impl*>(data);
    impl->wl.marshal_flags(reinterpret_cast<wl_proxy*>(surf), XDG_SURFACE_ACK_CONFIGURE, &kXdgSurfaceIface,
                           impl->wl.proxy_get_version(reinterpret_cast<wl_proxy*>(surf)), 0, serial);
    impl->wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl->wlSurface), WL_SURFACE_COMMIT, &kWlSurfaceIface,
                           impl->wl.proxy_get_version(reinterpret_cast<wl_proxy*>(impl->wlSurface)), 0);
}

void toplevelConfigure(void* data, xdg_toplevel*, int32_t w, int32_t h, wl_array*) {
    auto* impl = static_cast<Window::Impl*>(data);
    if (w > 0 && h > 0 && (w != impl->width || h != impl->height)) {
        impl->width = w;
        impl->height = h;
        if (impl->onResize) impl->onResize(w, h);
    }
}
void toplevelClose(void* data, xdg_toplevel*) {
    auto* impl = static_cast<Window::Impl*>(data);
    impl->closeRequested = true;
    if (impl->onClose) impl->onClose();
}

void pointerMotion(void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto* impl = static_cast<Window::Impl*>(data);
    if (impl->onCursorPos) impl->onCursorPos(wl_fixed_to_double(x), wl_fixed_to_double(y));
}
void pointerButton(void* data, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state) {
    auto* impl = static_cast<Window::Impl*>(data);
    if (!impl->onMouseButton) return;
    MouseButton b = MouseButton::Left;
    if (button == 273) b = MouseButton::Right;       // BTN_RIGHT
    else if (button == 274) b = MouseButton::Middle; // BTN_MIDDLE
    impl->onMouseButton(b, state == 1 ? Action::Press : Action::Release);
}
void pointerAxis(void* data, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value) {
    auto* impl = static_cast<Window::Impl*>(data);
    if (!impl->onScroll) return;
    if (axis == 0) impl->onScroll(0.0, -wl_fixed_to_double(value) / 10.0); // vertical: wheel up = positive
}
void pointerNoop(void*, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {}
void pointerLeave(void*, wl_pointer*, uint32_t, wl_surface*) {}

void keyboardKey(void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state) {
    auto* impl = static_cast<Window::Impl*>(data);
    const Key k = mapEvdev(key);
    if (k != Key::Unknown && impl->onKey) impl->onKey(k, state == 1 ? Action::Press : Action::Release);
}
void keyboardNoop(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
void keyboardKeymap(void*, wl_keyboard*, uint32_t, int32_t, uint32_t) {}
void keyboardModifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}

} // namespace

Window::Window() : impl_(std::make_unique<Impl>()) {}
Window::~Window() { destroy(); }
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::create(std::string_view title, int width, int height, const WindowHints& hints) {
    destroy();
    if (!loadEgl(impl_->egl)) return false;
    auto& egl = impl_->egl;

    const char* force = std::getenv("FUHGL_BACKEND");
    impl_->useWayland = (force && std::strcmp(force, "wayland") == 0) ||
                        ((!force || std::strcmp(force, "surfaceless") != 0) && std::getenv("WAYLAND_DISPLAY") != nullptr &&
                         loadWl(impl_->wl));

    if (impl_->useWayland) {
        auto& wl = impl_->wl;
        impl_->wlDisplay = wl.connect(nullptr);
        if (!impl_->wlDisplay) return false;
        wl_registry* reg = wl.get_registry(impl_->wlDisplay);
        static void (*registryImpl[2])(void) = {reinterpret_cast<void (*)(void)>(&registryGlobal),
                                                 reinterpret_cast<void (*)(void)>(&registryGlobalRemove)};
        wl.proxy_add_listener(reinterpret_cast<wl_proxy*>(reg), registryImpl, impl_.get());
        wl.roundtrip(impl_->wlDisplay);
        if (!impl_->compositor || !impl_->wmBase) return false;

        impl_->wlSurface = reinterpret_cast<wl_surface*>(
            wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->compositor), WL_COMPOSITOR_CREATE_SURFACE,
                             &kWlSurfaceIface, wl.proxy_get_version(reinterpret_cast<wl_proxy*>(impl_->compositor)), 0));
        impl_->xdgSurface = reinterpret_cast<xdg_surface*>(
            wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->wmBase), XDG_WM_BASE_GET_XDG_SURFACE, &kXdgWmBaseIface,
                             1, 0, impl_->wlSurface));
        static void (*wmBaseImpl[1])(void) = {reinterpret_cast<void (*)(void)>(&wmBasePing)};
        wl.proxy_add_listener(reinterpret_cast<wl_proxy*>(impl_->wmBase), wmBaseImpl, impl_.get());
        static void (*xdgSurfImpl[1])(void) = {reinterpret_cast<void (*)(void)>(&xdgSurfaceConfigure)};
        wl.proxy_add_listener(reinterpret_cast<wl_proxy*>(impl_->xdgSurface), xdgSurfImpl, impl_.get());
        impl_->xdgToplevel = reinterpret_cast<xdg_toplevel*>(
            wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->xdgSurface), XDG_SURFACE_GET_TOPLEVEL,
                             &kXdgSurfaceIface, 1, 0));
        static void (*toplevelImpl[3])(void) = {
            reinterpret_cast<void (*)(void)>(&toplevelConfigure),
            reinterpret_cast<void (*)(void)>(&toplevelClose),
            reinterpret_cast<void (*)(void)>(&toplevelClose)};
        wl.proxy_add_listener(reinterpret_cast<wl_proxy*>(impl_->xdgToplevel), toplevelImpl, impl_.get());
        std::string t(title);
        wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->xdgToplevel), XDG_TOPLEVEL_SET_TITLE, &kXdgToplevelIface,
                         1, 0, t.c_str());
        wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->wlSurface), WL_SURFACE_COMMIT, &kWlSurfaceIface, 1, 0);
        wl.roundtrip(impl_->wlDisplay);

        if (impl_->seat) {
            impl_->pointer = reinterpret_cast<wl_pointer*>(
                wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->seat), WL_SEAT_GET_POINTER, &kWlSeatIface, 1, 0));
            static void (*pointerImpl[5])(void) = {
                reinterpret_cast<void (*)(void)>(&pointerNoop), reinterpret_cast<void (*)(void)>(&pointerLeave),
                reinterpret_cast<void (*)(void)>(&pointerMotion), reinterpret_cast<void (*)(void)>(&pointerButton),
                reinterpret_cast<void (*)(void)>(&pointerAxis)};
            wl.proxy_add_listener(reinterpret_cast<wl_proxy*>(impl_->pointer), pointerImpl, impl_.get());
            impl_->keyboard = reinterpret_cast<wl_keyboard*>(
                wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->seat), WL_SEAT_GET_KEYBOARD, &kWlSeatIface, 1, 0));
            static void (*keyboardImpl[6])(void) = {
                reinterpret_cast<void (*)(void)>(&keyboardKeymap), reinterpret_cast<void (*)(void)>(&keyboardNoop),
                reinterpret_cast<void (*)(void)>(&keyboardNoop), reinterpret_cast<void (*)(void)>(&keyboardKey),
                reinterpret_cast<void (*)(void)>(&keyboardModifiers), reinterpret_cast<void (*)(void)>(&keyboardNoop)};
            wl.proxy_add_listener(reinterpret_cast<wl_proxy*>(impl_->keyboard), keyboardImpl, impl_.get());
        }

        if (egl.eglGetPlatformDisplayEXT)
            impl_->display = egl.eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, impl_->wlDisplay, nullptr);
        if (impl_->display == EGL_NO_DISPLAY) impl_->display = egl.eglGetDisplay(reinterpret_cast<void*>(impl_->wlDisplay));
    } else {
        // surfaceless / headless
        if (egl.eglGetPlatformDisplayEXT)
            impl_->display = egl.eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, nullptr, nullptr);
        if (impl_->display == EGL_NO_DISPLAY) impl_->display = egl.eglGetDisplay(nullptr);
    }
    if (impl_->display == EGL_NO_DISPLAY) return false;
    if (!egl.eglInitialize(impl_->display, nullptr, nullptr)) return false;
    if (!egl.eglBindAPI(EGL_OPENGL_API)) return false;

    const EGLint cfgAttribs[] = {
        EGL_SURFACE_TYPE, static_cast<EGLint>(impl_->useWayland ? EGL_WINDOW_BIT : EGL_PBUFFER_BIT),
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, hints.samples > 0 ? 1 : 0,
        EGL_SAMPLES, hints.samples > 0 ? hints.samples : 0,
        EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (!egl.eglChooseConfig(impl_->display, cfgAttribs, &config, 1, &numConfigs) || numConfigs <= 0) return false;

    if (impl_->useWayland) {
        if (!egl.eglCreatePlatformWindowSurfaceEXT) return false;
        impl_->surface = egl.eglCreatePlatformWindowSurfaceEXT(impl_->display, config, impl_->wlSurface, nullptr);
        if (impl_->surface == EGL_NO_SURFACE) return false;
    }

    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, hints.glMajor,
        EGL_CONTEXT_MINOR_VERSION, hints.glMinor,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        static_cast<EGLint>(hints.coreProfile ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
                                               : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT),
        EGL_NONE,
    };
    impl_->context = egl.eglCreateContext(impl_->display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (impl_->context == EGL_NO_CONTEXT) return false;
    // surfaceless contexts are made current without any surface (EGL_KHR_surfaceless_context)
    if (!egl.eglMakeCurrent(impl_->display, impl_->surface, impl_->surface, impl_->context)) return false;
    if (egl.eglSwapInterval) egl.eglSwapInterval(impl_->display, hints.vsync ? 1 : 0);

    impl_->width = width;
    impl_->height = height;
    impl_->open = true;
    impl_->closeRequested = false;
    impl_->startTime = std::chrono::steady_clock::now();

    gl::init();
    return true;
}

void Window::destroy() {
    if (!impl_) return;
    auto& egl = impl_->egl;
    auto& wl = impl_->wl;
    if (impl_->display != EGL_NO_DISPLAY && impl_->context != EGL_NO_CONTEXT)
        egl.eglMakeCurrent(impl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (impl_->context != EGL_NO_CONTEXT && egl.eglDestroyContext) egl.eglDestroyContext(impl_->display, impl_->context);
    if (impl_->surface != EGL_NO_SURFACE && egl.eglDestroySurface) egl.eglDestroySurface(impl_->display, impl_->surface);
    if (impl_->display != EGL_NO_DISPLAY && egl.eglTerminate) egl.eglTerminate(impl_->display);
    if (impl_->xdgToplevel) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->xdgToplevel)); impl_->xdgToplevel = nullptr; }
    if (impl_->xdgSurface) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->xdgSurface)); impl_->xdgSurface = nullptr; }
    if (impl_->wlSurface) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->wlSurface)); impl_->wlSurface = nullptr; }
    if (impl_->pointer) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->pointer)); impl_->pointer = nullptr; }
    if (impl_->keyboard) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->keyboard)); impl_->keyboard = nullptr; }
    if (impl_->wmBase) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->wmBase)); impl_->wmBase = nullptr; }
    if (impl_->compositor) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->compositor)); impl_->compositor = nullptr; }
    if (impl_->seat) { wl.proxy_destroy(reinterpret_cast<wl_proxy*>(impl_->seat)); impl_->seat = nullptr; }
    if (impl_->wlDisplay) { wl.disconnect(impl_->wlDisplay); impl_->wlDisplay = nullptr; }
    impl_->display = EGL_NO_DISPLAY;
    impl_->surface = EGL_NO_SURFACE;
    impl_->context = EGL_NO_CONTEXT;
    impl_->open = false;
}

bool Window::isOpen() const { return impl_->open; }
bool Window::shouldClose() const { return impl_->closeRequested; }
void Window::setShouldClose(bool value) { impl_->closeRequested = value; }

void Window::pollEvents() {
    if (!impl_->useWayland || !impl_->wlDisplay) return;
    auto& wl = impl_->wl;
    wl.dispatch_pending(impl_->wlDisplay);
    while (wl.prepare_read(impl_->wlDisplay) != 0) wl.dispatch_pending(impl_->wlDisplay);
    pollfd pfd{ wl.get_fd(impl_->wlDisplay), POLLIN, 0 };
    if (::poll(&pfd, 1, 0) > 0) {
        if (wl.read_events(impl_->wlDisplay) >= 0)
            wl.dispatch_pending(impl_->wlDisplay);
        else
            wl.cancel_read(impl_->wlDisplay);
    } else {
        wl.cancel_read(impl_->wlDisplay);
    }
}

void Window::swapBuffers() {
    if (impl_->display != EGL_NO_DISPLAY && impl_->surface != EGL_NO_SURFACE)
        impl_->egl.eglSwapBuffers(impl_->display, impl_->surface);
}

void Window::makeCurrent() {
    if (impl_->display != EGL_NO_DISPLAY && impl_->context != EGL_NO_CONTEXT)
        impl_->egl.eglMakeCurrent(impl_->display, impl_->surface, impl_->surface, impl_->context);
}

int Window::width() const { return impl_->width; }
int Window::height() const { return impl_->height; }
int Window::framebufferWidth() const { return impl_->width; }
int Window::framebufferHeight() const { return impl_->height; }

double Window::time() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - impl_->startTime).count();
}

void Window::setTitle(std::string_view title) {
    if (impl_->useWayland && impl_->xdgToplevel) {
        std::string t(title);
        impl_->wl.marshal_flags(reinterpret_cast<wl_proxy*>(impl_->xdgToplevel), XDG_TOPLEVEL_SET_TITLE,
                                &kXdgToplevelIface, 1, 0, t.c_str());
    }
}

void Window::setResizeCallback(ResizeCallback cb) { impl_->onResize = std::move(cb); }
void Window::setKeyCallback(KeyCallback cb) { impl_->onKey = std::move(cb); }
void Window::setMouseButtonCallback(MouseButtonCallback cb) { impl_->onMouseButton = std::move(cb); }
void Window::setCursorPosCallback(CursorPosCallback cb) { impl_->onCursorPos = std::move(cb); }
void Window::setScrollCallback(ScrollCallback cb) { impl_->onScroll = std::move(cb); }
void Window::setCloseCallback(CloseCallback cb) { impl_->onClose = std::move(cb); }

} // namespace fuhgl
