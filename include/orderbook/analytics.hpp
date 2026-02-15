#pragma once

#include "types.hpp"
#include "order_book.hpp"
#include <vector>
#include <numeric>
#include <cmath>
#include <deque>
#include <map>

namespace orderbook {

// Trading analytics and statistics
class Analytics {
public:
    struct VolumeProfile {
        Price price;
        Quantity volume;
    };
    
    struct Statistics {
        double vwap;              // Volume weighted average price
        double volatility;        // Price volatility
        Quantity total_volume;    // Total traded volume
        size_t trade_count;       // Number of trades
        Price high;               // Highest trade price
        Price low;                // Lowest trade price
        double avg_trade_size;    // Average trade size
    };
    
    Analytics() = default;
    
    // Record a trade
    void record_trade(const Trade& trade) {
        trades_.push_back(trade);
        
        // Keep last N trades for performance
        if (trades_.size() > max_history_) {
            trades_.pop_front();
        }
    }
    
    // Calculate VWAP (Volume Weighted Average Price)
    [[nodiscard]] double calculate_vwap() const {
        if (trades_.empty()) {
            return 0.0;
        }
        
        double sum_price_volume = 0.0;
        Quantity sum_volume = 0;
        
        for (const auto& trade : trades_) {
            sum_price_volume += static_cast<double>(trade.price) * trade.quantity;
            sum_volume += trade.quantity;
        }
        
        return sum_volume > 0 ? sum_price_volume / sum_volume : 0.0;
    }

    // Calculate time-weighted average price (TWAP)
    [[nodiscard]] double calculate_twap() const {
      if (trades_.empty()) {
        return 0.0;
      }

      double sum = 0.0;
      for (const auto &trade : trades_) {
        sum += static_cast<double>(trade.price);
      }

      return sum / trades_.size();
    }

    // Calculate price volatility (standard deviation)
    [[nodiscard]] double calculate_volatility() const {
        if (trades_.size() < 2) {
            return 0.0;
        }
        
        // Calculate mean price
        double mean = calculate_twap();

        // Calculate variance
        double variance = 0.0;
        for (const auto& trade : trades_) {
            double diff = static_cast<double>(trade.price) - mean;
            variance += diff * diff;
        }
        variance /= trades_.size();
        
        return std::sqrt(variance);
    }
    
    // Get volume profile (price -> volume distribution)
    [[nodiscard]] std::vector<VolumeProfile> get_volume_profile() const {
        std::map<Price, Quantity> profile;
        
        for (const auto& trade : trades_) {
            profile[trade.price] += trade.quantity;
        }
        
        std::vector<VolumeProfile> result;
        result.reserve(profile.size());
        
        for (const auto& [price, volume] : profile) {
            result.push_back({price, volume});
        }
        
        return result;
    }
    
    // Get comprehensive statistics
    [[nodiscard]] Statistics get_statistics() const {
        Statistics stats{};
        
        if (trades_.empty()) {
            return stats;
        }
        
        stats.vwap = calculate_vwap();
        stats.volatility = calculate_volatility();
        stats.trade_count = trades_.size();
        
        stats.high = std::numeric_limits<Price>::min();
        stats.low = std::numeric_limits<Price>::max();
        
        for (const auto& trade : trades_) {
            stats.total_volume += trade.quantity;
            stats.high = std::max(stats.high, trade.price);
            stats.low = std::min(stats.low, trade.price);
        }
        
        stats.avg_trade_size = stats.trade_count > 0 ? 
            static_cast<double>(stats.total_volume) / stats.trade_count : 0.0;
        
        return stats;
    }
    
    // Calculate order book imbalance (buy pressure vs sell pressure)
    [[nodiscard]] static double calculate_imbalance(const OrderBook& book, size_t depth = 5) {
        auto bids = book.get_bids(depth);
        auto asks = book.get_asks(depth);
        
        Quantity bid_volume = 0;
        Quantity ask_volume = 0;
        
        for (const auto& level : bids) {
            bid_volume += level.quantity;
        }
        
        for (const auto& level : asks) {
            ask_volume += level.quantity;
        }
        
        Quantity total = bid_volume + ask_volume;
        if (total == 0) {
            return 0.0;
        }
        
        return (static_cast<double>(bid_volume) - static_cast<double>(ask_volume)) / static_cast<double>(total);
    }

    [[nodiscard]] size_t trade_count() const noexcept { return trades_.size(); }

    [[nodiscard]] Price last_price() const noexcept {
      return trades_.empty() ? 0 : trades_.back().price;
    }

    // Reset analytics
    void clear() {
        trades_.clear();
    }
    
    void set_max_history(size_t max) {
        max_history_ = max;
    }

private:
    std::deque<Trade> trades_;
    size_t max_history_{10000};
};

} // namespace orderbook
