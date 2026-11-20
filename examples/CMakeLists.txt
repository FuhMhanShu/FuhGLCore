add_executable(triangle triangle/main.cpp)
target_link_libraries(triangle PRIVATE fuhglcore)

add_executable(screenshot screenshot/main.cpp)
target_link_libraries(screenshot PRIVATE fuhglcore)
