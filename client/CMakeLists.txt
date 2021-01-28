cmake_minimum_required(VERSION 3.16)
project(client)

find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_executable(client main.cpp shader_program.h dual_contour.h qef_simd.h thread_pool.h simplex.h iso_surface_generator.h marching_cubes.h)
target_link_libraries(
    client PRIVATE
    core
    GLEW::GLEW
    glfw
)