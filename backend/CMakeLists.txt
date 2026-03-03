cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 20)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# --- Build Output ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

# Hotfix for Abseil headers missing from Protobuf/gRPC include paths on macOS Homebrew
include_directories(/opt/homebrew/include)

option(MPI_ENABLED "Enable MPI Distributed Computing" OFF)
if(MPI_ENABLED)
    find_package(MPI REQUIRED)
    if(MPI_FOUND)
        add_compile_definitions(MPI_ENABLED)
        include_directories(${MPI_CXX_INCLUDE_PATH})
        link_libraries(${MPI_CXX_LIBRARIES})
    endif()
endif()

# --- OpenMP (Multi-threading) ---
option(ENABLE_OPENMP "Enable OpenMP Multi-threading" ON)
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found. Multi-threading enabled.")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    else()
        message(STATUS "OpenMP not found. Multi-threading disabled.")
    endif()
endif()

# --- Hardware Acceleration (AVX2 / NEON) ---
option(ENABLE_HW_ACCEL "Enable Hardware Acceleration (AVX2/NEON)" ON)
if(ENABLE_HW_ACCEL)
    if(MSVC)
         # Windows (MSVC)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
        cmake_host_system_information(RESULT HOST_ARCH QUERY OS_PLATFORM)
        if(${HOST_ARCH} MATCHES "AMD64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64")
             message(STATUS "Enabling AVX2 instructions for MSVC")
             # Use generator expression to avoid passing this to nvcc
             add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/arch:AVX2>)
        else()
             message(STATUS "Not x64, skipping AVX2 for MSVC")
        endif()
    else()
        # Linux/Mac (GCC/Clang)
        if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "amd64")
            message(STATUS "Enabling AVX2 instructions for GCC/Clang")
            add_compile_options(-mavx2 -mfma)
        elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
            message(STATUS "Enabling NEON instructions for ARM64")
        endif()
    endif()
endif()

option(ENABLE_PROMETHEUS "Enable Prometheus Metrics" ON)
if(ENABLE_PROMETHEUS)
    find_package(prometheus-cpp CONFIG)
    if(NOT prometheus-cpp_FOUND)
        message(WARNING "prometheus-cpp not found. Disabling metrics.")
        set(ENABLE_PROMETHEUS OFF)
    endif()
