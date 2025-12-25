cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 17)

if(APPLE)
    enable_language(OBJCXX)
    # Enable optimizations for Apple Silicon M3
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mcpu=apple-m3 -mtune=apple-m3")
    
    find_library(METAL_LIB Metal)
    find_library(FOUNDATION_LIB Foundation)
    
    find_library(METAL_LIB Metal)
    find_library(FOUNDATION_LIB Foundation)
endif()

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
if(MPI_FOUND)
    add_compile_definitions(MPI_ENABLED)
endif()

find_package(prometheus-cpp CONFIG REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)

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
# Manual generation to ensure output directory is respected
add_custom_command(
    OUTPUT "${GENERATED_DIR}/quantum.pb.cc"
           "${GENERATED_DIR}/quantum.pb.h"
    COMMAND protobuf::protoc
    ARGS --cpp_out=${GENERATED_DIR}
         -I${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} protobuf::protoc
    COMMENT "Running C++ protocol buffer compiler on quantum.proto"
    VERBATIM
)

set(PROTO_SRC_GENERATED_CPP "${GENERATED_DIR}/quantum.pb.cc")
set(PROTO_HDR_GENERATED_CPP "${GENERATED_DIR}/quantum.pb.h")

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

# --- Physics Kernel (No Heavy Deps) ---
add_library(QubitEnginePhysics
    src/QuantumRegister.cpp
    src/MetalContext.mm
)
target_include_directories(QubitEnginePhysics PUBLIC src)

if(MPI_FOUND)
    target_link_libraries(QubitEnginePhysics PUBLIC MPI::MPI_CXX)
endif()

if(APPLE)
    target_link_libraries(QubitEnginePhysics PUBLIC ${METAL_LIB} ${FOUNDATION_LIB})
endif()

# --- Service/Core Layer (gRPC, Logs) ---
add_library(QubitEngineCore
    src/ServiceImpl.cpp
    src/ServiceImpl_Visualize.cpp
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)

target_include_directories(QubitEngineCore PUBLIC ${GENERATED_DIR})

target_link_libraries(QubitEngineCore
    PUBLIC
    QubitEnginePhysics
    gRPC::grpc++
    gRPC::grpc
    gRPC::gpr
    gRPC::grpc++_reflection
    protobuf::libprotobuf 
    gflags
    glog::glog 
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    prometheus-cpp::pull
    prometheus-cpp::core
)

if(UNIX AND NOT APPLE)
    target_link_libraries(QubitEngineCore PUBLIC dl)
endif()

# --- Main Executable ---
add_executable(qubit_engine src/main.cpp)
target_link_libraries(qubit_engine PRIVATE QubitEngineCore)

# --- Python Bindings ---
pybind11_add_module(qubit_engine_module MODULE
    src/python_bindings.cpp
)
set_target_properties(qubit_engine_module PROPERTIES OUTPUT_NAME "qubit_engine")

target_link_libraries(qubit_engine_module PRIVATE QubitEngineCore)

# --- Testing ---
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(tests tests/QuantumTests.cpp)

target_link_libraries(tests PRIVATE
    QubitEngineCore
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME CoreTests COMMAND tests)

# --- Benchmark ---
add_executable(benchmark src/benchmark.cpp)
target_link_libraries(benchmark PRIVATE QubitEngineCore)
