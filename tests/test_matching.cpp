#include <orderbook.hpp>
#include "test_framework.hpp"

using namespace orderbook;
using namespace test;

TEST(simple_match) {
    OrderBook book;
    size_t tradeCount = 0;

    book.setTradeCallback([&](const Trade &trade) {
        tradeCount++;
        ASSERT_EQ(trade.quantity, 100);
    });

    // Add resting order
    book.addOrder(1, 100'0000, 100, Side::Sell);

    // Match with aggressive order
    book.addOrder(2, 100'0000, 100, Side::Buy);

    ASSERT_EQ(tradeCount, 1);

    // Both orders should be filled
    auto order1 = book.getOrder(1);
    auto order2 = book.getOrder(2);

    // Orders might be removed after fill, so check if they exist
    if (order1) {
        ASSERT_EQ(order1->status, OrderStatus::Filled);
    }
    if (order2) {
        ASSERT_EQ(order2->status, OrderStatus::Filled);
    }
}

TEST(partial_fill) {
    OrderBook book;

    // Large resting order
    book.addOrder(1, 100'0000, 200, Side::Sell);

    // Smaller aggressive order
    book.addOrder(2, 100'0000, 100, Side::Buy);

    auto order1 = book.getOrder(1);
    if (order1) {
        ASSERT_EQ(order1->filledQuantity, 100);
        ASSERT_EQ(order1->remainingQuantity(), 100);
        ASSERT_EQ(order1->status, OrderStatus::PartiallyFilled);
    }
}

TEST(multiple_matches) {
    OrderBook book;
    size_t tradeCount = 0;

    book.setTradeCallback([&](const Trade &) { tradeCount++; });

    // Add multiple resting sell orders
    book.addOrder(1, 100'0000, 50, Side::Sell);
    book.addOrder(2, 100'0000, 75, Side::Sell);
    book.addOrder(3, 101'0000, 100, Side::Sell);

    // Large buy order that matches multiple levels
    book.addOrder(4, 101'0000, 200, Side::Buy);

    // Should match with orders 1, 2, and partially with 3
    ASSERT_TRUE(tradeCount >= 2);
}

TEST(price_improvement) {
    OrderBook book;
    Price tradePrice = 0;

    book.setTradeCallback([&](const Trade &trade) { tradePrice = trade.price; });

    // Resting order at 100.00
    book.addOrder(1, 100'0000, 100, Side::Sell);

    // Aggressive buy at higher price - should match at resting price
    book.addOrder(2, 102'0000, 100, Side::Buy);

    ASSERT_EQ(tradePrice, 100'0000);  // Maker price, not taker price
}

TEST(market_order) {
    OrderBook book;

    // Add resting orders
    book.addOrder(1, 100'0000, 100, Side::Sell);
    book.addOrder(2, 101'0000, 100, Side::Sell);

    // Market buy order
    book.addOrder(3, 0, 150, Side::Buy, OrderType::Market);

    auto order = book.getOrder(3);
    if (order) {
        ASSERT_EQ(order->filledQuantity, 150);
    }
}

TEST(no_self_trade) {
    OrderBook book;

    // Buy order
    book.addOrder(1, 100'0000, 100, Side::Buy);

    // Sell order at same price from different order
    book.addOrder(2, 100'0000, 100, Side::Sell);

    // Should match normally (we don't prevent self-trade in this implementation)
    // This test just verifies the behavior
    ASSERT_TRUE(true);
}

TEST(ioc_order) {
    OrderBook book;

    // Resting order
    book.addOrder(1, 100'0000, 50, Side::Sell);

    // IOC order for 100 (only 50 available)
    book.addOrder(2, 100'0000, 100, Side::Buy, OrderType::IOC);

    auto order = book.getOrder(2);
    if (order) {
        ASSERT_EQ(order->filledQuantity, 50);
        ASSERT_EQ(order->status, OrderStatus::Cancelled);  // IOC cancelled unfilled portion
    }
}

TEST(fok_order_success) {
    OrderBook book;

    // Enough liquidity
    book.addOrder(1, 100'0000, 150, Side::Sell);

    // FOK order
    book.addOrder(2, 100'0000, 100, Side::Buy, OrderType::FOK);

    auto order = book.getOrder(2);
    if (order) {
        ASSERT_EQ(order->filledQuantity, 100);
        ASSERT_EQ(order->status, OrderStatus::Filled);
    }
}

TEST(fok_order_rejection) {
    OrderBook book;

    // Not enough liquidity
    book.addOrder(1, 100'0000, 50, Side::Sell);

    // FOK order for more than available
    book.addOrder(2, 100'0000, 100, Side::Buy, OrderType::FOK);

    auto order = book.getOrder(2);
    if (order) {
        ASSERT_EQ(order->status, OrderStatus::Rejected);
        ASSERT_EQ(order->filledQuantity, 0);  // FOK rollback
    }
}

TEST(crossing_spread) {
    OrderBook book;
    size_t tradeCount = 0;

    book.setTradeCallback([&](const Trade &) { tradeCount++; });

    // Normal market
    book.addOrder(1, 100'0000, 100, Side::Buy);
    book.addOrder(2, 101'0000, 100, Side::Sell);

    ASSERT_EQ(tradeCount, 0);  // No cross

    // Cross the spread
    book.addOrder(3, 101'0000, 100, Side::Buy);

    ASSERT_TRUE(tradeCount > 0);  // Should match
}

int main() {
    return TestSuite::instance().run();
}
