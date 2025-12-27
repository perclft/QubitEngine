cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
option(MPI_ENABLED "Enable MPI Distributed Computing" OFF)
if(MPI_ENABLED)
    find_package(MPI REQUIRED)
    if(MPI_FOUND)
        add_compile_definitions(MPI_ENABLED)
        include_directories(${MPI_CXX_INCLUDE_PATH})
        link_libraries(${MPI_CXX_LIBRARIES})
    endif()
endif()

find_package(prometheus-cpp CONFIG REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)

# --- GPU Acceleration (CUDA) ---
option(ENABLE_CUDA "Enable CUDA GPU Acceleration" ON)

if(ENABLE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        find_package(CUDAToolkit REQUIRED)
        add_compile_definitions(ENABLE_CUDA)
        message(STATUS "CUDA Detected. GPU Acceleration Enabled.")
    else()
        message(STATUS "CUDA Not Found. GPU Acceleration Disabled.")
    endif()
endif()

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto/quantum.proto)
# The output must be within the CMAKE_CURRENT_BINARY_DIR (build folder) or a source folder.
# Since your source files are outside the build folder, using a dedicated source directory is clean.
set(PROTO_OUT ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)

# 1. Generate C++ Protobuf files (e.g., quantum.pb.h/cc)
# NOTE: The DESTINATION parameter here is incorrect and should NOT be used with CMAKE_SOURCE_DIR
# if you want the generated files to be added to the source lists correctly.
# We will use the standard method where CMake generates them into the BUILD directory
# and then add that directory to the include path.

# Define the location where the generated files will land inside the build folder
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR}) # Ensure the directory exists

# Generate files into the build directory's generated subdirectory
protobuf_generate_cpp(PROTO_SRC_GENERATED_CPP PROTO_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)

# Define helper function for generating gRPC files if not present
# Define helper function for generating gRPC files if not present
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
        # Resolve canonical path to fix protoc matching issues
        get_filename_component(_abs_import "${grpc_IMPORT_DIRS}" REALPATH)
        list(APPEND _g_import_dirs "-I${_abs_import}")
    else()
        list(APPEND _g_import_dirs "-I${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(${srcs})
    set(${hdrs})
    
    # Locate the gRPC C++ plugin
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

# Generate C++ gRPC files
grpc_generate_cpp(GRPC_SRC_GENERATED_CPP GRPC_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)
# -----------------------------------------------------------------

# 2. Include Directories
# The C++ compiler needs to find the generated headers.
# Generated headers MUST be found in the directory *before* the package name.
# 3. Define Executable with ALL Sources (CRITICAL FIX)
# We must include the generated C++ source files (.cc)
# --- GPU Sources ---
set(GPU_SOURCES "")
if(ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
    list(APPEND GPU_SOURCES src/kernels/gate_kernels.cu)
endif()

add_executable(qubit_engine 
    src/QuantumRegister.cpp
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    src/backends/GPUQuantumRegister.cpp
    src/main.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
    ${GPU_SOURCES}
)

target_include_directories(qubit_engine PUBLIC
    src
    ${GENERATED_DIR} 
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(qubit_engine
    PRIVATE
    gRPC::grpc++
    gRPC::grpc
    gRPC::gpr
    gRPC::grpc++_reflection
    protobuf::libprotobuf 
    gflags::gflags
    glog::glog 
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    dl
    prometheus-cpp::pull
    prometheus-cpp::core
)

if(ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
   target_link_libraries(qubit_engine PRIVATE CUDA::cudart)
endif()

if(MPI_FOUND)
    target_link_libraries(qubit_engine PRIVATE MPI::MPI_CXX)
endif()

# --- Python Bindings ---
pybind11_add_module(qubit_engine_module MODULE
    src/python_bindings.cpp
    src/QuantumRegister.cpp
    src/backends/GPUQuantumRegister.cpp
    ${GPU_SOURCES}
)
set_target_properties(qubit_engine_module PROPERTIES OUTPUT_NAME "qubit_engine")

target_include_directories(qubit_engine_module PRIVATE src)

if(ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
    target_link_libraries(qubit_engine_module PRIVATE CUDA::cudart)
endif()

# Ensure MPI if needed
if(MPI_FOUND)
    target_link_libraries(qubit_engine_module PRIVATE MPI::MPI_CXX)
endif()

# --- Testing ---
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(tests tests/QuantumTests.cpp)

# Link QubitEngine sources (excluding main.cpp) to tests or link object lib?
# Easiest is to add sources to tests executable for now, or make a library.
# Given main.cpp has main(), we can't link qubit_engine executable.
# We should probably define a library for core logic.

# Solution: Add core sources to tests target.
target_sources(tests PRIVATE
    src/QuantumRegister.cpp
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)

target_include_directories(tests PRIVATE src ${GENERATED_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    dl
    prometheus-cpp::pull
    prometheus-cpp::core
)

if(MPI_FOUND)
    target_link_libraries(tests PRIVATE MPI::MPI_CXX)
endif()

add_test(NAME CoreTests COMMAND tests)

add_executable(benchmark tests/Benchmark.cpp)
target_sources(benchmark PRIVATE
    src/QuantumRegister.cpp
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)
target_include_directories(benchmark PRIVATE src ${GENERATED_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(benchmark PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    dl
    prometheus-cpp::pull
    prometheus-cpp::core
)
if(MPI_FOUND)
    target_link_libraries(benchmark PRIVATE MPI::MPI_CXX)
endif()

# --- MPI Verification Executable ---
if(MPI_FOUND)
    add_executable(mpi_test mpi_test.cpp)
    target_sources(mpi_test PRIVATE
        src/QuantumRegister.cpp
        ${PROTO_SRC_GENERATED_CPP}
        ${GRPC_SRC_GENERATED_CPP}
    )
    target_include_directories(mpi_test PRIVATE src ${GENERATED_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(mpi_test PRIVATE
        MPI::MPI_CXX
        gRPC::grpc++
        protobuf::libprotobuf
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        dl
    )
endif()
