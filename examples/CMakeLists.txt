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
