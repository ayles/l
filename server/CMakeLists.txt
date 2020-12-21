cmake_minimum_required(VERSION 3.16)
project(server)

add_executable(server main.cpp)
target_link_libraries(server PUBLIC core)