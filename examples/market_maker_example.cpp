#include <orderbook.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace orderbook;

int main() {
    std::cout << "=== Market Maker Example ===" << std::endl;
    std::cout << std::endl;
    
    // Track analytics
    MarketAnalytics analytics(100);
    
    // Create order book with analytics tracking
    auto trade_callback = [&analytics](const Trade& trade) {
        analytics.record_trade(trade);
        std::cout << "TRADE: Price=" << trade.price / 100.0
                  << " Qty=" << trade.quantity << std::endl;
    };
    
    OrderBook book(1, trade_callback);
    
    // Create market maker
    Price spread = 100;  // 1.00 spread
    Quantity quote_size = 100;
    SimpleMarketMaker market_maker(spread, quote_size);
    
    std::cout << "Initializing market with some orders..." << std::endl;
    
    // Add some baseline liquidity
    book.add_order(1, Side::Buy, 10000, 50);
    book.add_order(2, Side::Buy, 9900, 100);
    book.add_order(3, Side::Sell, 10100, 50);
    book.add_order(4, Side::Sell, 10200, 100);
    
    std::cout << std::endl;
    
    OrderId next_order_id = 100;
    
    // Simulate market making for a few iterations
    for (int i = 0; i < 5; ++i) {
        std::cout << "=== Market Making Iteration " << (i + 1) << " ===" << std::endl;
        
        // Generate quotes
        auto quote = market_maker.generate_quote(book, analytics);
        
        std::cout << "Generated Quote:" << std::endl;
        std::cout << "  Bid: " << quote.bid_price / 100.0 
                  << " @ " << quote.bid_quantity << std::endl;
        std::cout << "  Ask: " << quote.ask_price / 100.0 
                  << " @ " << quote.ask_quantity << std::endl;
        
        // Add market maker orders
        book.add_order(next_order_id++, Side::Buy, quote.bid_price, quote.bid_quantity);
        book.add_order(next_order_id++, Side::Sell, quote.ask_price, quote.ask_quantity);
        
        // Display market stats
        std::cout << "Market Stats:" << std::endl;
        auto best_bid = book.best_bid();
        auto best_ask = book.best_ask();
        auto mid = book.mid_price();
        
        if (best_bid) std::cout << "  Best Bid: " << *best_bid / 100.0 << std::endl;
        if (best_ask) std::cout << "  Best Ask: " << *best_ask / 100.0 << std::endl;
        if (mid) std::cout << "  Mid Price: " << *mid / 100.0 << std::endl;
        
        if (analytics.trade_count() > 0) {
            std::cout << "  VWAP: " << analytics.vwap() / 100.0 << std::endl;
            std::cout << "  Volume: " << analytics.total_volume() << std::endl;
            std::cout << "  Trades: " << analytics.trade_count() << std::endl;
        }
        
        // Simulate some market activity
        if (i % 2 == 0) {
            // Buy order that might match
            std::cout << "  Simulating buy order..." << std::endl;
            book.add_order(next_order_id++, Side::Buy, 10150, 25);
        } else {
            // Sell order that might match
            std::cout << "  Simulating sell order..." << std::endl;
            book.add_order(next_order_id++, Side::Sell, 9950, 25);
        }
        
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "=== Final Statistics ===" << std::endl;
    std::cout << "Total Trades: " << analytics.trade_count() << std::endl;
    std::cout << "Total Volume: " << analytics.total_volume() << std::endl;
    if (analytics.trade_count() > 0) {
        std::cout << "VWAP: " << analytics.vwap() / 100.0 << std::endl;
        std::cout << "TWAP: " << analytics.twap() / 100.0 << std::endl;
        std::cout << "Volatility: " << analytics.volatility() / 100.0 << std::endl;
        std::cout << "High: " << analytics.high_price() / 100.0 << std::endl;
        std::cout << "Low: " << analytics.low_price() / 100.0 << std::endl;
    }
    
    return 0;
}
