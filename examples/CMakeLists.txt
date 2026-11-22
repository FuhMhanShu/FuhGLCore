add_executable(triangle triangle/main.cpp)
target_link_libraries(triangle PRIVATE fuhglcore)

add_executable(screenshot screenshot/main.cpp)
target_link_libraries(screenshot PRIVATE fuhglcore)

add_executable(deferred deferred/main.cpp)
target_link_libraries(deferred PRIVATE fuhglcore)

add_executable(gpuinfo gpuinfo/main.cpp)
target_link_libraries(gpuinfo PRIVATE fuhglcore)

add_executable(stream stream/main.cpp)
target_link_libraries(stream PRIVATE fuhglcore)

add_executable(solids solids/main.cpp)
target_link_libraries(solids PRIVATE fuhglcore)

add_executable(lights lights/main.cpp)
target_link_libraries(lights PRIVATE fuhglcore)

add_executable(shadows shadows/main.cpp)
target_link_libraries(shadows PRIVATE fuhglcore)

add_executable(forward forward/main.cpp)
target_link_libraries(forward PRIVATE fuhglcore)

add_executable(postfx postfx/main.cpp)
target_link_libraries(postfx PRIVATE fuhglcore)

add_executable(text text/main.cpp)
target_link_libraries(text PRIVATE fuhglcore)

add_executable(instancing instancing/main.cpp)
target_link_libraries(instancing PRIVATE fuhglcore)

add_executable(showcase showcase/main.cpp)
target_link_libraries(showcase PRIVATE fuhglcore)

add_executable(demo demo/main.cpp)
target_link_libraries(demo PRIVATE fuhglcore)

add_executable(features features/main.cpp)
target_link_libraries(features PRIVATE fuhglcore)
