// macOS Cocoa backend for fuhgl::Window (Objective-C++).
// Uses only system frameworks (Cocoa, OpenGL); GL functions are loaded at
// runtime via fuhgl::gl::init().

#include "fuhgl/window.hpp"
#include "fuhgl/gl/loader.hpp"

#import <Cocoa/Cocoa.h>

#include <chrono>
#include <string>

namespace fuhgl {
namespace {

Key mapKeyCode(unsigned short keyCode) {
    switch (keyCode) {
        case 0x35: return Key::Escape;   // kVK_Escape
        case 0x31: return Key::Space;    // kVK_Space
        case 0x7B: return Key::Left;     // kVK_LeftArrow
        case 0x7E: return Key::Up;       // kVK_UpArrow
        case 0x7D: return Key::Right;    // kVK_RightArrow
        case 0x7C: return Key::Down;     // kVK_DownArrow
        case 0x38: return Key::LeftShift;
        case 0x3C: return Key::RightShift;
        case 0x3B: return Key::LeftControl;
        case 0x3E: return Key::RightControl;
        case 0x00: return Key::A; case 0x0B: return Key::B; case 0x08: return Key::C;
        case 0x02: return Key::D; case 0x0E: return Key::E; case 0x03: return Key::F;
        case 0x05: return Key::G; case 0x04: return Key::H; case 0x22: return Key::I;
        case 0x26: return Key::J; case 0x28: return Key::K; case 0x25: return Key::L;
        case 0x2E: return Key::M; case 0x2D: return Key::N; case 0x1F: return Key::O;
        case 0x23: return Key::P; case 0x0C: return Key::Q; case 0x0F: return Key::R;
        case 0x01: return Key::S; case 0x11: return Key::T; case 0x20: return Key::U;
        case 0x09: return Key::V; case 0x0D: return Key::W; case 0x07: return Key::X;
        case 0x10: return Key::Y; case 0x06: return Key::Z;
        case 0x1D: return Key::Digit0; case 0x12: return Key::Digit1; case 0x13: return Key::Digit2;
        case 0x14: return Key::Digit3; case 0x15: return Key::Digit4; case 0x17: return Key::Digit5;
        case 0x16: return Key::Digit6; case 0x18: return Key::Digit7; case 0x19: return Key::Digit8;
        case 0x1A: return Key::Digit9;
        default: return Key::Unknown;
    }
}

} // namespace

struct Window::Impl {
    NSWindow* window = nil;
    NSOpenGLContext* context = nil;
    id closeObserver = nil;
    int width = 0, height = 0;
    int fbWidth = 0, fbHeight = 0;
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
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app finishLaunching];

        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFAStencilSize, 8,
            NSOpenGLPFASampleBuffers, hints.samples > 0 ? (NSOpenGLPixelFormatAttribute)1 : (NSOpenGLPixelFormatAttribute)0,
            NSOpenGLPFASamples, hints.samples > 0 ? (NSOpenGLPixelFormatAttribute)hints.samples : (NSOpenGLPixelFormatAttribute)0,
            NSOpenGLPFANoRecovery,
            0,
        };
        NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        if (!format) return false;
        impl_->context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
        [format release];
        if (!impl_->context) return false;

        NSRect frame = NSMakeRect(0, 0, width, height);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
        if (hints.resizable) style |= NSWindowStyleMaskResizable;
        std::string t(title);
        impl_->window = [[NSWindow alloc] initWithContentRect:frame styleMask:style
                                                    backing:NSBackingStoreBuffered defer:NO];
        if (!impl_->window) return false;
        [impl_->window setTitle:[NSString stringWithUTF8String:t.c_str()]];
        [impl_->window setAcceptsMouseMovedEvents:YES];
        [impl_->context setView:impl_->window.contentView];
        [impl_->context makeCurrentContext];

        GLint swap = hints.vsync ? 1 : 0;
        [impl_->context setValues:&swap forParameter:NSOpenGLCPSwapInterval];

        __weak Window::Impl* weakImpl = impl_.get();
        impl_->closeObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:NSWindowWillCloseNotification object:impl_->window queue:nil
            usingBlock:^(NSNotification*) {
                Window::Impl* i = weakImpl;
                if (i) {
                    i->closeRequested = true;
                    if (i->onClose) i->onClose();
                }
            }];

        [impl_->window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];

        NSRect content = [impl_->window.contentView frame];
        NSSize backing = [impl_->window.contentView convertSizeToBacking:content.size];
        impl_->width = (int)content.size.width;
        impl_->height = (int)content.size.height;
        impl_->fbWidth = (int)backing.width;
        impl_->fbHeight = (int)backing.height;
        impl_->open = true;
        impl_->closeRequested = false;
        impl_->startTime = std::chrono::steady_clock::now();

        gl::init();
        return true;
    }
}

