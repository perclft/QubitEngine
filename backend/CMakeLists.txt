cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
if(MPI_FOUND)
    add_compile_definitions(MPI_ENABLED)
endif()

# Ensure M_PI and other constants are defined on MSVC
if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

find_package(prometheus-cpp CONFIG REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto/quantum.proto)
set(PROTO_OUT ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)

# Define the location where the generated files will land inside the build folder
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generate files into the build directory's generated subdirectory
protobuf_generate_cpp(PROTO_SRC_GENERATED_CPP PROTO_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)

# Helper function to generate gRPC C++ code
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

# Generate C++ gRPC files
grpc_generate_cpp(GRPC_SRC_GENERATED_CPP GRPC_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)

add_executable(qubit_engine 
    src/QuantumRegister.cpp
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
    gRPC::grpc++
    gRPC::grpc
    gRPC::gpr
    gRPC::grpc++_reflection
    protobuf::libprotobuf 
    gflags
    glog 
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    prometheus-cpp::pull
    prometheus-cpp::core
)

if(WIN32)
    target_link_libraries(qubit_engine PRIVATE ws2_32)
else()
    target_link_libraries(qubit_engine PRIVATE dl)
endif()

if(MPI_FOUND)
    target_link_libraries(qubit_engine PRIVATE MPI::MPI_CXX)
endif()

# --- Python Bindings ---
pybind11_add_module(qubit_engine_module MODULE
    src/python_bindings.cpp
    src/QuantumRegister.cpp
)
set_target_properties(qubit_engine_module PROPERTIES OUTPUT_NAME "qubit_engine")

target_include_directories(qubit_engine_module PRIVATE src)

if(MPI_FOUND)
    target_link_libraries(qubit_engine_module PRIVATE MPI::MPI_CXX)
endif()

# --- Testing ---
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(tests tests/QuantumTests.cpp)

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
    prometheus-cpp::pull
    prometheus-cpp::core
)

if(WIN32)
    target_link_libraries(tests PRIVATE ws2_32)
else()
    target_link_libraries(tests PRIVATE dl)
endif()

if(MPI_FOUND)
    target_link_libraries(tests PRIVATE MPI::MPI_CXX)
endif()

add_test(NAME CoreTests COMMAND tests)
