#pragma once

#include "order_book.hpp"
#include <atomic>
#include <thread>
#include <chrono>

namespace orderbook {

// Simple market making strategy
// Posts bid and ask quotes around mid price with configurable spread
class MarketMaker {
public:
    struct Config {
        Price spread_ticks{10};        // Spread in ticks (scaled price units)
        Quantity quote_size{100};       // Size of each quote
        size_t max_position{1000};      // Maximum inventory position
        bool enabled{true};
    };
    
    explicit MarketMaker(OrderBook& book, Config config = Config{})
        : book_(book)
        , config_(config)
        , next_order_id_(1)
        , position_(0)
        , running_(false) {}
    
    void start() {
        running_ = true;
        worker_ = std::thread([this]() { run(); });
    }
    
    void stop() {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }
    
    void update_config(const Config& config) {
        config_ = config;
    }
    
    [[nodiscard]] int64_t position() const noexcept {
        return position_.load();
    }

private:
    void run() {
        while (running_) {
            if (config_.enabled) {
                quote();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void quote() {
        auto mid = book_.mid_price();
        if (!mid) {
            return;
        }
        
        Price mid_price = *mid;
        Price half_spread = config_.spread_ticks / 2;
        
        Price bid_price = mid_price - half_spread;
        Price ask_price = mid_price + half_spread;
        
        // Check position limits
        if (position_.load() < static_cast<int64_t>(config_.max_position)) {
            book_.add_order(next_order_id_++, bid_price, config_.quote_size, Side::Buy);
        }
        
        if (position_.load() > -static_cast<int64_t>(config_.max_position)) {
            book_.add_order(next_order_id_++, ask_price, config_.quote_size, Side::Sell);
        }
    }
    
    void on_trade(const Trade& trade) {
        // Update position based on trades
        // This is simplified - production version would track order ownership
    }
    
    OrderBook& book_;
    Config config_;
    std::atomic<OrderId> next_order_id_;
    std::atomic<int64_t> position_;
    std::atomic<bool> running_;
    std::thread worker_;
};

} // namespace orderbook
