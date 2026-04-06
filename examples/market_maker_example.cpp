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
    std::atomic<Quantity> totalVolume{0};

    book.setTradeCallback([&](const Trade &trade) {
        analytics.recordTrade(trade);
        totalVolume += trade.quantity;

        double price = static_cast<double>(trade.price) / PRICE_SCALE;
        std::cout << "Trade: " << trade.quantity << " @ " << std::fixed << std::setprecision(4)
                  << price << "\n";
    });

    // Initialize the book with some orders
    std::cout << "1. Initializing order book with base orders...\n";
    book.addOrder(1, 100'0000, 500, Side::Buy);
    book.addOrder(2, 99'5000, 600, Side::Buy);
    book.addOrder(3, 101'0000, 500, Side::Sell);
    book.addOrder(4, 101'5000, 600, Side::Sell);

    // Print initial state
    auto bestBid = book.bestBid();
    auto bestAsk = book.bestAsk();
    if (bestBid && bestAsk) {
        double bid = static_cast<double>(*bestBid) / PRICE_SCALE;
        double ask = static_cast<double>(*bestAsk) / PRICE_SCALE;
        std::cout << "Initial market: " << bid << " / " << ask << "\n\n";
    }

    // Configure market maker
    MarketMaker::Config config;
    config.spreadTicks = 5000;  // 0.50 spread
    config.quoteSize = 100;     // Quote 100 units
    config.maxPosition = 1000;  // Max 1000 units inventory
    config.enabled = true;

    MarketMaker mm(book, config);

    std::cout << "2. Starting market maker...\n";
    mm.start();

    // Simulate some incoming orders
    std::cout << "\n3. Simulating market activity...\n\n";

    OrderId orderId = 1000;

    // Simulate for a few seconds
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Randomly add buy or sell orders
        if (i % 3 == 0) {
            // Aggressive buy
            auto ask = book.bestAsk();
            if (ask) {
                book.addOrder(orderId++, *ask, 50, Side::Buy);
            }
        } else if (i % 3 == 1) {
            // Aggressive sell
            auto bid = book.bestBid();
            if (bid) {
                book.addOrder(orderId++, *bid, 50, Side::Sell);
            }
        } else {
            // Passive order
            auto mid = book.midPrice();
            if (mid) {
                bool isBuy = (i % 2 == 0);
                Price price = isBuy ? (*mid - 2000) : (*mid + 2000);
                book.addOrder(orderId++, price, 30, isBuy ? Side::Buy : Side::Sell);
            }
        }
    }

    std::cout << "\n4. Stopping market maker...\n";
    mm.stop();

    // Print final statistics
    auto stats = analytics.getStatistics();
    std::cout << "\n=== Final Statistics ===\n";
    std::cout << "Total trades: " << stats.tradeCount << "\n";
    std::cout << "Total volume: " << stats.totalVolume << "\n";
    std::cout << "VWAP: " << std::fixed << std::setprecision(4) << stats.vwap / PRICE_SCALE << "\n";
    std::cout << "Average trade size: " << stats.avgTradeSize << "\n";
    std::cout << "Market maker position: " << mm.position() << "\n";

    // Final book state
    auto finalBid = book.bestBid();
    auto finalAsk = book.bestAsk();
    if (finalBid && finalAsk) {
        double bid = static_cast<double>(*finalBid) / PRICE_SCALE;
        double ask = static_cast<double>(*finalAsk) / PRICE_SCALE;
        double spread = static_cast<double>(*finalAsk - *finalBid) / PRICE_SCALE;
        std::cout << "\nFinal market: " << bid << " / " << ask << " (spread: " << spread << ")\n";
    }

    double imbalance = Analytics::calculateImbalance(book);
    std::cout << "Order book imbalance: " << std::fixed << std::setprecision(2) << (imbalance * 100)
              << "%\n";

    return 0;
}
