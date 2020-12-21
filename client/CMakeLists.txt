cmake_minimum_required(VERSION 3.16)
project(client)

find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_executable(client main.cpp)
target_link_libraries(
    client PRIVATE
    core
    GLEW::GLEW
    glfw
)