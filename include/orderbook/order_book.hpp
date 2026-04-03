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
using TradeCallback = std::function<void(const Trade&)>;
using OrderUpdateCallback = std::function<void(const Order&)>;

// Main order book class implementing a limit order book with price-time priority
class OrderBook {
public:
    OrderBook() = default;
    ~OrderBook() = default;
    
    // Non-copyable
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    
    // Add a new order to the book
    [[nodiscard]] bool add_order(OrderId id, Price price, Quantity quantity, 
                                  Side side, OrderType type = OrderType::Limit);
    
    // Cancel an existing order
    [[nodiscard]] bool cancel_order(OrderId id);
    
    // Modify an existing order (price and/or quantity)
    [[nodiscard]] bool modify_order(OrderId id, Price new_price, Quantity new_quantity);
    
    // Get order by ID
    [[nodiscard]] std::optional<Order> get_order(OrderId id) const;
    
    // Get best bid price
    [[nodiscard]] std::optional<Price> best_bid() const;
    
    // Get best ask price
    [[nodiscard]] std::optional<Price> best_ask() const;
    
    // Get spread
    [[nodiscard]] std::optional<Price> spread() const;
    
    // Get mid price
    [[nodiscard]] std::optional<Price> mid_price() const;
    
    // Get market depth (top N levels)
    [[nodiscard]] std::vector<LevelInfo> get_bids(size_t depth = 10) const;
    [[nodiscard]] std::vector<LevelInfo> get_asks(size_t depth = 10) const;
    
    // Get total volume at price level
    [[nodiscard]] Quantity get_volume_at_price(Price price, Side side) const;
    
    // Set callbacks
    void set_trade_callback(TradeCallback callback) { trade_callback_ = std::move(callback); }
    void set_order_update_callback(OrderUpdateCallback callback) { order_update_callback_ = std::move(callback); }
    
    // Clear all orders
    void clear();
    
    // Statistics
    [[nodiscard]] size_t order_count() const;
    [[nodiscard]] size_t bid_level_count() const;
    [[nodiscard]] size_t ask_level_count() const;

private:
    // Internal matching logic
    void match_order(Order* order);
    void match_limit_order(Order* order);
    void match_market_order(Order* order);
    
    // Order management
    void add_to_book(Order* order);
    void remove_from_book(Order* order);
    
    // Execute a trade
    void execute_trade(Order* maker, Order* taker, Quantity quantity);
    
    // Notify callbacks
    void notify_trade(const Trade& trade);
    void notify_order_update(const Order& order);
    
    // Get current timestamp
    [[nodiscard]] Timestamp now() const;
    
    // Price levels: map of price -> PriceLevel
    // For bids: reverse order (highest price first)
    // For asks: normal order (lowest price first)
    std::map<Price, std::unique_ptr<PriceLevel>, std::greater<Price>> bids_;
    std::map<Price, std::unique_ptr<PriceLevel>> asks_;
    
    // Order lookup: order_id -> Order*
    std::unordered_map<OrderId, Order*> orders_;
    
    // Memory pool for efficient order allocation
    MemoryPool<Order> order_pool_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
    
    // Callbacks
    TradeCallback trade_callback_;
    OrderUpdateCallback order_update_callback_;
    
    // Pending notifications collected while the mutex is held.
    // Swapped to local vectors before the lock is released so that
    // callbacks are always invoked without the mutex, preventing
    // deadlocks when a callback re-enters the OrderBook.
    std::vector<Trade> pending_trades_;
    std::vector<Order> pending_updates_;
};

} // namespace orderbook
