#pragma once

#include "order_book.hpp"
#include <vector>
#include <optional>

namespace orderbook {

// Cross-exchange arbitrage scanner.
//
// Scans every ordered pair of `OrderBook` instances and checks whether
// `bestBid(j) > bestAsk(i)` — i.e. it is possible to buy on book i and sell
// on book j at a profit.  Returns the most profitable opportunity found, or
// std::nullopt if none exists.
class ArbitrageDetector {
  public:
    // Describes a detected arbitrage opportunity.
    struct Opportunity {
        Price buyPrice;        // Price at which to buy (best ask of the buy book)
        Price sellPrice;       // Price at which to sell (best bid of the sell book)
        Quantity maxQuantity;  // Maximum executable quantity (min of the two top-level qtys)
        Price profit;          // sellPrice − buyPrice (in fixed-point units)
        size_t buyBookIdx;     // Index into the books vector for the buy leg
        size_t sellBookIdx;    // Index into the books vector for the sell leg
    };

    explicit ArbitrageDetector(std::vector<OrderBook *> books)
        : books_(std::move(books)) {}

    // Scan all ordered book pairs for crossed markets.  Returns the single
    // most profitable opportunity (highest profit per unit), or std::nullopt
    // if no arbitrage exists.  O(n²) in the number of books.
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

    // Return the profit as a percentage of the buy price:
    //   (profit / buyPrice) × 100
    // Returns 0.0 if buyPrice is zero.
    [[nodiscard]] static double profitPercentage(const Opportunity &opp) {
        if (opp.buyPrice == 0)
            return 0.0;
        return (static_cast<double>(opp.profit) / opp.buyPrice) * 100.0;
    }

  private:
    std::vector<OrderBook *> books_;
};

}  // namespace orderbook
