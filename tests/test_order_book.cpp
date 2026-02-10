#include <orderbook.hpp>
#include <cassert>
#include <iostream>

using namespace orderbook;

void test_basic_order_addition() {
    std::cout << "Test: Basic Order Addition... ";
    
    OrderBook book(1);
    
    assert(book.add_order(1, Side::Buy, 10000, 100));
    assert(book.add_order(2, Side::Sell, 10100, 100));
    
    auto bid = book.best_bid();
    auto ask = book.best_ask();
    
    assert(bid.has_value());
    assert(ask.has_value());
    assert(*bid == 10000);
    assert(*ask == 10100);
    
    std::cout << "PASSED" << std::endl;
}

void test_order_matching() {
    std::cout << "Test: Order Matching... ";
    
    size_t trade_count = 0;
    auto trade_callback = [&trade_count](const Trade& trade) {
        ++trade_count;
        assert(trade.quantity > 0);
    };
    
    OrderBook book(1, trade_callback);
    
    // Add resting sell order
    book.add_order(1, Side::Sell, 10000, 100);
    
    // Add aggressive buy order that matches
    book.add_order(2, Side::Buy, 10000, 50);
    
    assert(trade_count == 1);
    
    // Check remaining volume
    auto ask_volume = book.volume_at_price(Side::Sell, 10000);
    assert(ask_volume == 50);
    
    std::cout << "PASSED" << std::endl;
}

void test_order_cancellation() {
    std::cout << "Test: Order Cancellation... ";
    
    OrderBook book(1);
    
    book.add_order(1, Side::Buy, 10000, 100);
    assert(book.best_bid().has_value());
    
    assert(book.cancel_order(1));
    assert(!book.best_bid().has_value());
    
    // Try to cancel again (should fail)
    assert(!book.cancel_order(1));
    
    std::cout << "PASSED" << std::endl;
}

void test_order_modification() {
    std::cout << "Test: Order Modification... ";
    
    OrderBook book(1);
    
    book.add_order(1, Side::Buy, 10000, 100);
    
    // Modify price and quantity
    assert(book.modify_order(1, 10100, 150));
    
    auto bid = book.best_bid();
    assert(bid.has_value());
    assert(*bid == 10100);
    
    auto volume = book.volume_at_price(Side::Buy, 10100);
    assert(volume == 150);
    
    std::cout << "PASSED" << std::endl;
}

void test_price_time_priority() {
    std::cout << "Test: Price-Time Priority... ";
    
    std::vector<Trade> trades;
    auto trade_callback = [&trades](const Trade& trade) {
        trades.push_back(trade);
    };
    
    OrderBook book(1, trade_callback);
    
    // Add three sell orders at same price
    book.add_order(1, Side::Sell, 10000, 100);
    book.add_order(2, Side::Sell, 10000, 100);
    book.add_order(3, Side::Sell, 10000, 100);
    
    // Add buy order that matches all three
    book.add_order(4, Side::Buy, 10000, 250);
    
    // Should match in order: 1, 2, then partial of 3
    assert(trades.size() == 3);
    assert(trades[0].seller_order_id == 1);
    assert(trades[1].seller_order_id == 2);
    assert(trades[2].seller_order_id == 3);
    assert(trades[2].quantity == 50);
    
    std::cout << "PASSED" << std::endl;
}

void test_spread_and_mid_price() {
    std::cout << "Test: Spread and Mid Price... ";
    
    OrderBook book(1);
    
    book.add_order(1, Side::Buy, 9900, 100);
    book.add_order(2, Side::Sell, 10100, 100);
    
    auto spread = book.spread();
    auto mid = book.mid_price();
    
    assert(spread.has_value());
    assert(mid.has_value());
    assert(*spread == 200);
    assert(*mid == 10000);
    
    std::cout << "PASSED" << std::endl;
}

void test_market_depth() {
    std::cout << "Test: Market Depth... ";
    
    OrderBook book(1);
    
    // Add multiple bid levels
    book.add_order(1, Side::Buy, 10000, 100);
    book.add_order(2, Side::Buy, 9900, 150);
    book.add_order(3, Side::Buy, 9800, 200);
    
    // Add multiple ask levels
    book.add_order(4, Side::Sell, 10100, 100);
    book.add_order(5, Side::Sell, 10200, 150);
    book.add_order(6, Side::Sell, 10300, 200);
    
    auto [bid_depth, ask_depth] = book.depth();
    assert(bid_depth == 3);
    assert(ask_depth == 3);
    
    auto [bid_levels, ask_levels] = book.get_depth(2);
    assert(bid_levels.size() == 2);
    assert(ask_levels.size() == 2);
    assert(bid_levels[0].price == 10000);
    assert(ask_levels[0].price == 10100);
    
    std::cout << "PASSED" << std::endl;
}

void test_ioc_order() {
    std::cout << "Test: Immediate-Or-Cancel Order... ";
    
    size_t trade_count = 0;
    auto trade_callback = [&trade_count](const Trade&) { ++trade_count; };
    
    OrderBook book(1, trade_callback);
    
    // Add resting sell order
    book.add_order(1, Side::Sell, 10000, 100);
    
    // Add IOC buy order for more than available
    book.add_order(2, Side::Buy, 10000, 150, OrderType::Limit, TimeInForce::ImmediateOrCancel);
    
    // Should match 100, cancel remaining 50
    assert(trade_count == 1);
    assert(!book.best_bid().has_value());
    
    std::cout << "PASSED" << std::endl;
}

void test_duplicate_order_id() {
    std::cout << "Test: Duplicate Order ID... ";
    
    OrderBook book(1);
    
    assert(book.add_order(1, Side::Buy, 10000, 100));
    assert(!book.add_order(1, Side::Sell, 10100, 100));
    
    std::cout << "PASSED" << std::endl;
}

void test_empty_book_queries() {
    std::cout << "Test: Empty Book Queries... ";
    
    OrderBook book(1);
    
    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());
    assert(!book.spread().has_value());
    assert(!book.mid_price().has_value());
    
    auto [bid_depth, ask_depth] = book.depth();
    assert(bid_depth == 0);
    assert(ask_depth == 0);
    
    std::cout << "PASSED" << std::endl;
}

void run_order_book_tests() {
    std::cout << "=== Order Book Tests ===" << std::endl;
    
    test_basic_order_addition();
    test_order_matching();
    test_order_cancellation();
    test_order_modification();
    test_price_time_priority();
    test_spread_and_mid_price();
    test_market_depth();
    test_ioc_order();
    test_duplicate_order_id();
    test_empty_book_queries();
    
    std::cout << "All order book tests passed!" << std::endl;
    std::cout << std::endl;
}