void Window::destroy() {
    if (!impl_) return;
    if (impl_->closeObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:impl_->closeObserver];
        impl_->closeObserver = nil;
    }
    @autoreleasepool {
        if (impl_->context) {
            [NSOpenGLContext clearCurrentContext];
            [impl_->context release];
            impl_->context = nil;
        }
        if (impl_->window) {
            [impl_->window release];
            impl_->window = nil;
        }
    }
    impl_->open = false;
}

bool Window::isOpen() const { return impl_->open; }
bool Window::shouldClose() const { return impl_->closeRequested; }
void Window::setShouldClose(bool value) { impl_->closeRequested = value; }

void Window::pollEvents() {
    if (!impl_->window) return;
    @autoreleasepool {
        NSEvent* ev = nil;
        while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil
                                            inMode:NSDefaultRunLoopMode dequeue:YES]) != nil) {
            switch (ev.type) {
                case NSEventTypeKeyDown:
                case NSEventTypeKeyUp: {
                    const Key key = mapKeyCode(ev.keyCode);
                    if (key != Key::Unknown && impl_->onKey) {
                        Action a = ev.type == NSEventTypeKeyUp ? Action::Release
                                   : ev.isARepeat ? Action::Repeat : Action::Press;
                        impl_->onKey(key, a);
                    }
                    break;
                }
                case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp:
                case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp:
                case NSEventTypeOtherMouseDown: case NSEventTypeOtherMouseUp: {
                    if (impl_->onMouseButton) {
                        MouseButton b = ev.type == NSEventTypeLeftMouseDown || ev.type == NSEventTypeLeftMouseUp
                                            ? MouseButton::Left
                                            : ev.type == NSEventTypeRightMouseDown || ev.type == NSEventTypeRightMouseUp
                                                  ? MouseButton::Right : MouseButton::Middle;
                        const bool press = ev.type == NSEventTypeLeftMouseDown || ev.type == NSEventTypeRightMouseDown ||
                                           ev.type == NSEventTypeOtherMouseDown;
                        impl_->onMouseButton(b, press ? Action::Press : Action::Release);
                    }
                    break;
                }
                case NSEventTypeMouseMoved: {
                    if (impl_->onCursorPos) {
                        NSPoint p = [ev locationInWindow];
                        impl_->onCursorPos(p.x, impl_->height - p.y);
                    }
                    break;
                }
                case NSEventTypeScrollWheel:
                    if (impl_->onScroll) impl_->onScroll(ev.deltaX, ev.deltaY);
                    break;
                default:
                    break;
            }
            // Detect resizes (frame change) each poll.
            NSRect content = [impl_->window.contentView frame];
            const int w = (int)content.size.width, h = (int)content.size.height;
            if (w != impl_->width || h != impl_->height) {
                impl_->width = w;
                impl_->height = h;
                NSSize backing = [impl_->window.contentView convertSizeToBacking:content.size];
                impl_->fbWidth = (int)backing.width;
                impl_->fbHeight = (int)backing.height;
                if (impl_->onResize) impl_->onResize(w, h);
            }
            [NSApp sendEvent:ev];
        }
    }
}

void Window::swapBuffers() {
    if (impl_->context) [impl_->context flushBuffer];
}

void Window::makeCurrent() {
    if (impl_->context) [impl_->context makeCurrentContext];
}

int Window::width() const { return impl_->width; }
int Window::height() const { return impl_->height; }
int Window::framebufferWidth() const { return impl_->fbWidth; }
int Window::framebufferHeight() const { return impl_->fbHeight; }

double Window::time() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - impl_->startTime).count();
}

void Window::setTitle(std::string_view title) {
    if (!impl_->window) return;
    std::string t(title);
    [impl_->window setTitle:[NSString stringWithUTF8String:t.c_str()]];
}

void Window::setResizeCallback(ResizeCallback cb) { impl_->onResize = std::move(cb); }
void Window::setKeyCallback(KeyCallback cb) { impl_->onKey = std::move(cb); }
void Window::setMouseButtonCallback(MouseButtonCallback cb) { impl_->onMouseButton = std::move(cb); }
void Window::setCursorPosCallback(CursorPosCallback cb) { impl_->onCursorPos = std::move(cb); }
void Window::setScrollCallback(ScrollCallback cb) { impl_->onScroll = std::move(cb); }
void Window::setCloseCallback(CloseCallback cb) { impl_->onClose = std::move(cb); }

} // namespace fuhgl
