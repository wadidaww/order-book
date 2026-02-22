#include <orderbook.hpp>
#include "test_framework.hpp"

using namespace orderbook;
using namespace test;

TEST(add_order) {
    OrderBook book;
    bool result = book.add_order(1, 100'0000, 100, Side::Buy);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.order_count(), 1);
}

TEST(duplicate_order_id) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    bool result = book.add_order(1, 100'0000, 100, Side::Buy);
    ASSERT_FALSE(result);
    ASSERT_EQ(book.order_count(), 1);
}

TEST(best_bid_ask) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 99'0000, 100, Side::Buy);
    book.add_order(3, 101'0000, 100, Side::Sell);
    book.add_order(4, 102'0000, 100, Side::Sell);
    
    auto best_bid = book.best_bid();
    auto best_ask = book.best_ask();
    
    ASSERT_TRUE(best_bid.has_value());
    ASSERT_TRUE(best_ask.has_value());
    ASSERT_EQ(*best_bid, 100'0000);
    ASSERT_EQ(*best_ask, 101'0000);
}

TEST(spread) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 101'0000, 100, Side::Sell);
    
    auto spread = book.spread();
    ASSERT_TRUE(spread.has_value());
    ASSERT_EQ(*spread, 1'0000);
}

TEST(mid_price) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 102'0000, 100, Side::Sell);
    
    auto mid = book.mid_price();
    ASSERT_TRUE(mid.has_value());
    ASSERT_EQ(*mid, 101'0000);
}

TEST(cancel_order) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    ASSERT_EQ(book.order_count(), 1);
    
    bool result = book.cancel_order(1);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.order_count(), 0);
}

TEST(cancel_nonexistent_order) {
    OrderBook book;
    bool result = book.cancel_order(999);
    ASSERT_FALSE(result);
}

TEST(get_order) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    
    auto order = book.get_order(1);
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->id, 1);
    ASSERT_EQ(order->price, 100'0000);
    ASSERT_EQ(order->quantity, 100);
    ASSERT_EQ(order->side, Side::Buy);
}

TEST(market_depth) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 99'0000, 200, Side::Buy);
    book.add_order(3, 98'0000, 300, Side::Buy);
    
    auto bids = book.get_bids(2);
    ASSERT_EQ(bids.size(), 2);
    ASSERT_EQ(bids[0].price, 100'0000);
    ASSERT_EQ(bids[0].quantity, 100);
    ASSERT_EQ(bids[1].price, 99'0000);
    ASSERT_EQ(bids[1].quantity, 200);
}

TEST(volume_at_price) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 100'0000, 150, Side::Buy);
    
    Quantity vol = book.get_volume_at_price(100'0000, Side::Buy);
    ASSERT_EQ(vol, 250);
}

TEST(clear_book) {
    OrderBook book;
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 101'0000, 100, Side::Sell);
    
    book.clear();
    ASSERT_EQ(book.order_count(), 0);
    ASSERT_FALSE(book.best_bid().has_value());
    ASSERT_FALSE(book.best_ask().has_value());
}

TEST(price_time_priority) {
    OrderBook book;
    
    // Add orders at same price in sequence
    book.add_order(1, 100'0000, 100, Side::Buy);
    book.add_order(2, 100'0000, 150, Side::Buy);
    book.add_order(3, 100'0000, 200, Side::Buy);
    
    auto bids = book.get_bids(1);
    ASSERT_EQ(bids.size(), 1);
    ASSERT_EQ(bids[0].quantity, 450);  // All orders at same level
    ASSERT_EQ(bids[0].order_count, 3);
}

int main() {
    return TestSuite::instance().run();
}
