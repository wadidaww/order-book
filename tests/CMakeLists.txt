cmake_minimum_required(VERSION 3.20)

# Create separate test executables
add_executable(test_order_book test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE orderbook)

add_executable(test_matching test_matching.cpp)
target_link_libraries(test_matching PRIVATE orderbook)

add_executable(test_analytics test_analytics.cpp)
target_link_libraries(test_analytics PRIVATE orderbook)

add_executable(test_concurrency test_concurrency.cpp)
target_link_libraries(test_concurrency PRIVATE orderbook)

add_executable(test_performance test_performance.cpp)
target_link_libraries(test_performance PRIVATE orderbook)

# Add tests
add_test(NAME OrderBookTests COMMAND test_order_book)
add_test(NAME MatchingTests COMMAND test_matching)
add_test(NAME AnalyticsTests COMMAND test_analytics)
add_test(NAME ConcurrencyTests COMMAND test_concurrency)
add_test(NAME PerformanceTests COMMAND test_performance)
