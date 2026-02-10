#pragma once

#include "order.hpp"
#include "price_level.hpp"
#include "types.hpp"
#include <map>
#include <unordered_map>
#include <functional>
#include <vector>
#include <optional>
#include <mutex>
#include <shared_mutex>

namespace orderbook {

// Callback types
using TradeCallback = std::function<void(const Trade&)>;
using ExecutionCallback = std::function<void(const ExecutionReport&)>;

// Order book for a single symbol with price-time priority matching
class OrderBook {
public:
    explicit OrderBook(SymbolId symbol_id, 
                      TradeCallback trade_callback = nullptr,
                      ExecutionCallback exec_callback = nullptr)
        : symbol_id_(symbol_id)
        , trade_callback_(std::move(trade_callback))
        , exec_callback_(std::move(exec_callback))
        , order_pool_(10000) {
    }
    
    // Add a new order to the book
    [[nodiscard]] bool add_order(OrderId order_id, Side side, Price price, 
                                  Quantity quantity, OrderType type = OrderType::Limit,
                                  TimeInForce tif = TimeInForce::GoodTillCancel) {
        std::unique_lock lock(mutex_);
        
        // Check for duplicate order ID
        if (orders_.contains(order_id)) {
            return false;
        }
        
        // Acquire order from pool
        Order* order = order_pool_.acquire();
        order->order_id = order_id;
        order->symbol_id = symbol_id_;
        order->price = price;
        order->quantity = quantity;
        order->side = side;
        order->type = type;
        order->time_in_force = tif;
        order->status = OrderStatus::Active;
        order->timestamp = get_timestamp();
        
        // Store in order map
        orders_[order_id] = order;
        
        // Try to match against opposite side
        if (type == OrderType::Limit) {
            match_order(order);
        }
        
        // If order still has remaining quantity and not IOC/FOK, add to book
        if (order->remaining_quantity() > 0) {
            if (tif == TimeInForce::ImmediateOrCancel) {
                // Cancel remaining quantity for IOC
                cancel_order_internal(order);
            } else if (tif == TimeInForce::FillOrKill && order->filled_quantity != order->quantity) {
                // Cancel entire order for FOK if not completely filled
                order->filled_quantity = 0;
                cancel_order_internal(order);
            } else {
                // Add to price level
                add_to_book(order);
            }
        }
        
        // Send execution report
        send_execution_report(order, order->filled_quantity > 0 ? 
                             ExecutionType::PartialFill : ExecutionType::New);
        
        return true;
    }
    
    // Cancel an existing order
    [[nodiscard]] bool cancel_order(OrderId order_id) {
        std::unique_lock lock(mutex_);
        
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return false;
        }
        
        Order* order = it->second;
        if (!order->is_fillable()) {
            return false;
        }
        
        cancel_order_internal(order);
        return true;
    }
    
    // Modify an existing order (cancel and replace)
    [[nodiscard]] bool modify_order(OrderId order_id, Price new_price, Quantity new_quantity) {
        std::unique_lock lock(mutex_);
        
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return false;
        }
        
        Order* old_order = it->second;
        if (!old_order->is_fillable()) {
            return false;
        }
        
        // Remove from book
        remove_from_book(old_order);
        
        // Update order details
        Quantity old_filled = old_order->filled_quantity;
        old_order->price = new_price;
        old_order->quantity = new_quantity;
        old_order->filled_quantity = std::min(old_filled, new_quantity);
        old_order->timestamp = get_timestamp();
        
        // Re-match and add to book
        if (old_order->remaining_quantity() > 0) {
            match_order(old_order);
            if (old_order->remaining_quantity() > 0) {
                add_to_book(old_order);
            }
        }
        
