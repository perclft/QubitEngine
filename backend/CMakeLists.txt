cmake_minimum_required(VERSION 3.20)
project(QubitEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find Dependencies
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(OpenMP REQUIRED) # FIX: Standard OpenMP detection

# 2. Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
)

# 3. Define Sources (Explicitly separate Implementation and Header)
set(SOURCES
    src/main.cpp
    src/QuantumRegister.cpp
    src/ServiceImpl.cpp
    src/generated/quantum.pb.cc
    src/generated/quantum.grpc.pb.cc
)

# 4. Target
add_executable(qubit_engine ${SOURCES})

# 5. Link Libraries
target_link_libraries(qubit_engine
    PRIVATE
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
    absl::base
    absl::synchronization
    absl::strings
    OpenMP::OpenMP_CXX # FIX: Portable OpenMP link
)

# 6. Compiler Options (Release Mode)
if(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(qubit_engine PRIVATE -O3 -march=native)
endif()
