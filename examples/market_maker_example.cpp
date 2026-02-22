#include <orderbook.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace orderbook;

int main() {
    std::cout << "=== Market Maker Example ===\n\n";
    
    OrderBook book;
    Analytics analytics;
    
    // Track P&L
    std::atomic<int64_t> pnl{0};
    std::atomic<Quantity> total_volume{0};
    
    book.set_trade_callback([&](const Trade& trade) {
        analytics.record_trade(trade);
        total_volume += trade.quantity;
        
        double price = static_cast<double>(trade.price) / PRICE_SCALE;
        std::cout << "Trade: " << trade.quantity << " @ " 
                  << std::fixed << std::setprecision(4) << price << "\n";
    });
    
    // Initialize the book with some orders
    std::cout << "1. Initializing order book with base orders...\n";
    book.add_order(1, 100'0000, 500, Side::Buy);
    book.add_order(2, 99'5000, 600, Side::Buy);
    book.add_order(3, 101'0000, 500, Side::Sell);
    book.add_order(4, 101'5000, 600, Side::Sell);
    
    // Print initial state
    auto best_bid = book.best_bid();
    auto best_ask = book.best_ask();
    if (best_bid && best_ask) {
        double bid = static_cast<double>(*best_bid) / PRICE_SCALE;
        double ask = static_cast<double>(*best_ask) / PRICE_SCALE;
        std::cout << "Initial market: " << bid << " / " << ask << "\n\n";
    }
    
    // Configure market maker
    MarketMaker::Config config;
    config.spread_ticks = 5000;      // 0.50 spread
    config.quote_size = 100;         // Quote 100 units
    config.max_position = 1000;      // Max 1000 units inventory
    config.enabled = true;
    
    MarketMaker mm(book, config);
    
    std::cout << "2. Starting market maker...\n";
    mm.start();
    
    // Simulate some incoming orders
    std::cout << "\n3. Simulating market activity...\n\n";
    
    OrderId order_id = 1000;
    
    // Simulate for a few seconds
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Randomly add buy or sell orders
        if (i % 3 == 0) {
            // Aggressive buy
            auto ask = book.best_ask();
            if (ask) {
                book.add_order(order_id++, *ask, 50, Side::Buy);
            }
        } else if (i % 3 == 1) {
            // Aggressive sell
            auto bid = book.best_bid();
            if (bid) {
                book.add_order(order_id++, *bid, 50, Side::Sell);
            }
        } else {
            // Passive order
            auto mid = book.mid_price();
            if (mid) {
                bool is_buy = (i % 2 == 0);
                Price price = is_buy ? (*mid - 2000) : (*mid + 2000);
                book.add_order(order_id++, price, 30, 
                             is_buy ? Side::Buy : Side::Sell);
            }
        }
    }
    
    std::cout << "\n4. Stopping market maker...\n";
    mm.stop();
    
    // Print final statistics
    auto stats = analytics.get_statistics();
    std::cout << "\n=== Final Statistics ===\n";
    std::cout << "Total trades: " << stats.trade_count << "\n";
    std::cout << "Total volume: " << stats.total_volume << "\n";
    std::cout << "VWAP: " << std::fixed << std::setprecision(4) 
              << stats.vwap / PRICE_SCALE << "\n";
    std::cout << "Average trade size: " << stats.avg_trade_size << "\n";
    std::cout << "Market maker position: " << mm.position() << "\n";
    
    // Final book state
    auto final_bid = book.best_bid();
    auto final_ask = book.best_ask();
    if (final_bid && final_ask) {
        double bid = static_cast<double>(*final_bid) / PRICE_SCALE;
        double ask = static_cast<double>(*final_ask) / PRICE_SCALE;
        double spread = static_cast<double>(*final_ask - *final_bid) / PRICE_SCALE;
        std::cout << "\nFinal market: " << bid << " / " << ask 
                  << " (spread: " << spread << ")\n";
    }
    
    double imbalance = Analytics::calculate_imbalance(book);
    std::cout << "Order book imbalance: " << std::fixed << std::setprecision(2)
              << (imbalance * 100) << "%\n";
    
    return 0;
}
