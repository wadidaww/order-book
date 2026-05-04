#include <orderbook.hpp>
#include "test_framework.hpp"

using namespace orderbook;
using namespace test;

// Convenience: collar from 90.0000 to 110.0000 with tick = 1.0000
// (prices are scaled by PRICE_SCALE = 10000)
static constexpr Price LOWER = 90'0000;
static constexpr Price UPPER = 110'0000;
static constexpr Price TICK = 1'0000;

// -----------------------------------------------------------------------
// Collar enforcement
// -----------------------------------------------------------------------

TEST(reject_price_below_collar) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.addOrder(1, LOWER - TICK, 100, Side::Buy);
    ASSERT_FALSE(result);
    ASSERT_EQ(book.orderCount(), 0);
}

TEST(reject_price_above_collar) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.addOrder(1, UPPER + TICK, 100, Side::Sell);
    ASSERT_FALSE(result);
    ASSERT_EQ(book.orderCount(), 0);
}

TEST(accept_price_at_lower_bound) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.addOrder(1, LOWER, 100, Side::Buy);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

TEST(accept_price_at_upper_bound) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.addOrder(1, UPPER, 100, Side::Sell);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

TEST(accept_price_within_collar) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

// -----------------------------------------------------------------------
// Basic operations
// -----------------------------------------------------------------------

TEST(collar_add_order) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

TEST(collar_duplicate_order_id) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);
    bool result = book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_FALSE(result);
    ASSERT_EQ(book.orderCount(), 1);
}

TEST(collar_best_bid_ask) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
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

TEST(collar_spread) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 101'0000, 100, Side::Sell);

    auto spread = book.spread();
    ASSERT_TRUE(spread.has_value());
    ASSERT_EQ(*spread, 1'0000);
}

TEST(collar_mid_price) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 102'0000, 100, Side::Sell);

    auto mid = book.midPrice();
    ASSERT_TRUE(mid.has_value());
    ASSERT_EQ(*mid, 101'0000);
}

TEST(collar_cancel_order) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);
    ASSERT_EQ(book.orderCount(), 1);

    bool result = book.cancelOrder(1);
    ASSERT_TRUE(result);
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_FALSE(book.bestBid().has_value());
}

TEST(collar_cancel_nonexistent_order) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    bool result = book.cancelOrder(999);
    ASSERT_FALSE(result);
}

TEST(collar_get_order) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);

    auto order = book.getOrder(1);
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->id, 1);
    ASSERT_EQ(order->price, 100'0000);
    ASSERT_EQ(order->quantity, 100);
    ASSERT_EQ(order->side, Side::Buy);
}

TEST(collar_clear_book) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 101'0000, 100, Side::Sell);

    book.clear();
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_FALSE(book.bestBid().has_value());
    ASSERT_FALSE(book.bestAsk().has_value());
}

// -----------------------------------------------------------------------
// Matching
// -----------------------------------------------------------------------

