#pragma once

#include "order_book.hpp"
#include "cache.hpp"
#include <atomic>
#include <thread>
#include <chrono>

namespace orderbook {

// Configuration for the MarketMaker strategy.
// Defined outside the class so that it can be used as a default argument
// without triggering GCC's restriction on nested-class default member
// initializers referenced from default arguments of the enclosing class.
struct MarketMakerConfig {
    Price spreadTicks{10};     // Spread in ticks (scaled price units)
    Quantity quoteSize{100};   // Size of each quote
    size_t maxPosition{1000};  // Maximum inventory position
    bool enabled{true};
};

// Simple market making strategy
// Posts bid and ask quotes around mid price with configurable spread
class MarketMaker {
  public:
    using Config = MarketMakerConfig;

    explicit MarketMaker(OrderBook &book, Config config = Config{})
        : book_(book)
        , config_(config)
        , nextOrderId_(1)
        , position_(0)
        , running_(false) {}

    ~MarketMaker() { stop(); }

    // Non-copyable, non-movable
    MarketMaker(const MarketMaker &) = delete;
    MarketMaker &operator=(const MarketMaker &) = delete;
    MarketMaker(MarketMaker &&) = delete;
    MarketMaker &operator=(MarketMaker &&) = delete;

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

    void updateConfig(const Config &config) { config_ = config; }

    [[nodiscard]] int64_t position() const noexcept { return position_.load(); }

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
        auto mid = book_.midPrice();
        if (!mid) {
            return;
        }

        Price midPrice = *mid;
        Price halfSpread = config_.spreadTicks / 2;

        Price bidPrice = midPrice - halfSpread;
        Price askPrice = midPrice + halfSpread;

        // Check position limits
        if (position_.load() < static_cast<int64_t>(config_.maxPosition)) {
            book_.addOrder(nextOrderId_++, bidPrice, config_.quoteSize, Side::Buy);
        }

        if (position_.load() > -static_cast<int64_t>(config_.maxPosition)) {
            book_.addOrder(nextOrderId_++, askPrice, config_.quoteSize, Side::Sell);
        }
    }

    OrderBook &book_;
    Config config_;
    std::atomic<OrderId> nextOrderId_;
    std::atomic<int64_t> position_;
    // Isolated on its own cache line: written by the main thread (stop()) while
    // nextOrderId_ and position_ are written by the worker thread.  Without the
    // alignment the two groups would share a cache line, causing false sharing.
    alignas(detail::destructive_interference_size) std::atomic<bool> running_;
    std::thread worker_;
};

}  // namespace orderbook
