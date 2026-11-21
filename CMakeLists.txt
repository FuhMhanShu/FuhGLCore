cmake_minimum_required(VERSION 3.20)
project(FuhGLCore VERSION 0.1.0 LANGUAGES CXX)

option(FUHGL_BUILD_EXAMPLES "Build FuhGLCore examples" ON)
option(FUHGL_BUILD_TESTS "Build FuhGLCore tests" ON)

if(APPLE)
    enable_language(OBJCXX)
endif()

add_library(fuhglcore STATIC
    src/gl/loader.cpp
    src/gl/error.cpp
    src/gl/info.cpp
    src/gl/shader.cpp
    src/gl/program.cpp
    src/gl/buffer.cpp
    src/gl/vao.cpp
    src/gl/texture.cpp
    src/gl/renderbuffer.cpp
    src/gl/framebuffer.cpp
    src/gl/state.cpp
    src/gl/sampler.cpp
    src/gl/sync.cpp
    src/gl/program_pipeline.cpp
    src/gl/transform_feedback.cpp
    src/gl/ext/bindless.cpp
    src/skybox.cpp
    src/render/mesh.cpp
    src/render/culling.cpp
    src/render/material.cpp
    src/render/environment.cpp
    src/render/graph.cpp
    src/render/light.cpp
    src/render/shadow.cpp
    src/render/forward_renderer.cpp
    src/render/deferred_renderer.cpp
    src/render/forward.cpp
    src/render/deferred.cpp
    src/render/procedural.cpp
    src/render/solid.cpp
    src/render/text.cpp
    src/render/wireframe.cpp
)
add_library(FuhGLCore::fuhglcore ALIAS fuhglcore)

target_include_directories(fuhglcore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fuhglcore PUBLIC cxx_std_17)
set_target_properties(fuhglcore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(WIN32)
    target_sources(fuhglcore PRIVATE src/platform/window_win32.cpp)
    target_link_libraries(fuhglcore PUBLIC user32 gdi32 opengl32)
elseif(APPLE)
    target_sources(fuhglcore PRIVATE src/platform/window_cocoa.mm)
    target_link_libraries(fuhglcore PUBLIC "-framework Cocoa" "-framework OpenGL")
    set_source_files_properties(src/platform/window_cocoa.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(UNIX)
    # Linux window backend: X11 (default) or EGL (Wayland window / surfaceless headless)
    set(FUHGL_LINUX_BACKEND "X11" CACHE STRING "Linux window backend: X11 or EGL")
    if(FUHGL_LINUX_BACKEND STREQUAL "EGL")
        target_sources(fuhglcore PRIVATE src/platform/window_egl.cpp)
    else()
        target_sources(fuhglcore PRIVATE src/platform/window_x11.cpp)
    endif()
    target_link_libraries(fuhglcore PUBLIC ${CMAKE_DL_LIBS})
else()
    target_sources(fuhglcore PRIVATE src/platform/window_unsupported.cpp)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fuhglcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- hardening flags (GNU/Clang, Linux; opt out with -DFUHGL_ENABLE_HARDENING=OFF) ---
option(FUHGL_ENABLE_HARDENING "Compiler/linker hardening (stack protector, FORTIFY, RELRO) for GNU/Clang" ON)
if(FUHGL_ENABLE_HARDENING AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fstack-protector-strong" FUHGL_HAS_STACK_PROTECTOR)
    if(FUHGL_HAS_STACK_PROTECTOR)
        target_compile_options(fuhglcore PRIVATE -fstack-protector-strong)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        include(CheckLinkerFlag)
        check_cxx_compiler_flag("-fstack-clash-protection" FUHGL_HAS_STACK_CLASH)
        if(FUHGL_HAS_STACK_CLASH)
            target_compile_options(fuhglcore PRIVATE -fstack-clash-protection)
        endif()
        # _FORTIFY_SOURCE only does something with optimization enabled
        target_compile_options(fuhglcore PRIVATE
            $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:-D_FORTIFY_SOURCE=2>)
        check_linker_flag(CXX "-Wl,-z,relro,-z,now" FUHGL_HAS_FULL_RELRO)
        if(FUHGL_HAS_FULL_RELRO)
            target_link_options(fuhglcore PUBLIC -Wl,-z,relro,-z,now)
        endif()
        check_linker_flag(CXX "-Wl,-z,noexecstack" FUHGL_HAS_NOEXECSTACK)
        if(FUHGL_HAS_NOEXECSTACK)
            target_link_options(fuhglcore PUBLIC -Wl,-z,noexecstack)
        endif()
    endif()
endif()

# --- install / package export -------------------------------------------------
include(GNUInstallDirs)
install(TARGETS fuhglcore
    EXPORT FuhGLCoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
install(EXPORT FuhGLCoreTargets
    NAMESPACE FuhGLCore::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FuhGLCore
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/FuhGLCoreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    cmake/FuhGLCoreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FuhGLCoreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FuhGLCore
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/FuhGLCoreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/FuhGLCoreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FuhGLCore
)

# --- examples / tests ---------------------------------------------------------
if(FUHGL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(FUHGL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