        send_execution_report(old_order, ExecutionType::Replaced);
        return true;
    }
    
    // Get current best bid price
    [[nodiscard]] std::optional<Price> best_bid() const {
        std::shared_lock lock(mutex_);
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.begin()->first;  // Highest bid is at begin() due to std::greater<>
    }
    
    // Get current best ask price
    [[nodiscard]] std::optional<Price> best_ask() const {
        std::shared_lock lock(mutex_);
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first;
    }
    
    // Get spread
    [[nodiscard]] std::optional<Price> spread() const {
        std::shared_lock lock(mutex_);
        if (bids_.empty() || asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first - bids_.begin()->first;
    }
    
    // Get mid price
    [[nodiscard]] std::optional<Price> mid_price() const {
        std::shared_lock lock(mutex_);
        if (bids_.empty() || asks_.empty()) {
            return std::nullopt;
        }
        return (bids_.begin()->first + asks_.begin()->first) / 2;
    }
    
    // Get order book depth
    [[nodiscard]] std::pair<size_t, size_t> depth() const {
        std::shared_lock lock(mutex_);
        return {bids_.size(), asks_.size()};
    }
    
    // Get total volume at price level
    [[nodiscard]] Quantity volume_at_price(Side side, Price price) const {
        std::shared_lock lock(mutex_);
        if (side == Side::Buy) {
            auto it = bids_.find(price);
            if (it != bids_.end()) {
                return it->second.total_quantity();
            }
        } else {
            auto it = asks_.find(price);
            if (it != asks_.end()) {
                return it->second.total_quantity();
            }
        }
        return 0;
    }
    
    // Get market depth snapshot
    struct DepthLevel {
        Price price;
        Quantity quantity;
        size_t order_count;
    };
    
    [[nodiscard]] std::pair<std::vector<DepthLevel>, std::vector<DepthLevel>> 
    get_depth(size_t max_levels = 10) const {
        std::shared_lock lock(mutex_);
        
        std::vector<DepthLevel> bid_levels;
        std::vector<DepthLevel> ask_levels;
        
        // Get bid levels (highest to lowest) - bids_ already sorted highest first
        size_t count = 0;
        for (auto it = bids_.begin(); it != bids_.end() && count < max_levels; ++it, ++count) {
            bid_levels.push_back({it->first, it->second.total_quantity(), it->second.order_count()});
        }
        
        // Get ask levels (lowest to highest)
        count = 0;
        for (auto it = asks_.begin(); it != asks_.end() && count < max_levels; ++it, ++count) {
            ask_levels.push_back({it->first, it->second.total_quantity(), it->second.order_count()});
        }
        
        return {bid_levels, ask_levels};
    }
    
    [[nodiscard]] SymbolId symbol_id() const noexcept {
        return symbol_id_;
    }
    
private:
    void match_order(Order* order) {
        if (order->is_buy()) {
            match_buy_order(order);
        } else {
            match_sell_order(order);
        }
        
        // Update incoming order status
        if (order->filled_quantity > 0) {
            order->status = order->remaining_quantity() > 0 ? 
                OrderStatus::PartiallyFilled : OrderStatus::Filled;
        }
    }
    
    void match_buy_order(Order* order) {
        while (order->remaining_quantity() > 0 && !asks_.empty()) {
            auto it = asks_.begin();
            Price ask_price = it->first;
            
            // Check if prices cross
            if (order->price < ask_price) {
                break;
            }
            
            PriceLevel& level = it->second;
            match_against_level(order, level, ask_price);
            
            // Remove empty price level
            if (level.empty()) {
                asks_.erase(it);
            }
        }
    }
    
    void match_sell_order(Order* order) {
        while (order->remaining_quantity() > 0 && !bids_.empty()) {
            auto it = bids_.begin();  // Highest bid
            Price bid_price = it->first;
            
            // Check if prices cross
            if (order->price > bid_price) {
                break;
            }
            
            PriceLevel& level = it->second;
            match_against_level(order, level, bid_price);
            
            // Remove empty price level
            if (level.empty()) {
                bids_.erase(it);
            }
        }
    }
    
    void match_against_level(Order* order, PriceLevel& level, Price trade_price) {
        // Match against orders in price level (FIFO)
        Order* resting_order = level.head();
        while (resting_order && order->remaining_quantity() > 0) {
            Quantity match_qty = std::min(order->remaining_quantity(), 
                                         resting_order->remaining_quantity());
            
            // Execute trade
            execute_trade(order, resting_order, match_qty, trade_price);
            
            Order* next = resting_order->next;
            
            // Update resting order
            Quantity old_remaining = resting_order->remaining_quantity();
            resting_order->filled_quantity += match_qty;
            
            if (resting_order->remaining_quantity() == 0) {
                resting_order->status = OrderStatus::Filled;
                level.remove_order(resting_order);
                send_execution_report(resting_order, ExecutionType::Fill);
            } else {
                resting_order->status = OrderStatus::PartiallyFilled;
                level.update_quantity(old_remaining, resting_order->remaining_quantity());
                send_execution_report(resting_order, ExecutionType::PartialFill);
            }
            
            resting_order = next;
        }
    }
    
    void execute_trade(Order* aggressive, Order* passive, Quantity quantity, Price price) {
        // Update aggressive order
        aggressive->filled_quantity += quantity;
        
        // Create trade report
        Trade trade;
        trade.buyer_order_id = aggressive->is_buy() ? aggressive->order_id : passive->order_id;
        trade.seller_order_id = aggressive->is_buy() ? passive->order_id : aggressive->order_id;
        trade.price = price;
        trade.quantity = quantity;
        trade.timestamp = get_timestamp();
        trade.symbol_id = symbol_id_;
        
        // Invoke callback
        if (trade_callback_) {
            trade_callback_(trade);
        }
    }
    
    void add_to_book(Order* order) {
        if (order->is_buy()) {
            bids_[order->price].add_order(order);
        } else {
            asks_[order->price].add_order(order);
        }
    }
    
    void remove_from_book(Order* order) {
        if (order->is_buy()) {
            auto it = bids_.find(order->price);
            if (it != bids_.end()) {
                it->second.remove_order(order);
                if (it->second.empty()) {
                    bids_.erase(it);
                }
            }
        } else {
            auto it = asks_.find(order->price);
            if (it != asks_.end()) {
                it->second.remove_order(order);
                if (it->second.empty()) {
                    asks_.erase(it);
                }
            }
        }
    }
    
    void cancel_order_internal(Order* order) {
        remove_from_book(order);
        order->status = OrderStatus::Cancelled;
        send_execution_report(order, ExecutionType::Cancelled);
    }
    
    void send_execution_report(Order* order, ExecutionType exec_type) {
        if (exec_callback_) {
            ExecutionReport report;
            report.order_id = order->order_id;
            report.symbol_id = order->symbol_id;
            report.exec_type = exec_type;
            report.order_status = order->status;
            report.side = order->side;
            report.price = order->price;
            report.quantity = order->quantity;
            report.filled_quantity = order->filled_quantity;
            report.remaining_quantity = order->remaining_quantity();
            report.timestamp = get_timestamp();
            exec_callback_(report);
        }
    }
    
    [[nodiscard]] static Timestamp get_timestamp() noexcept {
        return std::chrono::duration_cast<Timestamp>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        );
    }
    
    SymbolId symbol_id_;
    TradeCallback trade_callback_;
    ExecutionCallback exec_callback_;
    OrderPool order_pool_;
    
    // Price level maps (price -> level)
    // Bids: highest price first
    // Asks: lowest price first
    std::map<Price, PriceLevel, std::greater<>> bids_;
    std::map<Price, PriceLevel> asks_;
    
    // Order ID -> Order pointer map
    std::unordered_map<OrderId, Order*> orders_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
};

} // namespace orderbook
