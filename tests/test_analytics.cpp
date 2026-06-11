#include <orderbook.hpp>
#include "test_framework.hpp"

using namespace orderbook;
using namespace test;

TEST(vwap_calculation) {
    Analytics analytics;

    // Add some trades
    analytics.recordTrade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.recordTrade(Trade(3, 4, 101'0000, 200, Timestamp{0}));
    analytics.recordTrade(Trade(5, 6, 99'0000, 100, Timestamp{0}));

    double vwap = analytics.calculateVwap();

    // VWAP = (100*100 + 101*200 + 99*100) / (100+200+100)
    double expected = (100'0000 * 100.0 + 101'0000 * 200.0 + 99'0000 * 100.0) / 400.0;

    ASSERT_TRUE(std::abs(vwap - expected) < 1.0);  // Small tolerance
}

TEST(volatility_calculation) {
    Analytics analytics;

    analytics.recordTrade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.recordTrade(Trade(3, 4, 105'0000, 100, Timestamp{0}));
    analytics.recordTrade(Trade(5, 6, 95'0000, 100, Timestamp{0}));

    double volatility = analytics.calculateVolatility();

    ASSERT_TRUE(volatility > 0);
}

TEST(statistics) {
    Analytics analytics;

    analytics.recordTrade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.recordTrade(Trade(3, 4, 105'0000, 150, Timestamp{0}));
    analytics.recordTrade(Trade(5, 6, 95'0000, 50, Timestamp{0}));

    auto stats = analytics.getStatistics();

    ASSERT_EQ(stats.tradeCount, 3);
    ASSERT_EQ(stats.totalVolume, 300);
    ASSERT_EQ(stats.high, 105'0000);
    ASSERT_EQ(stats.low, 95'0000);
    ASSERT_TRUE(stats.avgTradeSize == 100.0);
}

TEST(volume_profile) {
    Analytics analytics;

    analytics.recordTrade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.recordTrade(Trade(3, 4, 100'0000, 50, Timestamp{0}));
    analytics.recordTrade(Trade(5, 6, 101'0000, 75, Timestamp{0}));

    auto profile = analytics.getVolumeProfile();

    ASSERT_EQ(profile.size(), 2);

    // Find the 100.00 level
    bool found = false;
    for (const auto &level : profile) {
        if (level.price == 100'0000) {
            ASSERT_EQ(level.volume, 150);
            found = true;
        }
    }
    ASSERT_TRUE(found);
}

TEST(imbalance_calculation) {
    OrderBook book;

    // Add more buy volume
    book.addOrder(1, 100'0000, 300, Side::Buy);
    book.addOrder(2, 99'0000, 200, Side::Buy);

    // Less sell volume
    book.addOrder(3, 101'0000, 100, Side::Sell);
    book.addOrder(4, 102'0000, 100, Side::Sell);

    double imbalance = Analytics::calculateImbalance(book);

    // (500 - 200) / 700 = 0.428...
    ASSERT_TRUE(imbalance > 0);  // Positive means more buy pressure
    ASSERT_TRUE(imbalance < 1.0);
}

TEST(analytics_clear) {
    Analytics analytics;

    analytics.recordTrade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    auto stats1 = analytics.getStatistics();
    ASSERT_EQ(stats1.tradeCount, 1);

    analytics.clear();
    auto stats2 = analytics.getStatistics();
    ASSERT_EQ(stats2.tradeCount, 0);
}

// Verify that boost::circular_buffer caps history at maxHistory_.
// When the buffer is full the oldest entries are automatically overwritten
// (no pop_front needed), and size() never exceeds the configured capacity.
TEST(circular_buffer_capacity_cap) {
    Analytics analytics;
    constexpr size_t CAP = 5;
    analytics.setMaxHistory(CAP);

    // Record more trades than the capacity
    for (size_t i = 0; i < CAP + 3; ++i) {
        analytics.recordTrade(Trade(i, i + 100, 100'0000, 10, Timestamp{0}));
    }

    // Statistics should reflect only the last CAP trades
    auto stats = analytics.getStatistics();
    ASSERT_EQ(stats.tradeCount, CAP);
    ASSERT_EQ(stats.totalVolume, static_cast<Quantity>(CAP * 10));
}

// Verify that setMaxHistory shrinks an existing buffer, keeping only the most
// recent entries when new capacity is smaller than current size.
TEST(circular_buffer_resize) {
    Analytics analytics;

    // Fill with 10 trades
    for (size_t i = 0; i < 10; ++i) {
        analytics.recordTrade(
            Trade(i, i + 100, 100'0000 + static_cast<Price>(i) * 100, 1, Timestamp{0}));
    }
    ASSERT_EQ(analytics.getStatistics().tradeCount, 10);

    // Shrink capacity to 4 — oldest 6 trades should be dropped
    analytics.setMaxHistory(4);
    ASSERT_EQ(analytics.getStatistics().tradeCount, 4);
}

int main() {
    return TestSuite::instance().run();
}