endif()
find_package(pybind11 CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# --- GPU Acceleration (CUDA) ---
option(ENABLE_CUDA "Enable CUDA GPU Acceleration" ON)

if(ENABLE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        set(CMAKE_CUDA_ARCHITECTURES "75")
        find_package(CUDAToolkit REQUIRED)
        add_compile_definitions(ENABLE_CUDA)
        message(STATUS "CUDA Detected. GPU Acceleration Enabled.")
    else()
        message(STATUS "CUDA Not Found. GPU Acceleration Disabled.")
    endif()
endif()

# --- Metal (MacOS) ---
if(APPLE)
    enable_language(OBJCXX)
    find_library(METAL_LIB Metal REQUIRED)
    find_library(FOUNDATION_LIB Foundation REQUIRED)
    
    option(SKIP_METAL_SHADERS "Skip Metal shader compilation (use precompiled or fallback)" OFF)
    
    if(NOT SKIP_METAL_SHADERS)
        # Metal Shader Compilation
        set(METAL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/kernels/metal/gate_kernels.metal)
        set(METAL_LIB_OUT ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)
        
        add_custom_command(
            OUTPUT ${METAL_LIB_OUT}
            COMMAND xcrun -sdk macosx metal -c ${METAL_SRC} -o ${CMAKE_CURRENT_BINARY_DIR}/shaders.air
            COMMAND xcrun -sdk macosx metallib ${CMAKE_CURRENT_BINARY_DIR}/shaders.air -o ${METAL_LIB_OUT}
            DEPENDS ${METAL_SRC}
            COMMENT "Compiling Metal Shaders"
        )
        add_custom_target(CompileMetal ALL DEPENDS ${METAL_LIB_OUT})
    else()
        message(STATUS "Skipping Metal shader compilation (SKIP_METAL_SHADERS=ON)")
    endif()
    
    add_compile_definitions(ENABLE_METAL)
    set(METAL_SOURCES src/backends/metal/MetalContext.mm)
else()
    set(METAL_SOURCES "")
endif()

# --- Protobuf / gRPC Code Generation ---
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto/quantum.proto)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

protobuf_generate_cpp(PROTO_SRC_GENERATED_CPP PROTO_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)

function(grpc_generate_cpp srcs hdrs)
    cmake_parse_arguments(grpc "" "OUTPUT_DIRECTORY;IMPORT_DIRS" "" ${ARGN})
    set(_proto_files "${grpc_UNPARSED_ARGUMENTS}")
    if(NOT _proto_files)
        message(SEND_ERROR "Error: grpc_generate_cpp() called without any proto files")
        return()
    endif()

    if(grpc_OUTPUT_DIRECTORY)
        set(_g_output_dir "${grpc_OUTPUT_DIRECTORY}")
    else()
        set(_g_output_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    set(_g_import_dirs)
    if(grpc_IMPORT_DIRS)
        get_filename_component(_abs_import "${grpc_IMPORT_DIRS}" REALPATH)
        list(APPEND _g_import_dirs "-I${_abs_import}")
    else()
        list(APPEND _g_import_dirs "-I${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(${srcs})
    set(${hdrs})
    
    if(TARGET gRPC::grpc_cpp_plugin)
        get_target_property(gRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
        if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
            get_target_property(gRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
        endif()
    endif()

    if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
        find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin DOC "gRPC C++ plugin")
    endif()

    if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
         message(FATAL_ERROR "Could not locate gRPC::grpc_cpp_plugin executable")
    endif()

    foreach(fil ${_proto_files})
        get_filename_component(abs_fil ${fil} REALPATH)
        get_filename_component(fil_we ${fil} NAME_WE)

        list(APPEND ${srcs} "${_g_output_dir}/${fil_we}.grpc.pb.cc")
        list(APPEND ${hdrs} "${_g_output_dir}/${fil_we}.grpc.pb.h")

        add_custom_command(
            OUTPUT "${_g_output_dir}/${fil_we}.grpc.pb.cc"
                   "${_g_output_dir}/${fil_we}.grpc.pb.h"
            COMMAND protobuf::protoc
            ARGS --grpc_out=${_g_output_dir}
                 --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
                 ${_g_import_dirs}
                 ${abs_fil}
            DEPENDS "${abs_fil}" protobuf::protoc gRPC::grpc_cpp_plugin
            COMMENT "Running gRPC C++ compiler on ${fil} (Import: ${_g_import_dirs})"
            VERBATIM )
    endforeach()

    set_source_files_properties(${${srcs}} ${${hdrs}} PROPERTIES GENERATED TRUE)
    set(${srcs} ${${srcs}} PARENT_SCOPE)
    set(${hdrs} ${${hdrs}} PARENT_SCOPE)
endfunction()

grpc_generate_cpp(GRPC_SRC_GENERATED_CPP GRPC_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)

# --- GPU Sources ---
set(GPU_SOURCES "")
if(ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
    list(APPEND GPU_SOURCES src/kernels/gate_kernels.cu)
    list(APPEND GPU_SOURCES src/backends/CudaBackend.cpp)
endif()

# =================================================================
# Core Library — shared across all targets to eliminate duplication
# =================================================================
add_library(qubit_engine_core STATIC
    src/QuantumRegister.cpp
    src/backends/CpuBackend.cpp
    src/backends/GPUQuantumRegister.cpp
    src/CircuitOptimizer.cpp
    src/QuantumDifferentiator.cpp
    src/QuantumJIT.cpp
    src/OpenQASM.cpp
    ${METAL_SOURCES}
    ${GPU_SOURCES}
)

target_include_directories(qubit_engine_core PUBLIC src)
set_target_properties(qubit_engine_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Platform-specific link libraries for core
if(APPLE)
    target_link_libraries(qubit_engine_core PUBLIC ${METAL_LIB} ${FOUNDATION_LIB})
endif()

if(ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
    target_link_libraries(qubit_engine_core PUBLIC CUDA::cudart)
endif()

if(MPI_FOUND)
    target_link_libraries(qubit_engine_core PUBLIC MPI::MPI_CXX)
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(qubit_engine_core PUBLIC OpenMP::OpenMP_CXX)
endif()

target_link_libraries(qubit_engine_core PUBLIC Threads::Threads)

# =================================================================
# Main Executable
# =================================================================
add_executable(qubit_engine
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    src/main.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)

target_include_directories(qubit_engine PUBLIC
    src
    ${GENERATED_DIR} 
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(qubit_engine
    PRIVATE
    qubit_engine_core
    gRPC::grpc++
    gRPC::grpc
    gRPC::gpr
    gRPC::grpc++_reflection
    protobuf::libprotobuf 
    gflags
    glog::glog 
    OpenSSL::SSL
    OpenSSL::Crypto
    $<$<BOOL:${UNIX}>:dl>
    $<$<BOOL:${ENABLE_PROMETHEUS}>:prometheus-cpp::pull>
    $<$<BOOL:${ENABLE_PROMETHEUS}>:prometheus-cpp::core>
    spdlog::spdlog
)

# =================================================================
# Python Bindings
# =================================================================
pybind11_add_module(qubit_engine_module MODULE
    src/python_bindings.cpp
)
set_target_properties(qubit_engine_module PROPERTIES OUTPUT_NAME "core")

target_include_directories(qubit_engine_module PRIVATE src)
target_link_libraries(qubit_engine_module PRIVATE qubit_engine_core)

# =================================================================
# Testing
# =================================================================
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(tests
    tests/QuantumTests.cpp
    tests/ServiceTests.cpp
    tests/CudaTests.cpp
    tests/JITTests.cpp
    tests/OpenQASMTests.cpp
    tests/DifferentiatorTests.cpp
    tests/OptimizerTests.cpp
    tests/CircuitOptimizerTests.cpp
    tests/AdjointTests.cpp
    tests/CpuBackendTests.cpp
    tests/MockHardwareBackendTests.cpp
    tests/GPUQuantumRegisterTests.cpp
)

# Service tests need gRPC generated headers + ServiceImpl sources
target_sources(tests PRIVATE
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)

target_include_directories(tests PRIVATE src ${GENERATED_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(tests PRIVATE
    qubit_engine_core
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    $<$<BOOL:${UNIX}>:dl>
    $<$<BOOL:${ENABLE_PROMETHEUS}>:prometheus-cpp::pull>
    $<$<BOOL:${ENABLE_PROMETHEUS}>:prometheus-cpp::core>
    spdlog::spdlog
)

add_test(NAME CoreTests COMMAND tests)

# =================================================================
# Benchmark
# =================================================================
add_executable(benchmark tests/Benchmark.cpp)

target_sources(benchmark PRIVATE
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)

target_include_directories(benchmark PRIVATE src ${GENERATED_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(benchmark PRIVATE
    qubit_engine_core
    gRPC::grpc++
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    $<$<BOOL:${UNIX}>:dl>
    $<$<BOOL:${ENABLE_PROMETHEUS}>:prometheus-cpp::pull>
    $<$<BOOL:${ENABLE_PROMETHEUS}>:prometheus-cpp::core>
    spdlog::spdlog
)

# =================================================================
# MPI Verification Executable
# =================================================================
if(MPI_FOUND)
    add_executable(mpi_test mpi_test.cpp)

    target_sources(mpi_test PRIVATE
        ${PROTO_SRC_GENERATED_CPP}
        ${GRPC_SRC_GENERATED_CPP}
    )

    target_include_directories(mpi_test PRIVATE src ${GENERATED_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(mpi_test PRIVATE
        qubit_engine_core
        gRPC::grpc++
        protobuf::libprotobuf
        OpenSSL::SSL
        OpenSSL::Crypto
        $<$<BOOL:${UNIX}>:dl>
    )
endif()
