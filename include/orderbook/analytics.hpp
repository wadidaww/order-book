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
        Quantity totalVolume;     // Total traded volume
        size_t tradeCount;        // Number of trades
        Price high;               // Highest trade price
        Price low;                // Lowest trade price
        double avgTradeSize;      // Average trade size
    };
    
    Analytics() = default;
    
    // Record a trade
    void recordTrade(const Trade& trade) {
        trades_.push_back(trade);
        
        // Keep last N trades for performance
        if (trades_.size() > maxHistory_) {
            trades_.pop_front();
        }
    }
    
    // Calculate VWAP (Volume Weighted Average Price)
    [[nodiscard]] double calculateVwap() const {
        if (trades_.empty()) {
            return 0.0;
        }
        
        double sumPriceVolume = 0.0;
        Quantity sumVolume = 0;
        
        for (const auto& trade : trades_) {
            sumPriceVolume += static_cast<double>(trade.price) * trade.quantity;
            sumVolume += trade.quantity;
        }
        
        return sumVolume > 0 ? sumPriceVolume / sumVolume : 0.0;
    }
    
    // Calculate price volatility (standard deviation)
    [[nodiscard]] double calculateVolatility() const {
        if (trades_.size() < 2) {
            return 0.0;
        }
        
        // Calculate mean price
        double mean = 0.0;
        for (const auto& trade : trades_) {
            mean += static_cast<double>(trade.price);
        }
        mean /= trades_.size();
        
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
    [[nodiscard]] std::vector<VolumeProfile> getVolumeProfile() const {
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
    [[nodiscard]] Statistics getStatistics() const {
        Statistics stats{};
        
        if (trades_.empty()) {
            return stats;
        }
        
        stats.vwap = calculateVwap();
        stats.volatility = calculateVolatility();
        stats.tradeCount = trades_.size();
        
        stats.high = std::numeric_limits<Price>::min();
        stats.low = std::numeric_limits<Price>::max();
        
        for (const auto& trade : trades_) {
            stats.totalVolume += trade.quantity;
            stats.high = std::max(stats.high, trade.price);
            stats.low = std::min(stats.low, trade.price);
        }
        
        stats.avgTradeSize = stats.tradeCount > 0 ? 
            static_cast<double>(stats.totalVolume) / stats.tradeCount : 0.0;
        
        return stats;
    }
    
    // Calculate order book imbalance (buy pressure vs sell pressure)
    [[nodiscard]] static double calculateImbalance(const OrderBook& book, size_t depth = 5) {
        auto bids = book.getBids(depth);
        auto asks = book.getAsks(depth);
        
        Quantity bidVolume = 0;
        Quantity askVolume = 0;
        
        for (const auto& level : bids) {
            bidVolume += level.quantity;
        }
        
        for (const auto& level : asks) {
            askVolume += level.quantity;
        }
        
        Quantity total = bidVolume + askVolume;
        if (total == 0) {
            return 0.0;
        }
        
        return (static_cast<double>(bidVolume) - static_cast<double>(askVolume)) / static_cast<double>(total);
    }
    
    // Reset analytics
    void clear() {
        trades_.clear();
    }
    
    void setMaxHistory(size_t max) {
        maxHistory_ = max;
    }

private:
    std::deque<Trade> trades_;
    size_t maxHistory_{10000};
};

} // namespace orderbook
