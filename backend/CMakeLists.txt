cmake_minimum_required(VERSION 3.20)

# Project Name
project(QubitEngine CXX)

# Standard: C++17 or C++20 for modern features (std::filesystem, etc.)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. FIND DEPENDENCIES (via vcpkg or system)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED) 

# 2. INCLUDE DIRECTORIES
# We need to look in:
# - src/ (for your .hpp files)
# - include/ (for public headers)
# - src/generated/ (for the protoc output)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
)

# 3. DEFINE SOURCES
# Explicitly listing files gives you more control than file(GLOB)
set(SOURCES
    src/main.cpp
    src/QuantumRegister.cpp
    src/ServiceImpl.cpp
    # Include the Generated Protobuf/gRPC implementation files
    src/generated/quantum.pb.cc
    src/generated/quantum.grpc.pb.cc
)

# 4. EXECUTABLE TARGET
add_executable(qubit_engine ${SOURCES})

# 5. LINK LIBRARIES
# Link against the specific gRPC and Protobuf targets
target_link_libraries(qubit_engine
    PRIVATE
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
    absl::base
    absl::synchronization
    absl::strings
)

# 6. COMPILER FLAGS (Optimization)
if(CMAKE_BUILD_TYPE MATCHES Release)
    # AVX2 instructions for faster Matrix Multiplication
    target_compile_options(qubit_engine PRIVATE -O3 -march=native -fopenmp)
    target_link_libraries(qubit_engine PRIVATE gomp) # Link OpenMP
endif()
