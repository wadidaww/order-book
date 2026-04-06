#pragma once

#include "types.hpp"
#include "price_level.hpp"
#include "memory_pool.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <functional>
#include <optional>
#include <mutex>

namespace orderbook {

// Callback types for event notifications
using TradeCallback = std::function<void(const Trade &)>;
using OrderUpdateCallback = std::function<void(const Order &)>;

// Main order book class implementing a limit order book with price-time priority
class OrderBook {
  public:
    OrderBook() = default;
    ~OrderBook() = default;

    // Non-copyable, non-movable
    OrderBook(const OrderBook &) = delete;
    OrderBook &operator=(const OrderBook &) = delete;
    OrderBook(OrderBook &&) = delete;
    OrderBook &operator=(OrderBook &&) = delete;

    // Add a new order to the book
    [[nodiscard]] bool addOrder(OrderId id, Price price, Quantity quantity, Side side,
                                OrderType type = OrderType::Limit);

    // Cancel an existing order
    [[nodiscard]] bool cancelOrder(OrderId id);

    // Modify an existing order (price and/or quantity)
    [[nodiscard]] bool modifyOrder(OrderId id, Price newPrice, Quantity newQuantity);

    // Get order by ID
    [[nodiscard]] std::optional<Order> getOrder(OrderId id) const;

    // Get best bid price
    [[nodiscard]] std::optional<Price> bestBid() const;

    // Get best ask price
    [[nodiscard]] std::optional<Price> bestAsk() const;

    // Get spread
    [[nodiscard]] std::optional<Price> spread() const;

    // Get mid price
    [[nodiscard]] std::optional<Price> midPrice() const;

    // Get market depth (top N levels)
    [[nodiscard]] std::vector<LevelInfo> getBids(size_t depth = 10) const;
    [[nodiscard]] std::vector<LevelInfo> getAsks(size_t depth = 10) const;

    // Get total volume at price level
    [[nodiscard]] Quantity getVolumeAtPrice(Price price, Side side) const;

    // Set callbacks
    void setTradeCallback(TradeCallback callback) { tradeCallback_ = std::move(callback); }
    void setOrderUpdateCallback(OrderUpdateCallback callback) {
        orderUpdateCallback_ = std::move(callback);
    }

    // Clear all orders
    void clear();

    // Statistics
    [[nodiscard]] size_t orderCount() const;
    [[nodiscard]] size_t bidLevelCount() const;
    [[nodiscard]] size_t askLevelCount() const;

  private:
    // Internal matching logic
    void matchOrder(Order *order);
    void matchLimitOrder(Order *order);
    void matchMarketOrder(Order *order);

    // Order management
    void addToBook(Order *order);
    void removeFromBook(Order *order);

    // Execute a trade
    void executeTrade(Order *maker, Order *taker, Quantity quantity);

    // Notify callbacks
    void notifyTrade(const Trade &trade);
    void notifyOrderUpdate(const Order &order);

    // Get current timestamp
    [[nodiscard]] Timestamp now() const;

    // Price levels: map of price -> PriceLevel
    // For bids: reverse order (highest price first)
    // For asks: normal order (lowest price first)
    std::map<Price, std::unique_ptr<PriceLevel>, std::greater<Price>> bids_;
    std::map<Price, std::unique_ptr<PriceLevel>> asks_;

    // Order lookup: order_id -> Order*
    std::unordered_map<OrderId, Order *> orders_;

    // Memory pool for efficient order allocation
    MemoryPool<Order> orderPool_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Callbacks
    TradeCallback tradeCallback_;
    OrderUpdateCallback orderUpdateCallback_;

    // Pending notifications collected while the mutex is held.
    // Swapped to local vectors before the lock is released so that
    // callbacks are always invoked without the mutex, preventing
    // deadlocks when a callback re-enters the OrderBook.
    std::vector<Trade> pendingTrades_;
    std::vector<Order> pendingUpdates_;
};

}  // namespace orderbook
