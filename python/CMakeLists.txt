cmake_minimum_required(VERSION 3.15)
project(qubit_engine_python)

set(CMAKE_CXX_STANDARD 17)

# Locate Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Fetch Pybind11
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Include backend headers
include_directories(${CMAKE_SOURCE_DIR}/../backend/src)

# Define the module
pybind11_add_module(qubit_engine src/bindings.cpp)

# Link against the backend objects (re-compiling them here for simplicity to avoid shared lib complexities cross-platform)
# Alternatively, we could link against a shared lib if backend exported one.
# For now, we'll confirm 'backend' sources.

set(BACKEND_SOURCES
    ../backend/src/QuantumRegister.cpp
    ../backend/src/backends/CpuBackend.cpp
    ../backend/src/backends/GPUQuantumRegister.cpp
)

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND BACKEND_SOURCES ../backend/src/backends/MetalBackend.mm)
    find_library(METAL_LIB Metal REQUIRED)
    find_library(FOUNDATION_LIB Foundation REQUIRED)
    target_link_libraries(qubit_engine PRIVATE ${METAL_LIB} ${FOUNDATION_LIB})
    target_compile_definitions(qubit_engine PRIVATE ENABLE_METAL)
endif()

# Compiler flags for optimizations
if(MSVC)
    target_compile_options(qubit_engine PRIVATE /arch:AVX2)
else()
    if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "amd64")
        target_compile_options(qubit_engine PRIVATE -mavx2 -mfma)
    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
        # target_compile_options(qubit_engine PRIVATE -march=armv8-a+simd)
    endif()
endif()

target_sources(qubit_engine PRIVATE ${BACKEND_SOURCES})

# Hardware Acceleration options matched to backend
option(ENABLE_HW_ACCEL "Enable Hardware Acceleration" ON)
if(ENABLE_HW_ACCEL)
    target_compile_definitions(qubit_engine PRIVATE ENABLE_HW_ACCEL)
endif()

# Ensure we use the correct Python libraries
target_link_libraries(qubit_engine PRIVATE Python3::Python)
