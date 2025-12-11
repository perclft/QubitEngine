cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

#set(PROTO_FILE ${CMAKE_SOURCE_DIR}/api/proto/quantum.proto)
# The output must be within the CMAKE_CURRENT_BINARY_DIR (build folder) or a source folder.
# Since your source files are outside the build folder, using a dedicated source directory is clean.
set(PROTO_OUT ${CMAKE_SOURCE_DIR}/backend/src/generated)

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
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)

# Generate C++ gRPC files
grpc_generate_cpp(GRPC_SRC_GENERATED_CPP GRPC_HDR_GENERATED_CPP
    ${PROTO_FILE}
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/api/proto
    OUTPUT_DIRECTORY ${GENERATED_DIR}
)
# -----------------------------------------------------------------

# 2. Include Directories
# The C++ compiler needs to find the generated headers.
# Generated headers MUST be found in the directory *before* the package name.
target_include_directories(qubit_engine PUBLIC
    # 1. Source code includes (e.g., backend/src/ServiceImpl.hpp)
    backend/src
    # 2. Generated headers are here: <GENERATED_DIR>/qubit_engine/quantum.grpc.pb.h
    ${GENERATED_DIR} 
)

# 3. Define Executable with ALL Sources (CRITICAL FIX)
# We must include the generated C++ source files (.cc)
add_executable(qubit_engine 
    backend/src/QuantumRegister.cpp
    backend/src/ServiceImpl.cpp
    backend/src/main.cpp
    
    # Generated sources (.cc) must be included here!
    ${PROTO_SRC_GENERATED_CPP}
    ${GRPC_SRC_GENERATED_CPP}
)

# 4. Link Libraries (Existing)
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
