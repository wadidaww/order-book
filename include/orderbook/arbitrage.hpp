#pragma once

#include "order_book.hpp"
#include <vector>
#include <optional>

namespace orderbook {

// Arbitrage detector for multiple order books
class ArbitrageDetector {
public:
    struct Opportunity {
        Price buy_price;
        Price sell_price;
        Quantity max_quantity;
        Price profit;
        size_t buy_book_idx;
        size_t sell_book_idx;
    };
    
    explicit ArbitrageDetector(std::vector<OrderBook*> books)
        : books_(std::move(books)) {}
    
    // Detect arbitrage opportunities across books
    [[nodiscard]] std::optional<Opportunity> detect() const {
        if (books_.size() < 2) {
            return std::nullopt;
        }
        
        Opportunity best_opp;
        best_opp.profit = 0;
        bool found = false;
        
        for (size_t i = 0; i < books_.size(); ++i) {
            auto best_ask_i = books_[i]->best_ask();
            if (!best_ask_i) continue;
            
            auto asks_i = books_[i]->get_asks(1);
            if (asks_i.empty()) continue;
            
            for (size_t j = 0; j < books_.size(); ++j) {
                if (i == j) continue;
                
                auto best_bid_j = books_[j]->best_bid();
                if (!best_bid_j) continue;
                
                auto bids_j = books_[j]->get_bids(1);
                if (bids_j.empty()) continue;
                
                // Check if we can buy at i and sell at j for profit
                if (*best_bid_j > *best_ask_i) {
                    Price profit = *best_bid_j - *best_ask_i;
                    
                    if (profit > best_opp.profit) {
                        best_opp.buy_price = *best_ask_i;
                        best_opp.sell_price = *best_bid_j;
                        best_opp.max_quantity = std::min(asks_i[0].quantity, bids_j[0].quantity);
                        best_opp.profit = profit;
                        best_opp.buy_book_idx = i;
                        best_opp.sell_book_idx = j;
                        found = true;
                    }
                }
            }
        }
        
        return found ? std::optional<Opportunity>(best_opp) : std::nullopt;
    }
    
    // Calculate profit percentage
    [[nodiscard]] static double profit_percentage(const Opportunity& opp) {
        if (opp.buy_price == 0) return 0.0;
        return (static_cast<double>(opp.profit) / opp.buy_price) * 100.0;
    }

private:
    std::vector<OrderBook*> books_;
};

} // namespace orderbook
