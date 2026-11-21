add_executable(fuhgl_tests
    main.cpp
    test_math.cpp
    test_quat.cpp
    test_noise.cpp
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
    test_wireframe.cpp
    test_environment.cpp
    test_graph.cpp
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

# Headless test for F20 Wireframe: draws a white AABB + a green mesh wireframe into
# an FBO and verifies via readback that line pixels of both colors were written.
# Skips cleanly without GL >= 3.3.
add_executable(fuhgl_gl_smoke_wireframe gl_smoke_wireframe.cpp)
target_link_libraries(fuhgl_gl_smoke_wireframe PRIVATE fuhglcore)
add_test(NAME gl_smoke_wireframe COMMAND fuhgl_gl_smoke_wireframe)

# Headless test for P3 (F21): program pipelines with separable programs + pixel check,
# transform feedback capture with readback, program binary round-trip, subroutines,
# GL 4.6 entry points (version-guarded). Skips without GL >= 4.1.
add_executable(fuhgl_gl_smoke_p3 gl_smoke_p3.cpp)
target_link_libraries(fuhgl_gl_smoke_p3 PRIVATE fuhglcore)
add_test(NAME gl_smoke_p3 COMMAND fuhgl_gl_smoke_p3)

# Headless test for F22 reflection: links a program with UBO + uniforms + attribs and
# verifies Program::activeUniforms()/activeAttributes()/uniformBlocks(). Skips < GL 3.3.
add_executable(fuhgl_gl_smoke_reflection gl_smoke_reflection.cpp)
target_link_libraries(fuhgl_gl_smoke_reflection PRIVATE fuhglcore)
add_test(NAME gl_smoke_reflection COMMAND fuhgl_gl_smoke_reflection)

# F24 instancing smoke test: one mesh + per-instance matrices, pixel-verified
add_executable(fuhgl_gl_smoke_instancing gl_smoke_instancing.cpp)
target_link_libraries(fuhgl_gl_smoke_instancing PRIVATE fuhglcore)
add_test(NAME gl_smoke_instancing COMMAND fuhgl_gl_smoke_instancing)

# F28/F29 PBR + IBL smoke test: metal vs dielectric pixels, env ambient, 5th G-buffer
# attachment (metallic/roughness/ao) readback. Skips without GL >= 3.3.
add_executable(fuhgl_gl_smoke_pbr gl_smoke_pbr.cpp)
target_link_libraries(fuhgl_gl_smoke_pbr PRIVATE fuhglcore)
add_test(NAME gl_smoke_pbr COMMAND fuhgl_gl_smoke_pbr)

# F30 CSM smoke test: ForwardRenderer with enableCsmShadows — shadow blob darker in a
# near view (cascade 0) and a far view (higher cascade). Skips without GL >= 3.3.
add_executable(fuhgl_gl_smoke_csm gl_smoke_csm.cpp)
target_link_libraries(fuhgl_gl_smoke_csm PRIVATE fuhglcore)
add_test(NAME gl_smoke_csm COMMAND fuhgl_gl_smoke_csm)

# F31 render graph smoke test: two-pass graph (fill + scale) readback, cycle rejected.
add_executable(fuhgl_gl_smoke_graph gl_smoke_graph.cpp)
target_link_libraries(fuhgl_gl_smoke_graph PRIVATE fuhglcore)
add_test(NAME gl_smoke_graph COMMAND fuhgl_gl_smoke_graph)

# F32 TAA smoke test: jitter changes the raw frame, accumulation converges.
add_executable(fuhgl_gl_smoke_taa gl_smoke_taa.cpp)
target_link_libraries(fuhgl_gl_smoke_taa PRIVATE fuhglcore)
add_test(NAME gl_smoke_taa COMMAND fuhgl_gl_smoke_taa)

# F33 SSR smoke test: emissive lamp reflected in the floor brightens it.
add_executable(fuhgl_gl_smoke_ssr gl_smoke_ssr.cpp)
target_link_libraries(fuhgl_gl_smoke_ssr PRIVATE fuhglcore)
add_test(NAME gl_smoke_ssr COMMAND fuhgl_gl_smoke_ssr)

# --- test semantics (F26) ---------------------------------------------------------------
# "unit": pure CPU tests, no GL context required.
# "gl_context": needs a GL context; the test exits with code 77 ("Skipped" via
# SKIP_RETURN_CODE) when no context / required GL version is available — a skip is
# always visible in ctest output, never a silent pass. Real assertion failures
# (exit 1) always fail the job.
set_tests_properties(fuhgl_tests PROPERTIES LABELS "unit")
set_tests_properties(
    gl_smoke gl_smoke_deferred gl_smoke_buffers gl_smoke_solids gl_smoke_state gl_smoke_lights
    gl_smoke_shadows gl_smoke_forward gl_smoke_deferred_fx gl_smoke_dsa gl_smoke_text
    gl_smoke_wireframe gl_smoke_p3 gl_smoke_reflection gl_smoke_instancing gl_smoke_pbr gl_smoke_csm gl_smoke_graph gl_smoke_taa gl_smoke_ssr
    PROPERTIES LABELS "gl_context" SKIP_RETURN_CODE 77)
