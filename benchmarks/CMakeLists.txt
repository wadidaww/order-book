cmake_minimum_required(VERSION 3.20)

# Single-threaded benchmarks live in examples/performance_benchmark.cpp.

# Multi-producer / reader benchmark for ConcurrentOrderBook
add_executable(concurrent_benchmark concurrent_benchmark.cpp)
target_link_libraries(concurrent_benchmark PRIVATE orderbook)
