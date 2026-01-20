cmake_minimum_required(VERSION 3.15)
project(QubitEngine LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    add_compile_options(-O3 -mavx2 -mfma -fobjc-arc)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    add_compile_options(-O3 -fobjc-arc) # NEON is default on Apple Silicon
endif()

# Bypass gRPC/Protobuf for now to verify Backend compilation
# find_package(Protobuf REQUIRED)
# find_package(gRPC REQUIRED)
# find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 1. Include Directories
include_directories(backend/src)
# include_directories(backend/src/generated)

# 2. Find Sources
# Include .mm for Metal support
file(GLOB_RECURSE ENGINE_SRCS "backend/src/*.cpp" "backend/src/*.mm")
# Exclude main.cpp and other service/binding files that have external dependencies
list(FILTER ENGINE_SRCS EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER ENGINE_SRCS EXCLUDE REGEX ".*python_bindings\\.cpp$")
list(FILTER ENGINE_SRCS EXCLUDE REGEX ".*ServiceImpl.*\\.cpp$")

add_library(qubit_backend SHARED ${ENGINE_SRCS})

target_link_libraries(qubit_backend
    PRIVATE
    Threads::Threads
    "-framework Metal"
    "-framework Foundation"
    "-framework CoreGraphics"
)

# 4. Metal Backend verification executable
add_executable(verify_metal backend/src/verify_metal_main.cpp)
target_link_libraries(verify_metal 
    qubit_backend
    "-framework Metal"
    "-framework Foundation" 
)
# 5. Circuit Optimizer Test
add_executable(test_optimizer backend/src/CircuitOptimizerTest.cpp)
target_link_libraries(test_optimizer qubit_backend)
# 4. Metal Shaders Compilation (Apple Only)
if(APPLE)
    set(METAL_SRC "${CMAKE_SOURCE_DIR}/backend/src/shaders.metal")
    set(METALLIB_OUT "${CMAKE_BINARY_DIR}/default.metallib")
    
    add_custom_command(
        OUTPUT ${METALLIB_OUT}
        COMMAND xcrun -sdk macosx metal -c ${METAL_SRC} -o ${CMAKE_BINARY_DIR}/shaders.air
        COMMAND xcrun -sdk macosx metallib ${CMAKE_BINARY_DIR}/shaders.air -o ${METALLIB_OUT}
        DEPENDS ${METAL_SRC}
        COMMENT "Compiling Metal Shaders"
    )
    
    # Add dependency to main library to ensure shaders are built
    add_custom_target(metal_shaders ALL DEPENDS ${METALLIB_OUT})
    add_dependencies(qubit_backend metal_shaders)
endif()
