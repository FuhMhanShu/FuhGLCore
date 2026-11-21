add_executable(fuhgl_tests
    main.cpp
    test_math.cpp
    test_camera.cpp
    test_loader.cpp
    test_frustum.cpp
    test_shader_includes.cpp
    test_info.cpp
    test_solids.cpp
    test_light.cpp
    test_mesh.cpp
    test_shadow.cpp
    test_text.cpp
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

# Headless test for F14 light types: forward-renders a box top with directional /
# spot / point lights and verifies the center pixel brightness per light type.
add_executable(fuhgl_gl_smoke_lights gl_smoke_lights.cpp)
target_link_libraries(fuhgl_gl_smoke_lights PRIVATE fuhglcore)
add_test(NAME gl_smoke_lights COMMAND fuhgl_gl_smoke_lights)

# Headless test for F15 shadow mapping: floor + casters rendered with/without a
# ShadowMap; shadowed pixels must get darker. Skips without GL >= 3.3.
add_executable(fuhgl_gl_smoke_shadows gl_smoke_shadows.cpp)
target_link_libraries(fuhgl_gl_smoke_shadows PRIVATE fuhglcore)
add_test(NAME gl_smoke_shadows COMMAND fuhgl_gl_smoke_shadows)

# Headless test for F16 ForwardRenderer: auto shadow pass (blob gets darker) +
# exponential fog (far pixels move toward the fog color). Skips without GL >= 3.3.
add_executable(fuhgl_gl_smoke_forward gl_smoke_forward.cpp)
target_link_libraries(fuhgl_gl_smoke_forward PRIVATE fuhglcore)
add_test(NAME gl_smoke_forward COMMAND fuhgl_gl_smoke_forward)

# Headless test for F17 DeferredRenderer: SSAO / bloom / god rays / parallax each
# verified by toggling the effect and comparing pixels. Skips without GL >= 3.3.
add_executable(fuhgl_gl_smoke_deferred_fx gl_smoke_deferred_fx.cpp)
target_link_libraries(fuhgl_gl_smoke_deferred_fx PRIVATE fuhglcore)
add_test(NAME gl_smoke_deferred_fx COMMAND fuhgl_gl_smoke_deferred_fx)

# Headless test for P2 (complete GL 4.5 DSA table): named buffers/textures/samplers/
# VAOs/FBOs/renderbuffers/queries/TFs with readback verification. Skips without GL 4.5.
add_executable(fuhgl_gl_smoke_dsa gl_smoke_dsa.cpp)
target_link_libraries(fuhgl_gl_smoke_dsa PRIVATE fuhglcore)
add_test(NAME gl_smoke_dsa COMMAND fuhgl_gl_smoke_dsa)

# Headless test for F19 TextRenderer: renders text into an FBO and verifies that
# glyph pixels are actually written (bright pixels where text was drawn).
# Skips cleanly without GL >= 3.3.
add_executable(fuhgl_gl_smoke_text gl_smoke_text.cpp)
target_link_libraries(fuhgl_gl_smoke_text PRIVATE fuhglcore)
add_test(NAME gl_smoke_text COMMAND fuhgl_gl_smoke_text)
