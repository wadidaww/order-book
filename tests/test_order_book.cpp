#include <orderbook.hpp>
#include "test_framework.hpp"

using namespace orderbook;
using namespace test;

TEST(add_order) {
    OrderBook book;
    bool result = book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

TEST(duplicate_order_id) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    bool result = book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_FALSE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

TEST(best_bid_ask) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 99'0000, 100, Side::Buy);
    book.addOrder(3, 101'0000, 100, Side::Sell);
    book.addOrder(4, 102'0000, 100, Side::Sell);

    auto bestBid = book.bestBid();
    auto bestAsk = book.bestAsk();

    ASSERT_TRUE(bestBid.has_value());
    ASSERT_TRUE(bestAsk.has_value());
    ASSERT_EQ(*bestBid, 100'0000);
    ASSERT_EQ(*bestAsk, 101'0000);
}

TEST(spread) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 101'0000, 100, Side::Sell);

    auto spread = book.spread();
    ASSERT_TRUE(spread.has_value());
    ASSERT_EQ(*spread, 1'0000);
}

TEST(mid_price) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 102'0000, 100, Side::Sell);

    auto mid = book.midPrice();
    ASSERT_TRUE(mid.has_value());
    ASSERT_EQ(*mid, 101'0000);
}

TEST(cancel_order) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_EQ(book.orderCount(), 1);

    bool result = book.cancelOrder(1);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 0);
}

TEST(cancel_nonexistent_order) {
    OrderBook book;
    bool result = book.cancelOrder(999);
    ASSERT_FALSE(result);
}

TEST(get_order) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);

    auto order = book.getOrder(1);
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->id, 1);
    ASSERT_EQ(order->price, 100'0000);
    ASSERT_EQ(order->quantity, 100);
    ASSERT_EQ(order->side, Side::Buy);
}

TEST(market_depth) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 99'0000, 200, Side::Buy);
    book.addOrder(3, 98'0000, 300, Side::Buy);

    auto bids = book.getBids(2);
    ASSERT_EQ(bids.size(), 2);
    ASSERT_EQ(bids[0].price, 100'0000);
    ASSERT_EQ(bids[0].quantity, 100);
    ASSERT_EQ(bids[1].price, 99'0000);
    ASSERT_EQ(bids[1].quantity, 200);
}

TEST(volume_at_price) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 100'0000, 150, Side::Buy);

    Quantity vol = book.getVolumeAtPrice(100'0000, Side::Buy);
    ASSERT_EQ(vol, 250);
}

TEST(clear_book) {
    OrderBook book;
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 101'0000, 100, Side::Sell);

    book.clear();
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_FALSE(book.bestBid().has_value());
    ASSERT_FALSE(book.bestAsk().has_value());
}

TEST(price_time_priority) {
    OrderBook book;

    // Add orders at same price in sequence
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 100'0000, 150, Side::Buy);
    book.addOrder(3, 100'0000, 200, Side::Buy);

    auto bids = book.getBids(1);
    ASSERT_EQ(bids.size(), 1);
    ASSERT_EQ(bids[0].quantity, 450);  // All orders at same level
    ASSERT_EQ(bids[0].orderCount, 3);
}

int main() {
    return TestSuite::instance().run();
}
