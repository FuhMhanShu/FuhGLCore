add_executable(fuhgl_tests
    main.cpp
    test_math.cpp
    test_camera.cpp
    test_loader.cpp
    test_frustum.cpp
    test_shader_includes.cpp
)
target_link_libraries(fuhgl_tests PRIVATE fuhglcore)
add_test(NAME fuhgl_tests COMMAND fuhgl_tests)

# Headless GL smoke test: renders a triangle into an FBO via a surfaceless context
# and verifies the pixel. Skips (passes) when no GL implementation is available.
add_executable(fuhgl_gl_smoke gl_smoke.cpp)
target_link_libraries(fuhgl_gl_smoke PRIVATE fuhglcore)
add_test(NAME gl_smoke COMMAND fuhgl_gl_smoke)

# Headless deferred-pipeline render test (lit box + sky background pixel check).
add_executable(fuhgl_gl_smoke_deferred gl_smoke_deferred.cpp)
target_link_libraries(fuhgl_gl_smoke_deferred PRIVATE fuhglcore)
add_test(NAME gl_smoke_deferred COMMAND fuhgl_gl_smoke_deferred)
