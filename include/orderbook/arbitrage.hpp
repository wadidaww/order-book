#pragma once

#include "order_book.hpp"
#include <vector>
#include <optional>

namespace orderbook {

// Arbitrage detector for multiple order books
class ArbitrageDetector {
  public:
    struct Opportunity {
        Price buyPrice;
        Price sellPrice;
        Quantity maxQuantity;
        Price profit;
        size_t buyBookIdx;
        size_t sellBookIdx;
    };

    explicit ArbitrageDetector(std::vector<OrderBook *> books)
        : books_(std::move(books)) {}

    // Detect arbitrage opportunities across books
    [[nodiscard]] std::optional<Opportunity> detect() const {
        if (books_.size() < 2) {
            return std::nullopt;
        }

        Opportunity bestOpp;
        bestOpp.profit = 0;
        bool found = false;

        for (size_t i = 0; i < books_.size(); ++i) {
            auto bestAskI = books_[i]->bestAsk();
            if (!bestAskI)
                continue;

            auto asksI = books_[i]->getAsks(1);
            if (asksI.empty())
                continue;

            for (size_t j = 0; j < books_.size(); ++j) {
                if (i == j)
                    continue;

                auto bestBidJ = books_[j]->bestBid();
                if (!bestBidJ)
                    continue;

                auto bidsJ = books_[j]->getBids(1);
                if (bidsJ.empty())
                    continue;

                // Check if we can buy at i and sell at j for profit
                if (*bestBidJ > *bestAskI) {
                    Price profit = *bestBidJ - *bestAskI;

                    if (profit > bestOpp.profit) {
                        bestOpp.buyPrice = *bestAskI;
                        bestOpp.sellPrice = *bestBidJ;
                        bestOpp.maxQuantity = std::min(asksI[0].quantity, bidsJ[0].quantity);
                        bestOpp.profit = profit;
                        bestOpp.buyBookIdx = i;
                        bestOpp.sellBookIdx = j;
                        found = true;
                    }
                }
            }
        }

        return found ? std::optional<Opportunity>(bestOpp) : std::nullopt;
    }

    // Calculate profit percentage
    [[nodiscard]] static double profitPercentage(const Opportunity &opp) {
        if (opp.buyPrice == 0)
            return 0.0;
        return (static_cast<double>(opp.profit) / opp.buyPrice) * 100.0;
    }

  private:
    std::vector<OrderBook *> books_;
};

}  // namespace orderbook
