cmake_minimum_required(VERSION 3.15)
project(Benchmarks)

set(CMAKE_CXX_STANDARD 17)

# --- Dependencies ---
find_package(benchmark REQUIRED)
find_package(OpenMP)
find_package(Threads REQUIRED)

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

# --- Define Dependencies ---
# Since backend is not a library, we compile the necessary sources directly.
set(BACKEND_SOURCES
    ../backend/src/QuantumRegister.cpp
    ../backend/src/backends/CpuBackend.cpp
    # Add dummy/empty GPU backend or exclude ENABLE_CUDA if possible to avoid linking CUDA
    ../backend/src/backends/GPUQuantumRegister.cpp 
)

# Mocking or Handling Missing Symbols if any
# (e.g., if CpuBackend depends on generated protobufs, we might fail. 
# Checking CpuBackend.cpp usage... it uses "CpuBackend.hpp", "QuantumRegister.hpp")
# It seems decoupled from ServiceImpl (gRPC).

add_executable(memory_benchmark MemoryWallBenchmark.cpp ${BACKEND_SOURCES})

target_link_libraries(memory_benchmark
    PRIVATE
    benchmark::benchmark
    Threads::Threads
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(memory_benchmark PRIVATE OpenMP::OpenMP_CXX)
endif()

# MPI stub if valid
option(MPI_ENABLED "Enable MPI" OFF)
if(MPI_ENABLED)
    find_package(MPI REQUIRED)
    target_link_libraries(memory_benchmark PRIVATE MPI::MPI_CXX)
    add_compile_definitions(MPI_ENABLED)
endif()
