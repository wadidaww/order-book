#pragma once

#include "types.hpp"
#include "order_book.hpp"
#include <vector>
#include <numeric>
#include <cmath>
#include <map>
#include <boost/circular_buffer.hpp>

namespace orderbook {

// Trading analytics and statistics.
//
// Maintains a rolling window of executed trades (capped at maxHistory_) and
// exposes statistical queries over that window:
//
//   • VWAP      — volume-weighted average price
//   • Volatility — population standard deviation of trade prices
//   • High/Low  — price extremes in the window
//   • Imbalance — (bidVol − askVol) / (bidVol + askVol) for top-N levels
//
// All query methods are const and may be called from multiple threads as long
// as no concurrent call to recordTrade() / clear() is in progress.
class Analytics {
  public:
    struct VolumeProfile {
        Price price;
        Quantity volume;
    };

    struct Statistics {
        double vwap;           // Volume-weighted average price
        double volatility;     // Population standard deviation of trade prices
        Quantity totalVolume;  // Total traded volume in the window
        size_t tradeCount;     // Number of trades in the window
        Price high;            // Highest trade price in the window
        Price low;             // Lowest trade price in the window
        double avgTradeSize;   // Average quantity per trade
    };

    Analytics() = default;

    // Record a trade for subsequent statistical analysis.
    // The circular_buffer automatically overwrites the oldest entry when the
    // buffer reaches capacity, so no explicit pop_front() is needed and the
    // underlying storage is never reallocated during normal operation.
    void recordTrade(const Trade &trade) { trades_.push_back(trade); }

    // Calculate VWAP (Volume-Weighted Average Price) over the history window.
    // Returns 0.0 if the window is empty.
    [[nodiscard]] double calculateVwap() const {
        if (trades_.empty()) {
            return 0.0;
        }

        double sumPriceVolume = 0.0;
        Quantity sumVolume = 0;

        for (const auto &trade : trades_) {
            sumPriceVolume += static_cast<double>(trade.price) * trade.quantity;
            sumVolume += trade.quantity;
        }

        return sumVolume > 0 ? sumPriceVolume / sumVolume : 0.0;
    }

    // Calculate population standard deviation of trade prices.
    // Returns 0.0 if fewer than 2 trades are in the window.
    [[nodiscard]] double calculateVolatility() const {
        if (trades_.size() < 2) {
            return 0.0;
        }

        // Calculate mean price
        double mean = 0.0;
        for (const auto &trade : trades_) {
            mean += static_cast<double>(trade.price);
        }
        mean /= trades_.size();

        // Calculate variance
        double variance = 0.0;
        for (const auto &trade : trades_) {
            double diff = static_cast<double>(trade.price) - mean;
            variance += diff * diff;
        }
        variance /= trades_.size();

        return std::sqrt(variance);
    }

    // Return price → volume distribution over the history window, sorted by
    // price in ascending order.
    [[nodiscard]] std::vector<VolumeProfile> getVolumeProfile() const {
        std::map<Price, Quantity> profile;

        for (const auto &trade : trades_) {
            profile[trade.price] += trade.quantity;
        }

        std::vector<VolumeProfile> result;
        result.reserve(profile.size());

        for (const auto &[price, volume] : profile) {
            result.push_back({price, volume});
        }

        return result;
    }

    // Return aggregate statistics over the current history window.
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

        for (const auto &trade : trades_) {
            stats.totalVolume += trade.quantity;
            stats.high = std::max(stats.high, trade.price);
            stats.low = std::min(stats.low, trade.price);
        }

        stats.avgTradeSize =
            stats.tradeCount > 0 ? static_cast<double>(stats.totalVolume) / stats.tradeCount : 0.0;

        return stats;
    }

    // Calculate order-book imbalance over the top `depth` price levels:
    //   result = (bidVolume − askVolume) / (bidVolume + askVolume)
    // Positive values indicate buy-side pressure; negative = sell-side.
    // Returns 0.0 when both sides are empty.
    [[nodiscard]] static double calculateImbalance(const OrderBook &book, size_t depth = 5) {
        auto bids = book.getBids(depth);
        auto asks = book.getAsks(depth);

        Quantity bidVolume = 0;
        Quantity askVolume = 0;

        for (const auto &level : bids) {
            bidVolume += level.quantity;
        }

        for (const auto &level : asks) {
            askVolume += level.quantity;
        }

        Quantity total = bidVolume + askVolume;
        if (total == 0) {
            return 0.0;
        }

        return (static_cast<double>(bidVolume) - static_cast<double>(askVolume)) /
               static_cast<double>(total);
    }

    // Clear the trade history.
    void clear() { trades_.clear(); }

    // Resize the trade history window.  The circular_buffer is resized in-place;
    // if new capacity < current size the oldest entries are dropped.
    void setMaxHistory(size_t max) {
        maxHistory_ = max;
        trades_.set_capacity(max);
    }

  private:
    // Default capacity of 10 000 trades.  boost::circular_buffer pre-allocates
    // a contiguous block of this size, giving O(1) push_back with zero
    // reallocation and much better cache behaviour than std::deque.
    static constexpr size_t kDefaultMaxHistory = 10'000;
    boost::circular_buffer<Trade> trades_{kDefaultMaxHistory};
    size_t maxHistory_{kDefaultMaxHistory};
};

}  // namespace orderbook
