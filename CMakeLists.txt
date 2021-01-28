cmake_minimum_required(VERSION 3.16)
project(l)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(PROTOBUF_IMPORT_DIRS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(core)
add_subdirectory(server)
add_subdirectory(client)