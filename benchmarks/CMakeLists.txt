cmake_minimum_required(VERSION 3.20)

# Latency benchmark
add_executable(latency_benchmark latency_benchmark.cpp)
target_link_libraries(latency_benchmark orderbook)
