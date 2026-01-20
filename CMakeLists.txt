cmake_minimum_required(VERSION 3.15)
project(QubitEngine LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    add_compile_options(-O3 -mavx2 -mfma)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    add_compile_options(-O3) # NEON is default on Apple Silicon
endif()

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 1. Include Directories
include_directories(backend/src)
include_directories(backend/src/generated)

# 2. Find Sources
# Include .mm for Metal support
file(GLOB_RECURSE ENGINE_SRCS "backend/src/*.cpp" "backend/src/*.mm")
# We will manually compile protos in makefile or docker, so just find them
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
    "-framework Metal"
    "-framework Foundation"
    "-framework CoreGraphics"
)

# 4. Metal Backend verification executable
# add_executable(verify_metal backend/src/verify_metal_main.cpp)
# target_link_libraries(verify_metal qubit_engine)

# 4. Metal Shaders Compilation (Apple Only)
# Note: Disabled due to missing Metal Toolchain in CI/Env
# if(APPLE)
#     set(METAL_SRC "${CMAKE_SOURCE_DIR}/backend/src/shaders.metal")
# ...
# endif()
