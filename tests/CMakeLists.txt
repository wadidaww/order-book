cmake_minimum_required(VERSION 3.20)

# Unit tests
add_executable(test_order_book 
    test_order_book.cpp
    test_analytics.cpp
    test_main.cpp
)
target_link_libraries(test_order_book orderbook)

# Enable testing
add_test(NAME OrderBookTests COMMAND test_order_book)
