cmake_minimum_required(VERSION 3.15)
project(Benchmarks)

set(CMAKE_CXX_STANDARD 17)

# --- Dependencies ---
include(FetchContent)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(googlebenchmark)

find_package(OpenMP)
find_package(Threads REQUIRED)

# --- Metal (MacOS) ---
if(APPLE)
    enable_language(OBJCXX)
    find_library(METAL_LIB Metal REQUIRED)
    find_library(FOUNDATION_LIB Foundation REQUIRED)
    set(METAL_SOURCES ../backend/src/backends/MetalBackend.mm)
    add_compile_definitions(ENABLE_METAL)
else()
    set(METAL_SOURCES "")
endif()

# --- Hardware Acceleration (Copy from Backend) ---
option(ENABLE_HW_ACCEL "Enable Hardware Acceleration (AVX2/NEON)" ON)
if(ENABLE_HW_ACCEL)
    if(MSVC)
        cmake_host_system_information(RESULT HOST_ARCH QUERY OS_PLATFORM)
        if("${HOST_ARCH}" MATCHES "AMD64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64")
             message(STATUS "Enabling AVX2 instructions for MSVC")
             add_compile_options(/arch:AVX2)
        endif()
    else()
        if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "amd64")
            add_compile_options(-mavx2 -mfma)
        elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
            # add_compile_options(-march=armv8-a+simd) 
        endif()
    endif()
endif()

# --- Include Backend Headers ---
include_directories(${CMAKE_SOURCE_DIR}/../backend/src)

if(APPLE)
    # Enable ARC for Metal backend
    add_compile_options(-fobjc-arc)
endif()

# --- Define Dependencies ---
set(BACKEND_SOURCES
    ../backend/src/QuantumRegister.cpp
    ../backend/src/backends/CpuBackend.cpp
    ../backend/src/backends/GPUQuantumRegister.cpp
    ../backend/src/CircuitOptimizer.cpp
    ${METAL_SOURCES}
)

if(ENABLE_CUDA)
    find_package(CUDA REQUIRED)
    list(APPEND BACKEND_SOURCES ../backend/src/backends/CudaBackend.cpp)
endif()

# Benchmark executables
add_executable(memory_benchmark MemoryWallBenchmark.cpp ${BACKEND_SOURCES})
target_link_libraries(memory_benchmark
    PRIVATE
    benchmark::benchmark
    Threads::Threads
)

add_executable(antigravity_memory_benchmark AntigravityMemoryBenchmark.cpp ${BACKEND_SOURCES})
target_link_libraries(antigravity_memory_benchmark
    PRIVATE
    benchmark::benchmark
    Threads::Threads
)

if(APPLE)
    target_link_libraries(memory_benchmark PRIVATE ${METAL_LIB} ${FOUNDATION_LIB})
    target_link_libraries(antigravity_memory_benchmark PRIVATE ${METAL_LIB} ${FOUNDATION_LIB})
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(memory_benchmark PRIVATE OpenMP::OpenMP_CXX)
    target_link_libraries(antigravity_memory_benchmark PRIVATE OpenMP::OpenMP_CXX)
endif()

# MPI stub if valid
option(MPI_ENABLED "Enable MPI" OFF)
if(MPI_ENABLED)
    find_package(MPI REQUIRED)
    target_link_libraries(memory_benchmark PRIVATE MPI::MPI_CXX)
    target_link_libraries(antigravity_memory_benchmark PRIVATE MPI::MPI_CXX)
    add_compile_definitions(MPI_ENABLED)
endif()
