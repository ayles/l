cmake_minimum_required(VERSION 3.16)
project(core)

find_package(glm REQUIRED)
find_package(Protobuf REQUIRED)
find_package(ENet REQUIRED)

set(PROTOS
    object.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTOS})

add_library(core STATIC main.cpp ${PROTO_SRCS})
target_include_directories(core PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
    core PUBLIC

    glm::glm
    protobuf::libprotoc
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
    enet
)