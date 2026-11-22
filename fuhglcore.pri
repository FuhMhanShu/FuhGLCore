# FuhGLCore — qmake dependency include (mirrors CMakeLists.txt).
#
# Use FuhGLCore as a dependency in a qmake project:
#   qmake6 "FUHGLCORE_DIR=/path/to/FuhGLCore" yourapp.pro
#   # yourapp.pro:
#   include($$FUHGLCORE_DIR/fuhglcore.pri)
#
# The FuhGLCore sources are compiled directly into the consuming target
# (FuhGLCore stays zero-dependency; no prebuilt library is needed).
# FUHGLCORE_DIR resolution order: qmake variable > environment variable > error.

isEmpty(FUHGLCORE_DIR) {
    FUHGLCORE_DIR = $$(FUHGLCORE_DIR)
}
isEmpty(FUHGLCORE_DIR) {
    error("fuhglcore.pri: set FUHGLCORE_DIR to a FuhGLCore checkout (qmake var or environment), e.g. qmake6 FUHGLCORE_DIR=/path/to/FuhGLCore yourapp.pro")
}
FUHGLCORE_DIR = $$absolute_path($$FUHGLCORE_DIR)

CONFIG += c++17

INCLUDEPATH += $$FUHGLCORE_DIR/include

HEADERS += $$files($$FUHGLCORE_DIR/include/fuhgl/**/*.hpp, true)
SOURCES += \
    $$FUHGLCORE_DIR/src/skybox.cpp \
    $$files($$FUHGLCORE_DIR/src/gl/**/*.cpp, true) \
    $$files($$FUHGLCORE_DIR/src/render/**/*.cpp, true)

win32 {
    SOURCES += $$FUHGLCORE_DIR/src/platform/window_win32.cpp
    LIBS += -luser32 -lgdi32 -lopengl32
} else:macx {
    OBJECTIVE_SOURCES += $$FUHGLCORE_DIR/src/platform/window_cocoa.mm
    QMAKE_OBJECTIVE_CXXFLAGS += -fobjc-arc
    LIBS += -framework Cocoa -framework OpenGL
} else:unix {
    # Linux window backend: X11 (default) or EGL (Wayland window / surfaceless headless)
    #   qmake6 "FUHGL_LINUX_BACKEND=EGL" ...
    isEmpty(FUHGL_LINUX_BACKEND): FUHGL_LINUX_BACKEND = X11
    equals(FUHGL_LINUX_BACKEND, EGL) {
        SOURCES += $$FUHGLCORE_DIR/src/platform/window_egl.cpp
    } else {
        SOURCES += $$FUHGLCORE_DIR/src/platform/window_x11.cpp
    }
    LIBS += $$QMAKE_LIBS_DLOPEN
} else {
    SOURCES += $$FUHGLCORE_DIR/src/platform/window_unsupported.cpp
}

gcc|clang {
    QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
}
