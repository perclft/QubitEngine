cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-O3 -mavx2 -mfma)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 1. Include Directories (Source + Generated)
include_directories(backend/src)
include_directories(backend/src/generated)

# 2. Find Sources
file(GLOB_RECURSE ENGINE_SRCS "backend/src/*.cpp")
file(GLOB_RECURSE PROTO_SRCS "backend/src/generated/*.cc")

add_executable(qubit_engine 
    ${ENGINE_SRCS} 
    ${PROTO_SRCS}
)

target_link_libraries(qubit_engine
    PRIVATE
    gRPC::grpc++
    gRPC::grpc
    gRPC::gpr
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    dl
)
