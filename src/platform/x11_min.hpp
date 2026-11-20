#pragma once

// Minimal X11/GLX declarations so FuhGLCore builds WITHOUT libX11-dev / GL
// headers installed: only the types, constants and functions actually used
// are declared here, and libX11/libGL are dlopen'ed at runtime.
// The struct layouts below match the stable X11 ABI.

using XID = unsigned long;
using Atom = unsigned long;
using Time_ = unsigned long;
using Bool_ = int;
using VisualID = unsigned long;
using Pixmap = XID;
using Colormap = XID;
using Cursor = XID;
using XWindow = XID;

struct Display; // opaque (struct _XDisplay in libX11)
struct Visual;  // opaque

struct XVisualInfo {
    Visual* visual;
    VisualID visualid;
    int screen;
    int depth;
    int cls;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
};

struct XSetWindowAttributes {
    Pixmap background_pixmap;
    unsigned long background_pixel;
    Pixmap border_pixmap;
    unsigned long border_pixel;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool_ save_under;
    long event_mask;
    long do_not_propagate_mask;
    Bool_ override_redirect;
    Colormap colormap;
    Cursor cursor;
};

struct XAnyEvent {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    XWindow window;
};
struct XKeyEvent {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    XWindow window;
    XWindow root;
    XWindow subwindow;
    Time_ time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    int same_screen;
};
struct XButtonEvent {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    XWindow window;
    XWindow root;
    XWindow subwindow;
    Time_ time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int button;
    int same_screen;
};
struct XMotionEvent {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    XWindow window;
    XWindow root;
    XWindow subwindow;
    Time_ time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    char is_hint;
    int same_screen;
};
struct XConfigureEvent {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    XWindow event;
    XWindow window;
    int x, y;
    int width, height;
    int border_width;
    XWindow above;
    int override_redirect;
};
struct XClientMessageEvent {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    XWindow window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
};
union XEvent {
    int type;
    XAnyEvent xany;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XMotionEvent xmotion;
    XConfigureEvent xconfigure;
    XClientMessageEvent xclient;
    long pad[24];
};

// --- X11 constants ------------------------------------------------------------
enum : int {
    X11_KeyPress = 2, X11_KeyRelease = 3, X11_ButtonPress = 4, X11_ButtonRelease = 5,
    X11_MotionNotify = 6, X11_ConfigureNotify = 22, X11_ClientMessage = 33, X11_DestroyNotify = 17,
};
enum : long {
    X11_KeyPressMask = 1L << 0, X11_KeyReleaseMask = 1L << 1,
    X11_ButtonPressMask = 1L << 2, X11_ButtonReleaseMask = 1L << 3,
    X11_PointerMotionMask = 1L << 6, X11_StructureNotifyMask = 1L << 17,
};
enum : unsigned long { X11_CWEventMask = 1UL << 11, X11_CWColormap = 1UL << 13 };
enum : int { X11_InputOutput = 1, X11_AllocNone = 0 };
enum : unsigned int { X11_Button1 = 1, X11_Button2 = 2, X11_Button3 = 3, X11_Button4 = 4, X11_Button5 = 5 };
// --- keysyms (subset) -----------------------------------------------------------
enum : unsigned long {
    XK_Escape_ = 0xFF1B, XK_space_ = 0x0020,
    XK_Left_ = 0xFF51, XK_Up_ = 0xFF52, XK_Right_ = 0xFF53, XK_Down_ = 0xFF54,
    XK_Shift_L_ = 0xFFE1, XK_Shift_R_ = 0xFFE2, XK_Control_L_ = 0xFFE3, XK_Control_R_ = 0xFFE4,
};

// --- GLX constants --------------------------------------------------------------
enum : int {
    GLX_DOUBLEBUFFER = 5, GLX_RED_SIZE = 8, GLX_GREEN_SIZE = 9, GLX_BLUE_SIZE = 10,
    GLX_ALPHA_SIZE = 11, GLX_DEPTH_SIZE = 12, GLX_STENCIL_SIZE = 13,
    GLX_RGBA_BIT = 0x00000001, GLX_TRUE_COLOR = 0x8002,
    GLX_SAMPLE_BUFFERS = 100000, GLX_SAMPLES = 100001,
    GLX_CONTEXT_MAJOR_VERSION_ARB = 0x2091, GLX_CONTEXT_MINOR_VERSION_ARB = 0x2092,
    GLX_CONTEXT_PROFILE_MASK_ARB = 0x9126,
    GLX_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001,
    GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002,
    GLX_NONE = 0,
};

using GLXFBConfig = void*;
using GLXContext = void*;
using GLXDrawable = XID;

// --- function pointer types ------------------------------------------------------
using XOpenDisplayFn = Display* (*)(const char*);
using XCloseDisplayFn = int (*)(Display*);
using XDefaultScreenFn = int (*)(Display*);
using XRootWindowFn = XWindow (*)(Display*, int);
using XCreateColormapFn = Colormap (*)(Display*, XWindow, Visual*, int);
using XCreateWindowFn = XWindow (*)(Display*, XWindow, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual*, unsigned long, XSetWindowAttributes*);
using XDestroyWindowFn = int (*)(Display*, XWindow);
using XMapWindowFn = int (*)(Display*, XWindow);
using XStoreNameFn = int (*)(Display*, XWindow, const char*);
using XSelectInputFn = int (*)(Display*, XWindow, long);
using XPendingFn = int (*)(Display*);
using XNextEventFn = int (*)(Display*, XEvent*);
using XInternAtomFn = Atom (*)(Display*, const char*, Bool_);
using XSetWMProtocolsFn = int (*)(Display*, XWindow, Atom*, int);
using XFreeFn = int (*)(void*);
using XLookupKeysymFn = unsigned long (*)(XKeyEvent*, int);

using GlXChooseFBConfigFn = GLXFBConfig* (*)(Display*, int, const int*, int*);
using GlXGetVisualFromFBConfigFn = XVisualInfo* (*)(Display*, GLXFBConfig);
using GlXCreateContextFn = GLXContext (*)(Display*, XVisualInfo*, GLXContext, Bool_);
using GlXCreateContextAttribsFn = GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool_, const int*);
using GlXMakeCurrentFn = Bool_ (*)(Display*, GLXDrawable, GLXContext);
using GlXSwapBuffersFn = void (*)(Display*, GLXDrawable);
using GlXDestroyContextFn = void (*)(Display*, GLXContext);
using GlXGetProcAddressFn = void* (*)(const unsigned char*);
using GlXSwapIntervalEXTFn = int (*)(Display*, GLXDrawable, int);
using GlXSwapIntervalSGIFn = int (*)(int);
