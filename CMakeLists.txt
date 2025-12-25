cmake_minimum_required(VERSION 3.15)
project(QubitEngine)

set(CMAKE_CXX_STANDARD 17)

# Delegate to subdirectories
add_subdirectory(backend)
add_subdirectory(game)
