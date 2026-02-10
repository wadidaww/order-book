cmake_minimum_required(VERSION 3.20)

# Simple test framework
add_executable(test_order_book 
    test_order_book.cpp
    test_matching.cpp
    test_analytics.cpp
)
target_link_libraries(test_order_book PRIVATE orderbook)

add_test(NAME OrderBookTests COMMAND test_order_book)
