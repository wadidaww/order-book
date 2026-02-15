#pragma once

#include "analytics.hpp"
#include "order_book.hpp"
#include <atomic>
#include <chrono>
#include <thread>

namespace orderbook {

// Market maker strategy interface
class MarketMakerStrategy {
public:
  virtual ~MarketMakerStrategy() = default;

  struct Quote {
    Price bid_price;
    Quantity bid_quantity;
    Price ask_price;
    Quantity ask_quantity;
  };

  // Generate quotes based on market conditions
  [[nodiscard]] virtual Quote
  generate_quote(const OrderBook &book, const Analytics &analytics) const = 0;
};

// Simple market maker with fixed spread
class SimpleMarketMaker : public MarketMakerStrategy {
public:
  SimpleMarketMaker(Price spread, Quantity quote_size)
      : spread_(spread), quote_size_(quote_size) {}

  [[nodiscard]] Quote
  generate_quote(const OrderBook &book,
                 const Analytics &analytics) const override {
    Quote quote{};

    // Get mid price
    auto mid = book.mid_price();
    if (!mid) {
      // Use last trade price if no quotes
      if (analytics.trade_count() > 0) {
        mid = analytics.last_price();
      } else {
        return quote;
      }
    }

    Price half_spread = spread_ / 2;
    quote.bid_price = *mid - half_spread;
    quote.ask_price = *mid + half_spread;
    quote.bid_quantity = quote_size_;
    quote.ask_quantity = quote_size_;

    return quote;
  }

private:
  Price spread_;
  Quantity quote_size_;
};

// Arbitrage detector for cross-exchange opportunities
class ArbitrageDetector {
public:
  struct ArbitrageOpportunity {
    Price buy_price;
    Price sell_price;
    Quantity quantity;
    double profit_bps; // Profit in basis points
  };

  // Detect arbitrage between two order books
  [[nodiscard]] std::optional<ArbitrageOpportunity>
  detect(const OrderBook &book1, const OrderBook &book2,
         Price transaction_cost = 0) const {

    auto bid1 = book1.best_bid();
    auto ask1 = book1.best_ask();
    auto bid2 = book2.best_bid();
    auto ask2 = book2.best_ask();

    if (!bid1 || !ask1 || !bid2 || !ask2) {
      return std::nullopt;
    }

    // Check if we can buy on book1 and sell on book2
    if (*ask1 + transaction_cost < *bid2) {
      Quantity vol1 = book1.get_volume_at_price(*ask1, Side::Sell);
      Quantity vol2 = book2.get_volume_at_price(*bid2, Side::Buy);
      Quantity quantity = std::min(vol1, vol2);

      Price profit = *bid2 - *ask1 - transaction_cost;
      double profit_bps = (static_cast<double>(profit) / *ask1) * 10000.0;

      return ArbitrageOpportunity{*ask1, *bid2, quantity, profit_bps};
    }

    // Check if we can buy on book2 and sell on book1
    if (*ask2 + transaction_cost < *bid1) {
      Quantity vol1 = book2.get_volume_at_price(*ask2, Side::Sell);
      Quantity vol2 = book1.get_volume_at_price(*bid1, Side::Buy);
      Quantity quantity = std::min(vol1, vol2);

      Price profit = *bid1 - *ask2 - transaction_cost;
      double profit_bps = (static_cast<double>(profit) / *ask2) * 10000.0;

      return ArbitrageOpportunity{*ask2, *bid1, quantity, profit_bps};
    }

    return std::nullopt;
  }
};

// Simple market making strategy
// Posts bid and ask quotes around mid price with configurable spread
class MarketMaker {
public:
    struct Config {
        Price spread_ticks;        // Spread in ticks (scaled price units)
        Quantity quote_size;       // Size of each quote
        size_t max_position;       // Maximum inventory position
        bool enabled;
        
        Config() : spread_ticks(10), quote_size(100), max_position(1000), enabled(true) {}
    };

    explicit MarketMaker(OrderBook &book, Config config = Config{})
        : book_(book), config_(config), next_order_id_(1), position_(0),
          running_(false) {}

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
    
    void on_trade([[maybe_unused]] const Trade& trade) {
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