TEST(collar_limit_order_match) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    // Place a sell resting order
    book.addOrder(1, 100'0000, 100, Side::Sell);
    // A buy at the same price should match fully
    book.addOrder(2, 100'0000, 100, Side::Buy);

    // Both orders are filled; price levels should be empty
    ASSERT_FALSE(book.bestAsk().has_value());
    ASSERT_FALSE(book.bestBid().has_value());
    ASSERT_EQ(book.bidLevelCount(), 0);
    ASSERT_EQ(book.askLevelCount(), 0);

    // Verify filled status
    auto order1 = book.getOrder(1);
    auto order2 = book.getOrder(2);
    if (order1) {
        ASSERT_EQ(order1->status, OrderStatus::Filled);
    }
    if (order2) {
        ASSERT_EQ(order2->status, OrderStatus::Filled);
    }
}

TEST(collar_partial_match) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 50, Side::Sell);
    book.addOrder(2, 100'0000, 100, Side::Buy);

    // Sell order fully consumed; buy order partially filled and resting
    ASSERT_TRUE(book.bestBid().has_value());
    ASSERT_EQ(*book.bestBid(), 100'0000);
    ASSERT_EQ(book.getVolumeAtPrice(100'0000, Side::Buy), 50);
}

TEST(collar_price_time_priority) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    // Three resting sells at same price
    book.addOrder(1, 101'0000, 100, Side::Sell);
    book.addOrder(2, 101'0000, 100, Side::Sell);
    book.addOrder(3, 101'0000, 100, Side::Sell);

    auto bids = book.getAsks(1);
    ASSERT_EQ(bids.size(), 1);
    ASSERT_EQ(bids[0].quantity, 300);
    ASSERT_EQ(bids[0].orderCount, 3);
}

// -----------------------------------------------------------------------
// Modify
// -----------------------------------------------------------------------

TEST(collar_modify_to_valid_price) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);

    bool result = book.modifyOrder(1, 99'0000, 100);
    ASSERT_TRUE(result);

    auto order = book.getOrder(1);
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->price, 99'0000);
}

TEST(collar_modify_to_out_of_collar_price) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);

    bool result = book.modifyOrder(1, LOWER - TICK, 100);
    ASSERT_FALSE(result);

    // Original order still intact
    auto order = book.getOrder(1);
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->price, 100'0000);
}

// -----------------------------------------------------------------------
// Callbacks
// -----------------------------------------------------------------------

TEST(collar_trade_callback) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);

    int tradeCount = 0;
    book.setTradeCallback([&](const Trade &) { ++tradeCount; });

    book.addOrder(1, 100'0000, 100, Side::Sell);
    book.addOrder(2, 100'0000, 100, Side::Buy);

    ASSERT_EQ(tradeCount, 1);
}

// -----------------------------------------------------------------------
// Volume and depth
// -----------------------------------------------------------------------

TEST(collar_volume_at_price) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 100'0000, 150, Side::Buy);

    Quantity vol = book.getVolumeAtPrice(100'0000, Side::Buy);
    ASSERT_EQ(vol, 250);
}

TEST(collar_volume_out_of_collar) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
    Quantity vol = book.getVolumeAtPrice(UPPER + TICK, Side::Buy);
    ASSERT_EQ(vol, 0);
}

TEST(collar_market_depth) {
    PriceCollarOrderBook book(LOWER, UPPER, TICK);
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

// -----------------------------------------------------------------------
// Constructor validation
// -----------------------------------------------------------------------

TEST(collar_invalid_bounds) {
    bool threw = false;
    try {
        PriceCollarOrderBook book(110'0000, 90'0000, TICK);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(collar_invalid_tick) {
    bool threw = false;
    try {
        PriceCollarOrderBook book(LOWER, UPPER, 0);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

// -----------------------------------------------------------------------
// DynamicCircularQueue direct tests
// -----------------------------------------------------------------------

TEST(dcq_push_front_pop) {
    ds::DynamicCircularQueue<int> q;
    int a = 1, b = 2, c = 3;
    q.push(&a);
    q.push(&b);
    q.push(&c);

    ASSERT_EQ(q.size(), 3);
    ASSERT_EQ(*q.front(), 1);
    q.popFront();
    ASSERT_EQ(*q.front(), 2);
    q.popFront();
    ASSERT_EQ(*q.front(), 3);
    q.popFront();
    ASSERT_TRUE(q.empty());
}

TEST(dcq_remove_middle) {
    ds::DynamicCircularQueue<int> q;
    int a = 1, b = 2, c = 3;
    q.push(&a);
    q.push(&b);
    q.push(&c);

    bool removed = q.remove(&b);
    ASSERT_TRUE(removed);
    ASSERT_EQ(q.size(), 2);

    ASSERT_EQ(*q.front(), 1);
    q.popFront();
    ASSERT_EQ(*q.front(), 3);
    q.popFront();
    ASSERT_TRUE(q.empty());
}

TEST(dcq_remove_front) {
    ds::DynamicCircularQueue<int> q;
    int a = 1, b = 2;
    q.push(&a);
    q.push(&b);

    q.remove(&a);
    ASSERT_EQ(q.size(), 1);
    ASSERT_EQ(*q.front(), 2);
}

TEST(dcq_remove_nonexistent) {
    ds::DynamicCircularQueue<int> q;
    int a = 1, x = 99;
    q.push(&a);
    bool removed = q.remove(&x);
    ASSERT_FALSE(removed);
    ASSERT_EQ(q.size(), 1);
}

TEST(dcq_growth) {
    ds::DynamicCircularQueue<int> q(4);
    std::vector<int> vals(32);
    for (int i = 0; i < 32; ++i) {
        vals[i] = i;
        q.push(&vals[i]);
    }
    ASSERT_EQ(q.size(), 32);

    for (int i = 0; i < 32; ++i) {
        ASSERT_EQ(*q.front(), i);
        q.popFront();
    }
    ASSERT_TRUE(q.empty());
}

TEST(dcq_compaction_with_tombstones) {
    // Fill the queue to trigger compaction path:
    // push 8 items, remove half (leaving tombstones), then push more.
    ds::DynamicCircularQueue<int> q(8);
    std::vector<int> vals(16);
    for (int i = 0; i < 8; ++i) {
        vals[i] = i;
        q.push(&vals[i]);
    }
    // Remove even-indexed elements (tombstones in middle)
    for (int i = 0; i < 8; i += 2) {
        q.remove(&vals[i]);
    }
    // Push more to force compaction
    for (int i = 8; i < 16; ++i) {
        vals[i] = i;
        q.push(&vals[i]);
    }

    ASSERT_EQ(q.size(), 12);  // 4 remaining + 8 new
}

int main() {
    return TestSuite::instance().run();
}
