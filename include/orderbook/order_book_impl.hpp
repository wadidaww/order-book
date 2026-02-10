#pragma once

#include "order_book.hpp"
#include <algorithm>
#include <mutex>

namespace orderbook {

inline bool OrderBook::add_order(OrderId id, Price price, Quantity quantity, 
                                  Side side, OrderType type) {
    std::unique_lock lock(mutex_);
    
    // Check if order ID already exists
    if (orders_.find(id) != orders_.end()) {
        return false;
    }
    
    // Allocate order from pool
    auto* order = order_pool_.allocate(id, price, quantity, side, type, now());
    orders_[id] = order;
    
    // Match the order
    match_order(order);
    
    // If order is not fully filled and it's a limit order, add to book
    if (!order->is_filled() && order->type == OrderType::Limit) {
        add_to_book(order);
    }
    
    notify_order_update(*order);
    return true;
}

inline bool OrderBook::cancel_order(OrderId id) {
    std::unique_lock lock(mutex_);
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second;
    
    // Remove from book if still there
    if (order->status == OrderStatus::New || order->status == OrderStatus::PartiallyFilled) {
        remove_from_book(order);
    }
    
    order->status = OrderStatus::Cancelled;
    notify_order_update(*order);
    
    // Cleanup
    orders_.erase(it);
    order_pool_.deallocate(order);
    
    return true;
}

inline bool OrderBook::modify_order(OrderId id, Price new_price, Quantity new_quantity) {
    std::unique_lock lock(mutex_);
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second;
    
    // For simplicity, we cancel and replace
    // In production, might want to maintain time priority if price unchanged
    remove_from_book(order);
    
    order->price = new_price;
    order->quantity = order->filled_quantity + new_quantity;
    
    // Try to match with new price
    match_order(order);
    
    // Add back to book if not filled
    if (!order->is_filled() && order->type == OrderType::Limit) {
        add_to_book(order);
    }
    
    notify_order_update(*order);
    return true;
}

inline std::optional<Order> OrderBook::get_order(OrderId id) const {
    std::shared_lock lock(mutex_);
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    
    return *it->second;
}

inline std::optional<Price> OrderBook::best_bid() const {
    std::shared_lock lock(mutex_);
    
    if (bids_.empty()) {
        return std::nullopt;
    }
    
    return bids_.begin()->first;
}

inline std::optional<Price> OrderBook::best_ask() const {
    std::shared_lock lock(mutex_);
    
    if (asks_.empty()) {
        return std::nullopt;
    }
    
    return asks_.begin()->first;
}

inline std::optional<Price> OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    
    if (!bid || !ask) {
        return std::nullopt;
    }
    
    return *ask - *bid;
}

inline std::optional<Price> OrderBook::mid_price() const {
    auto bid = best_bid();
    auto ask = best_ask();
    
    if (!bid || !ask) {
        return std::nullopt;
    }
    
    return (*bid + *ask) / 2;
}

inline std::vector<LevelInfo> OrderBook::get_bids(size_t depth) const {
    std::shared_lock lock(mutex_);
    
    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, bids_.size()));
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        if (count >= depth) break;
        result.emplace_back(price, level->total_quantity(), level->order_count());
        ++count;
    }
    
    return result;
}

inline std::vector<LevelInfo> OrderBook::get_asks(size_t depth) const {
    std::shared_lock lock(mutex_);
    
    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, asks_.size()));
    
    size_t count = 0;
    for (const auto& [price, level] : asks_) {
        if (count >= depth) break;
        result.emplace_back(price, level->total_quantity(), level->order_count());
        ++count;
    }
    
    return result;
}

inline Quantity OrderBook::get_volume_at_price(Price price, Side side) const {
    std::shared_lock lock(mutex_);
    
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) {
            return 0;
        }
        return it->second->total_quantity();
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) {
            return 0;
        }
        return it->second->total_quantity();
    }
}

inline void OrderBook::clear() {
    std::unique_lock lock(mutex_);
    
    bids_.clear();
    asks_.clear();
    orders_.clear();
    order_pool_.clear();
}

inline size_t OrderBook::order_count() const {
    std::shared_lock lock(mutex_);
    return orders_.size();
}

inline size_t OrderBook::bid_level_count() const {
    std::shared_lock lock(mutex_);
    return bids_.size();
}

inline size_t OrderBook::ask_level_count() const {
    std::shared_lock lock(mutex_);
    return asks_.size();
}

inline void OrderBook::match_order(Order* order) {
    if (order->type == OrderType::Market) {
        match_market_order(order);
    } else {
        match_limit_order(order);
    }
}

