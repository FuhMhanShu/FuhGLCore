add_executable(fuhgl_tests
    main.cpp
    test_math.cpp
    test_camera.cpp
    test_loader.cpp
)
target_link_libraries(fuhgl_tests PRIVATE fuhglcore)
add_test(NAME fuhgl_tests COMMAND fuhgl_tests)
