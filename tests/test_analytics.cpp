#include <orderbook.hpp>
#include "test_framework.hpp"

using namespace orderbook;
using namespace test;

TEST(vwap_calculation) {
    Analytics analytics;
    
    // Add some trades
    analytics.record_trade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.record_trade(Trade(3, 4, 101'0000, 200, Timestamp{0}));
    analytics.record_trade(Trade(5, 6, 99'0000, 100, Timestamp{0}));
    
    double vwap = analytics.calculate_vwap();
    
    // VWAP = (100*100 + 101*200 + 99*100) / (100+200+100)
    double expected = (100'0000 * 100.0 + 101'0000 * 200.0 + 99'0000 * 100.0) / 400.0;
    
    ASSERT_TRUE(std::abs(vwap - expected) < 1.0);  // Small tolerance
}

TEST(volatility_calculation) {
    Analytics analytics;
    
    analytics.record_trade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.record_trade(Trade(3, 4, 105'0000, 100, Timestamp{0}));
    analytics.record_trade(Trade(5, 6, 95'0000, 100, Timestamp{0}));
    
    double volatility = analytics.calculate_volatility();
    
    ASSERT_TRUE(volatility > 0);
}

TEST(statistics) {
    Analytics analytics;
    
    analytics.record_trade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.record_trade(Trade(3, 4, 105'0000, 150, Timestamp{0}));
    analytics.record_trade(Trade(5, 6, 95'0000, 50, Timestamp{0}));
    
    auto stats = analytics.get_statistics();
    
    ASSERT_EQ(stats.trade_count, 3);
    ASSERT_EQ(stats.total_volume, 300);
    ASSERT_EQ(stats.high, 105'0000);
    ASSERT_EQ(stats.low, 95'0000);
    ASSERT_TRUE(stats.avg_trade_size == 100.0);
}

TEST(volume_profile) {
    Analytics analytics;
    
    analytics.record_trade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    analytics.record_trade(Trade(3, 4, 100'0000, 50, Timestamp{0}));
    analytics.record_trade(Trade(5, 6, 101'0000, 75, Timestamp{0}));
    
    auto profile = analytics.get_volume_profile();
    
    ASSERT_EQ(profile.size(), 2);
    
    // Find the 100.00 level
    bool found = false;
    for (const auto& level : profile) {
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
    book.add_order(1, 100'0000, 300, Side::Buy);
    book.add_order(2, 99'0000, 200, Side::Buy);
    
    // Less sell volume
    book.add_order(3, 101'0000, 100, Side::Sell);
    book.add_order(4, 102'0000, 100, Side::Sell);
    
    double imbalance = Analytics::calculate_imbalance(book);
    
    // (500 - 200) / 700 = 0.428...
    ASSERT_TRUE(imbalance > 0);  // Positive means more buy pressure
    ASSERT_TRUE(imbalance < 1.0);
}

TEST(analytics_clear) {
    Analytics analytics;
    
    analytics.record_trade(Trade(1, 2, 100'0000, 100, Timestamp{0}));
    auto stats1 = analytics.get_statistics();
    ASSERT_EQ(stats1.trade_count, 1);
    
    analytics.clear();
    auto stats2 = analytics.get_statistics();
    ASSERT_EQ(stats2.trade_count, 0);
}

int main() {
    return TestSuite::instance().run();
}
