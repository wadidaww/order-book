cmake_minimum_required(VERSION 3.20)
project(OrderBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate a compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# command for Ninja (not working)
set(CMAKE_GENERATOR Ninja)

# Compiler flags for performance and warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-result
        -march=native
        -O3
        -DNDEBUG
    )
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library
add_library(orderbook INTERFACE)
target_include_directories(orderbook INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Examples
add_subdirectory(examples)

# Tests (optional, enabled by default)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
option(BUILD_BENCHMARKS "Build benchmarks" ON)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Install
install(DIRECTORY include/ DESTINATION include)
install(TARGETS orderbook EXPORT OrderBookTargets)
