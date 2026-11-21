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
