cmake_minimum_required(VERSION 3.15)
project(alu_grpc_server CXX)

set(CMAKE_CXX_STANDARD 17)

# Find Protobuf and gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Generate gRPC and Protobuf C++ sources
set(PROTO_FILE "alu_image.proto")
set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/alu_image.pb.cc")
set(PROTO_HEADER "${CMAKE_CURRENT_BINARY_DIR}/alu_image.pb.h")
set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/alu_image.grpc.pb.cc")
set(GRPC_HEADER "${CMAKE_CURRENT_BINARY_DIR}/alu_image.grpc.pb.h")

add_custom_command(
    OUTPUT "${PROTO_SRC}" "${PROTO_HEADER}" "${GRPC_SRC}" "${GRPC_HEADER}"
    COMMAND protobuf::protoc
    ARGS --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
         --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
         -I "${CMAKE_CURRENT_SOURCE_DIR}"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
)

# Add the Alu Image backend
set(ALU_BACKEND_SRC "../std/image_backend.cpp")

add_executable(alu_grpc_server
    server.cpp
    ${ALU_BACKEND_SRC}
    ${PROTO_SRC}
    ${GRPC_SRC}
)

target_include_directories(alu_grpc_server PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    "../std"
)

target_link_libraries(alu_grpc_server
    gRPC::grpc++
    protobuf::libprotobuf
)
