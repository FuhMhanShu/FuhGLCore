add_executable(fuhgl_tests
    main.cpp
    test_math.cpp
    test_camera.cpp
    test_loader.cpp
    test_frustum.cpp
    test_shader_includes.cpp
    test_info.cpp
    test_solids.cpp
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

# Headless test for the F9 buffer/texture/image extensions (mapRange, copies,
# compressed textures, image binding, indirect compute, SSBO, VAO 4.3).
add_executable(fuhgl_gl_smoke_buffers gl_smoke_buffers.cpp)
target_link_libraries(fuhgl_gl_smoke_buffers PRIVATE fuhglcore)
add_test(NAME gl_smoke_buffers COMMAND fuhgl_gl_smoke_buffers)

# Headless test for solids/CSG + preset materials (marching tetrahedra mesh,
# procedural checker texture, DeferredPipeline render, pixel check).
add_executable(fuhgl_gl_smoke_solids gl_smoke_solids.cpp)
target_link_libraries(fuhgl_gl_smoke_solids PRIVATE fuhglcore)
add_test(NAME gl_smoke_solids COMMAND fuhgl_gl_smoke_solids)

# Headless test for F13: state setters, batching draws, debug labels, queries/sync,
# internal formats, image copies/clears — verified via pixel readback where possible.
add_executable(fuhgl_gl_smoke_state gl_smoke_state.cpp)
target_link_libraries(fuhgl_gl_smoke_state PRIVATE fuhglcore)
add_test(NAME gl_smoke_state COMMAND fuhgl_gl_smoke_state)