inline void OrderBook::match_limit_order(Order* order) {
    if (order->side == Side::Buy) {
        // Match buy order against asks
        while (!order->is_filled() && !asks_.empty()) {
            auto& [price, level] = *asks_.begin();
            
            // Check if we can match at this price level
            if (order->price < price) {
                break;
            }
            
            // Match orders at this level
            while (!order->is_filled() && !level->empty()) {
                Order* maker = level->front();
                Quantity match_qty = std::min(order->remaining_quantity(), 
                                             maker->remaining_quantity());
                
                execute_trade(maker, order, match_qty);
                
                // Update or remove maker order
                if (maker->is_filled()) {
                    level->remove_order(maker);
                    maker->status = OrderStatus::Filled;
                    notify_order_update(*maker);
                } else {
                    Quantity old_qty = maker->remaining_quantity() + match_qty;
                    level->update_quantity(old_qty, maker->remaining_quantity());
                    maker->status = OrderStatus::PartiallyFilled;
                    notify_order_update(*maker);
                }
            }
            
            // Remove empty level
            if (level->empty()) {
                asks_.erase(asks_.begin());
            }
        }
    } else {
        // Match sell order against bids
        while (!order->is_filled() && !bids_.empty()) {
            auto& [price, level] = *bids_.begin();
            
            // Check if we can match at this price level
            if (order->price > price) {
                break;
            }
            
            // Match orders at this level
            while (!order->is_filled() && !level->empty()) {
                Order* maker = level->front();
                Quantity match_qty = std::min(order->remaining_quantity(), 
                                             maker->remaining_quantity());
                
                execute_trade(maker, order, match_qty);
                
                // Update or remove maker order
                if (maker->is_filled()) {
                    level->remove_order(maker);
                    maker->status = OrderStatus::Filled;
                    notify_order_update(*maker);
                } else {
                    Quantity old_qty = maker->remaining_quantity() + match_qty;
                    level->update_quantity(old_qty, maker->remaining_quantity());
                    maker->status = OrderStatus::PartiallyFilled;
                    notify_order_update(*maker);
                }
            }
            
            // Remove empty level
            if (level->empty()) {
                bids_.erase(bids_.begin());
            }
        }
    }
    
    // Update order status
    if (order->is_filled()) {
        order->status = OrderStatus::Filled;
    } else if (order->filled_quantity > 0) {
        order->status = OrderStatus::PartiallyFilled;
    }
    
    // Handle IOC and FOK orders
    if (order->type == OrderType::IOC && !order->is_filled()) {
        order->status = OrderStatus::Cancelled;
    } else if (order->type == OrderType::FOK && order->filled_quantity != order->quantity) {
        order->status = OrderStatus::Rejected;
        order->filled_quantity = 0;  // Rollback
    }
}

inline void OrderBook::match_market_order(Order* order) {
    if (order->side == Side::Buy) {
        // Market buy order matches against asks
        while (!order->is_filled() && !asks_.empty()) {
            auto& [price, level] = *asks_.begin();
            
            while (!order->is_filled() && !level->empty()) {
                Order* maker = level->front();
                Quantity match_qty = std::min(order->remaining_quantity(), 
                                             maker->remaining_quantity());
                
                execute_trade(maker, order, match_qty);
                
                if (maker->is_filled()) {
                    level->remove_order(maker);
                    maker->status = OrderStatus::Filled;
                    notify_order_update(*maker);
                } else {
                    Quantity old_qty = maker->remaining_quantity() + match_qty;
                    level->update_quantity(old_qty, maker->remaining_quantity());
                    maker->status = OrderStatus::PartiallyFilled;
                    notify_order_update(*maker);
                }
            }
            
            if (level->empty()) {
                asks_.erase(asks_.begin());
            }
        }
    } else {
        // Market sell order matches against bids
        while (!order->is_filled() && !bids_.empty()) {
            auto& [price, level] = *bids_.begin();
            
            while (!order->is_filled() && !level->empty()) {
                Order* maker = level->front();
                Quantity match_qty = std::min(order->remaining_quantity(), 
                                             maker->remaining_quantity());
                
                execute_trade(maker, order, match_qty);
                
                if (maker->is_filled()) {
                    level->remove_order(maker);
                    maker->status = OrderStatus::Filled;
                    notify_order_update(*maker);
                } else {
                    Quantity old_qty = maker->remaining_quantity() + match_qty;
                    level->update_quantity(old_qty, maker->remaining_quantity());
                    maker->status = OrderStatus::PartiallyFilled;
                    notify_order_update(*maker);
                }
            }
            
            if (level->empty()) {
                bids_.erase(bids_.begin());
            }
        }
    }
    
    order->status = order->is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
}

inline void OrderBook::add_to_book(Order* order) {
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it == bids_.end()) {
            auto level = std::make_unique<PriceLevel>(order->price);
            level->add_order(order);
            bids_[order->price] = std::move(level);
        } else {
            it->second->add_order(order);
        }
    } else {
        auto it = asks_.find(order->price);
        if (it == asks_.end()) {
            auto level = std::make_unique<PriceLevel>(order->price);
            level->add_order(order);
            asks_[order->price] = std::move(level);
        } else {
            it->second->add_order(order);
        }
    }
}

inline void OrderBook::remove_from_book(Order* order) {
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it != bids_.end()) {
            it->second->remove_order(order);
            if (it->second->empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(order->price);
        if (it != asks_.end()) {
            it->second->remove_order(order);
            if (it->second->empty()) {
                asks_.erase(it);
            }
        }
    }
}

inline void OrderBook::execute_trade(Order* maker, Order* taker, Quantity quantity) {
    maker->filled_quantity += quantity;
    taker->filled_quantity += quantity;
    
    Trade trade(maker->id, taker->id, maker->price, quantity, now());
    notify_trade(trade);
}

inline void OrderBook::notify_trade(const Trade& trade) {
    if (trade_callback_) {
        trade_callback_(trade);
    }
}

inline void OrderBook::notify_order_update(const Order& order) {
    if (order_update_callback_) {
        order_update_callback_(order);
    }
}

inline Timestamp OrderBook::now() const {
    return std::chrono::duration_cast<Timestamp>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    );
}

} // namespace orderbook
