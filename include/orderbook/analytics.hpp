#pragma once

#include "types.hpp"
#include "orderbook.hpp"
#include <vector>
#include <deque>
#include <numeric>
#include <algorithm>
#include <cmath>

namespace orderbook {

// Market statistics and analytics
class MarketAnalytics {
public:
    explicit MarketAnalytics(size_t window_size = 1000)
        : window_size_(window_size) {
    }
    
    // Record a trade
    void record_trade(const Trade& trade) {
        trades_.push_back(trade);
        if (trades_.size() > window_size_) {
            trades_.pop_front();
        }
        
        // Update statistics
        total_volume_ += trade.quantity;
        trade_count_++;
        
        if (trade_count_ == 1) {
            high_price_ = trade.price;
            low_price_ = trade.price;
            first_price_ = trade.price;
        } else {
            high_price_ = std::max(high_price_, trade.price);
            low_price_ = std::min(low_price_, trade.price);
        }
        last_price_ = trade.price;
    }
    
    // Calculate volume-weighted average price (VWAP)
    [[nodiscard]] double vwap() const {
        if (trades_.empty()) {
            return 0.0;
        }
        
        double total_value = 0.0;
        Quantity total_qty = 0;
        
        for (const auto& trade : trades_) {
            total_value += static_cast<double>(trade.price) * trade.quantity;
            total_qty += trade.quantity;
        }
        
        return total_qty > 0 ? total_value / total_qty : 0.0;
    }
    
    // Calculate time-weighted average price (TWAP)
    [[nodiscard]] double twap() const {
        if (trades_.empty()) {
            return 0.0;
        }
        
        double sum = 0.0;
        for (const auto& trade : trades_) {
            sum += static_cast<double>(trade.price);
        }
        
        return sum / trades_.size();
    }
    
    // Calculate price volatility (standard deviation)
    [[nodiscard]] double volatility() const {
        if (trades_.size() < 2) {
            return 0.0;
        }
        
        double mean = twap();
        double variance = 0.0;
        
        for (const auto& trade : trades_) {
            double diff = static_cast<double>(trade.price) - mean;
            variance += diff * diff;
        }
        
        return std::sqrt(variance / trades_.size());
    }
    
    // Get recent trades
    [[nodiscard]] const std::deque<Trade>& recent_trades() const {
        return trades_;
    }
    
    [[nodiscard]] Price high_price() const { return high_price_; }
    [[nodiscard]] Price low_price() const { return low_price_; }
    [[nodiscard]] Price first_price() const { return first_price_; }
    [[nodiscard]] Price last_price() const { return last_price_; }
    [[nodiscard]] Quantity total_volume() const { return total_volume_; }
    [[nodiscard]] size_t trade_count() const { return trade_count_; }
    
    // Reset statistics
    void reset() {
        trades_.clear();
        total_volume_ = 0;
        trade_count_ = 0;
        high_price_ = 0;
        low_price_ = 0;
        first_price_ = 0;
        last_price_ = 0;
    }
    
private:
    size_t window_size_;
    std::deque<Trade> trades_;
    Quantity total_volume_{0};
    size_t trade_count_{0};
    Price high_price_{0};
    Price low_price_{0};
    Price first_price_{0};
    Price last_price_{0};
};

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
    [[nodiscard]] virtual Quote generate_quote(const OrderBook& book, 
                                               const MarketAnalytics& analytics) const = 0;
};

// Simple market maker with fixed spread
class SimpleMarketMaker : public MarketMakerStrategy {
public:
    SimpleMarketMaker(Price spread, Quantity quote_size)
        : spread_(spread), quote_size_(quote_size) {
    }
    
    [[nodiscard]] Quote generate_quote(const OrderBook& book, 
                                       const MarketAnalytics& analytics) const override {
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
        double profit_bps;  // Profit in basis points
    };
    
    // Detect arbitrage between two order books
    [[nodiscard]] std::optional<ArbitrageOpportunity> 
    detect(const OrderBook& book1, const OrderBook& book2, 
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
            Quantity vol1 = book1.volume_at_price(Side::Sell, *ask1);
            Quantity vol2 = book2.volume_at_price(Side::Buy, *bid2);
            Quantity quantity = std::min(vol1, vol2);
            
            Price profit = *bid2 - *ask1 - transaction_cost;
            double profit_bps = (static_cast<double>(profit) / *ask1) * 10000.0;
            
            return ArbitrageOpportunity{*ask1, *bid2, quantity, profit_bps};
        }
        
        // Check if we can buy on book2 and sell on book1
        if (*ask2 + transaction_cost < *bid1) {
            Quantity vol1 = book2.volume_at_price(Side::Sell, *ask2);
            Quantity vol2 = book1.volume_at_price(Side::Buy, *bid1);
            Quantity quantity = std::min(vol1, vol2);
            
            Price profit = *bid1 - *ask2 - transaction_cost;
            double profit_bps = (static_cast<double>(profit) / *ask2) * 10000.0;
            
            return ArbitrageOpportunity{*ask2, *bid1, quantity, profit_bps};
        }
        
        return std::nullopt;
    }
};

} // namespace orderbook
