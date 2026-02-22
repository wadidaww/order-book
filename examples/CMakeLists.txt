cmake_minimum_required(VERSION 3.20)

# Basic example
add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example PRIVATE orderbook)

# Market maker example
add_executable(market_maker_example market_maker_example.cpp)
target_link_libraries(market_maker_example PRIVATE orderbook)

# Arbitrage example
add_executable(arbitrage_example arbitrage_example.cpp)
target_link_libraries(arbitrage_example PRIVATE orderbook)

# Performance benchmark
add_executable(performance_benchmark performance_benchmark.cpp)
target_link_libraries(performance_benchmark PRIVATE orderbook)
