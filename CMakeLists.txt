cmake_minimum_required(VERSION 3.20)
project(OrderBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer options (mutually exclusive)
option(ENABLE_ASAN "Enable AddressSanitizer + UBSanitizer + LeakSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ENABLE_ASAN and ENABLE_TSAN are mutually exclusive")
endif()

# Compiler flags for warnings (always applied)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-result
    )

    # Performance flags — skipped for sanitizer and explicit Debug builds so
    # that instrumentation tools get accurate diagnostics.
    if(NOT ENABLE_ASAN AND NOT ENABLE_TSAN AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-march=native -O3 -DNDEBUG)
    endif()

    # AddressSanitizer + UBSanitizer + LeakSanitizer
    if(ENABLE_ASAN)
        add_compile_options(-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()

    # ThreadSanitizer
    if(ENABLE_TSAN)
        add_compile_options(-g -O1 -fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()

    # Plain Debug build (no sanitizers)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_ASAN AND NOT ENABLE_TSAN)
        add_compile_options(-g -O0)
    endif()
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
