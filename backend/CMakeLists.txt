cmake_minimum_required(VERSION 3.20)
project(QubitEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find Dependencies (vcpkg)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
find_package(GTest CONFIG REQUIRED) # Added for Testing

# 2. Locate gRPC Plugin
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)

# 3. Auto-Generate Protobuf/gRPC Sources
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILES ${PROTO_SRC_DIR}/quantum.proto)
set(GENERATED_PB_SRCS
    ${PROTO_GEN_DIR}/quantum.pb.cc
    ${PROTO_GEN_DIR}/quantum.grpc.pb.cc
    ${PROTO_GEN_DIR}/quantum.pb.h
    ${PROTO_GEN_DIR}/quantum.grpc.pb.h
)

add_custom_command(
    OUTPUT ${GENERATED_PB_SRCS}
    COMMAND protobuf::protoc
      --proto_path=${PROTO_SRC_DIR}
      --cpp_out=${PROTO_GEN_DIR}
      --grpc_out=${PROTO_GEN_DIR}
      --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
      ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Running C++ protocol buffer compiler on quantum.proto"
)

# 4. Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTO_GEN_DIR}
)

# 5. Build The Main Engine
add_executable(qubit_engine 
    src/main.cpp
    src/QuantumRegister.cpp
    src/ServiceImpl.cpp
    ${GENERATED_PB_SRCS}
)

target_link_libraries(qubit_engine
    PRIVATE
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
    absl::base
    absl::synchronization
    absl::strings
    OpenMP::OpenMP_CXX
)

# 6. Build The Unit Tests
enable_testing()

add_executable(qubit_tests 
    tests/QuantumTests.cpp 
    src/QuantumRegister.cpp # Link implementation (but NOT main.cpp)
)

target_link_libraries(qubit_tests 
    PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    OpenMP::OpenMP_CXX
)

# Register with CTest
include(GoogleTest)
gtest_discover_tests(qubit_tests)

# 7. Optimization
if(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(qubit_engine PRIVATE -O3 -march=native)
    target_compile_options(qubit_tests PRIVATE -O3 -march=native)
endif()
