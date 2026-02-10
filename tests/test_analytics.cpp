#include <orderbook.hpp>
#include <cassert>
#include <iostream>
#include <cmath>

using namespace orderbook;

void test_market_analytics_vwap() {
    std::cout << "Test: VWAP Calculation... ";
    
    MarketAnalytics analytics;
    
    Trade trade1{1, 2, 10000, 100, Timestamp{0}, 1};
    Trade trade2{3, 4, 10100, 200, Timestamp{0}, 1};
    Trade trade3{5, 6, 9900, 150, Timestamp{0}, 1};
    
    analytics.record_trade(trade1);
    analytics.record_trade(trade2);
    analytics.record_trade(trade3);
    
    // VWAP = (10000*100 + 10100*200 + 9900*150) / (100+200+150)
    //      = (1000000 + 2020000 + 1485000) / 450
    //      = 4505000 / 450 = 10011.11
    double vwap = analytics.vwap();
    assert(std::abs(vwap - 10011.11) < 0.1);
    
    std::cout << "PASSED" << std::endl;
}

void test_market_analytics_statistics() {
    std::cout << "Test: Market Statistics... ";
    
    MarketAnalytics analytics;
    
    Trade trade1{1, 2, 10000, 100, Timestamp{0}, 1};
    Trade trade2{3, 4, 10100, 200, Timestamp{0}, 1};
    Trade trade3{5, 6, 9900, 150, Timestamp{0}, 1};
    
    analytics.record_trade(trade1);
    analytics.record_trade(trade2);
    analytics.record_trade(trade3);
    
    assert(analytics.high_price() == 10100);
    assert(analytics.low_price() == 9900);
    assert(analytics.first_price() == 10000);
    assert(analytics.last_price() == 9900);
    assert(analytics.total_volume() == 450);
    assert(analytics.trade_count() == 3);
    
    std::cout << "PASSED" << std::endl;
}

void test_simple_market_maker() {
    std::cout << "Test: Simple Market Maker... ";
    
    OrderBook book(1);
    MarketAnalytics analytics;
    
    // Add some baseline orders
    book.add_order(1, Side::Buy, 9900, 100);
    book.add_order(2, Side::Sell, 10100, 100);
    
    SimpleMarketMaker mm(100, 50);  // 1.00 spread, 50 size
    
    auto quote = mm.generate_quote(book, analytics);
    
    // Mid price is 10000, so quote should be 9950 / 10050
    assert(quote.bid_price == 9950);
    assert(quote.ask_price == 10050);
    assert(quote.bid_quantity == 50);
    assert(quote.ask_quantity == 50);
    
    std::cout << "PASSED" << std::endl;
}

void test_arbitrage_detection() {
    std::cout << "Test: Arbitrage Detection... ";
    
    OrderBook book1(1);
    OrderBook book2(1);
    
    // Book 1: Lower prices
    book1.add_order(1, Side::Buy, 9900, 100);
    book1.add_order(2, Side::Sell, 10000, 100);
    
    // Book 2: Higher prices (arbitrage opportunity)
    book2.add_order(3, Side::Buy, 10100, 100);
    book2.add_order(4, Side::Sell, 10200, 100);
    
    ArbitrageDetector detector;
    
    // Check for arbitrage
    auto opportunity = detector.detect(book1, book2, 0);
    
    assert(opportunity.has_value());
    assert(opportunity->buy_price == 10000);  // Buy on book1
    assert(opportunity->sell_price == 10100); // Sell on book2
    assert(opportunity->quantity == 100);
    assert(opportunity->profit_bps > 0);
    
    std::cout << "PASSED" << std::endl;
}

void test_no_arbitrage() {
    std::cout << "Test: No Arbitrage When Spreads Don't Cross... ";
    
    OrderBook book1(1);
    OrderBook book2(1);
    
    // Book 1
    book1.add_order(1, Side::Buy, 9900, 100);
    book1.add_order(2, Side::Sell, 10100, 100);
    
    // Book 2: Similar prices (no arbitrage)
    book2.add_order(3, Side::Buy, 9950, 100);
    book2.add_order(4, Side::Sell, 10050, 100);
    
    ArbitrageDetector detector;
    
    auto opportunity = detector.detect(book1, book2, 0);
    
    assert(!opportunity.has_value());
    
    std::cout << "PASSED" << std::endl;
}

void run_analytics_tests() {
    std::cout << "=== Analytics Tests ===" << std::endl;
    
    test_market_analytics_vwap();
    test_market_analytics_statistics();
    test_simple_market_maker();
    test_arbitrage_detection();
    test_no_arbitrage();
    
    std::cout << "All analytics tests passed!" << std::endl;
    std::cout << std::endl;
}
