cmake_minimum_required(VERSION 3.20)

# Simple example
add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example orderbook)

# Market maker example
add_executable(market_maker_example market_maker_example.cpp)
target_link_libraries(market_maker_example orderbook)

# Arbitrage example
add_executable(arbitrage_example arbitrage_example.cpp)
target_link_libraries(arbitrage_example orderbook)

# Performance benchmark
add_executable(performance_benchmark performance_benchmark.cpp)
target_link_libraries(performance_benchmark orderbook)
