#pragma once

#include "order_book.hpp"
#include <algorithm>
#include <mutex>

namespace orderbook {

inline bool OrderBook::addOrder(OrderId id, Price price, Quantity quantity, 
                                  Side side, OrderType type) {
    std::vector<Trade> localTrades;
    std::vector<Order> localUpdates;

    {
        std::unique_lock lock(mutex_);

        // Check if order ID already exists
        if (orders_.find(id) != orders_.end()) {
            return false;
        }

        // Allocate order from pool
        auto* order = orderPool_.allocate(id, price, quantity, side, type, now());
        orders_[id] = order;

        // Match the order
        matchOrder(order);

        // If order is not fully filled and it's a limit order, add to book
        if (!order->isFilled() && order->type == OrderType::Limit) {
            addToBook(order);
        }

        notifyOrderUpdate(*order);

        // Collect notifications before releasing the lock
        localTrades.swap(pendingTrades_);
        localUpdates.swap(pendingUpdates_);
    }

    // Dispatch callbacks outside the lock to prevent re-entrancy deadlocks
    for (const auto& t : localTrades) {
        if (tradeCallback_) tradeCallback_(t);
    }
    for (const auto& u : localUpdates) {
        if (orderUpdateCallback_) orderUpdateCallback_(u);
    }

    return true;
}

inline bool OrderBook::cancelOrder(OrderId id) {
    std::vector<Order> localUpdates;

    {
        std::unique_lock lock(mutex_);

        auto it = orders_.find(id);
        if (it == orders_.end()) {
            return false;
        }

        Order* order = it->second;

        // Remove from book if still there
        if (order->status == OrderStatus::New || order->status == OrderStatus::PartiallyFilled) {
            removeFromBook(order);
        }

        order->status = OrderStatus::Cancelled;
        notifyOrderUpdate(*order);

        // Cleanup
        orders_.erase(it);
        orderPool_.deallocate(order);

        localUpdates.swap(pendingUpdates_);
    }

    for (const auto& u : localUpdates) {
        if (orderUpdateCallback_) orderUpdateCallback_(u);
    }

    return true;
}

inline bool OrderBook::modifyOrder(OrderId id, Price newPrice, Quantity newQuantity) {
    std::vector<Trade> localTrades;
    std::vector<Order> localUpdates;

    {
        std::unique_lock lock(mutex_);

        auto it = orders_.find(id);
        if (it == orders_.end()) {
            return false;
        }

        Order* order = it->second;

        // For simplicity, we cancel and replace
        // In production, might want to maintain time priority if price unchanged
        removeFromBook(order);

        order->price = newPrice;
        order->quantity = order->filledQuantity + newQuantity;

        // Try to match with new price
        matchOrder(order);

        // Add back to book if not filled
        if (!order->isFilled() && order->type == OrderType::Limit) {
            addToBook(order);
        }

        notifyOrderUpdate(*order);

        localTrades.swap(pendingTrades_);
        localUpdates.swap(pendingUpdates_);
    }

    for (const auto& t : localTrades) {
        if (tradeCallback_) tradeCallback_(t);
    }
    for (const auto& u : localUpdates) {
        if (orderUpdateCallback_) orderUpdateCallback_(u);
    }

    return true;
}

inline std::optional<Order> OrderBook::getOrder(OrderId id) const {
    std::shared_lock lock(mutex_);
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    
    return *it->second;
}

inline std::optional<Price> OrderBook::bestBid() const {
    std::shared_lock lock(mutex_);
    
    if (bids_.empty()) {
        return std::nullopt;
    }
    
    return bids_.begin()->first;
}

inline std::optional<Price> OrderBook::bestAsk() const {
    std::shared_lock lock(mutex_);
    
    if (asks_.empty()) {
        return std::nullopt;
    }
    
    return asks_.begin()->first;
}

inline std::optional<Price> OrderBook::spread() const {
    std::shared_lock lock(mutex_);

    if (bids_.empty() || asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first - bids_.begin()->first;
}

inline std::optional<Price> OrderBook::midPrice() const {
    std::shared_lock lock(mutex_);

    if (bids_.empty() || asks_.empty()) {
        return std::nullopt;
    }

    return (bids_.begin()->first + asks_.begin()->first) / 2;
}

inline std::vector<LevelInfo> OrderBook::getBids(size_t depth) const {
    std::shared_lock lock(mutex_);
    
    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, bids_.size()));
    
    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        if (count >= depth) break;
        result.emplace_back(price, level->totalQuantity(), level->orderCount());
        ++count;
    }
    
    return result;
}

inline std::vector<LevelInfo> OrderBook::getAsks(size_t depth) const {
    std::shared_lock lock(mutex_);
    
    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, asks_.size()));
    
    size_t count = 0;
    for (const auto& [price, level] : asks_) {
        if (count >= depth) break;
        result.emplace_back(price, level->totalQuantity(), level->orderCount());
        ++count;
    }
    
    return result;
}

inline Quantity OrderBook::getVolumeAtPrice(Price price, Side side) const {
    std::shared_lock lock(mutex_);
    
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) {
            return 0;
        }
        return it->second->totalQuantity();
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) {
            return 0;
        }
        return it->second->totalQuantity();
    }
}

inline void OrderBook::clear() {
    std::unique_lock lock(mutex_);
    
    bids_.clear();
    asks_.clear();
    orders_.clear();
    orderPool_.clear();
}

inline size_t OrderBook::orderCount() const {
    std::shared_lock lock(mutex_);
    return orders_.size();
}

inline size_t OrderBook::bidLevelCount() const {
    std::shared_lock lock(mutex_);
    return bids_.size();
}

inline size_t OrderBook::askLevelCount() const {
    std::shared_lock lock(mutex_);
    return asks_.size();
}

inline void OrderBook::matchOrder(Order* order) {
    if (order->type == OrderType::Market) {
        matchMarketOrder(order);
    } else {
        matchLimitOrder(order);
    }
}

inline void OrderBook::matchLimitOrder(Order* order) {
    if (order->side == Side::Buy) {
        // Match buy order against asks
        while (!order->isFilled() && !asks_.empty()) {
            auto& [price, level] = *asks_.begin();
            
            // Check if we can match at this price level
            if (order->price < price) {
                break;
            }
            
            // Match orders at this level
            while (!order->isFilled() && !level->empty()) {
                Order* maker = level->front();
                Quantity matchQty = std::min(order->remainingQuantity(), 
                                             maker->remainingQuantity());
                
                // Update level quantity BEFORE executing trade
                Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                Quantity newMakerQty = maker->remainingQuantity();
                level->updateQuantity(oldMakerQty, newMakerQty);
                
                // Remove maker order if filled
                if (maker->isFilled()) {
                    level->removeOrder(maker);
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }
            
            // Remove empty level
            if (level->empty()) {
                asks_.erase(asks_.begin());
            }
        }
    } else {
        // Match sell order against bids
        while (!order->isFilled() && !bids_.empty()) {
            auto& [price, level] = *bids_.begin();
            
            // Check if we can match at this price level
            if (order->price > price) {
                break;
            }
            
            // Match orders at this level
            while (!order->isFilled() && !level->empty()) {
                Order* maker = level->front();
                Quantity matchQty = std::min(order->remainingQuantity(), 
                                             maker->remainingQuantity());
                
                // Update level quantity BEFORE executing trade
                Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                Quantity newMakerQty = maker->remainingQuantity();
                level->updateQuantity(oldMakerQty, newMakerQty);
                
                // Remove maker order if filled
                if (maker->isFilled()) {
                    level->removeOrder(maker);
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }
            
            // Remove empty level
            if (level->empty()) {
                bids_.erase(bids_.begin());
            }
        }
    }
    
    // Update order status
    if (order->isFilled()) {
        order->status = OrderStatus::Filled;
    } else if (order->filledQuantity > 0) {
        order->status = OrderStatus::PartiallyFilled;
    }
    
    // Handle IOC and FOK orders
    if (order->type == OrderType::IOC && !order->isFilled()) {
        order->status = OrderStatus::Cancelled;
    } else if (order->type == OrderType::FOK && order->filledQuantity != order->quantity) {
        order->status = OrderStatus::Rejected;
        order->filledQuantity = 0;  // Rollback
    }
}

inline void OrderBook::matchMarketOrder(Order* order) {
    if (order->side == Side::Buy) {
        // Market buy order matches against asks
        while (!order->isFilled() && !asks_.empty()) {
            auto& [price, level] = *asks_.begin();
            
            while (!order->isFilled() && !level->empty()) {
                Order* maker = level->front();
                Quantity matchQty = std::min(order->remainingQuantity(), 
                                             maker->remainingQuantity());
                
                // Update level quantity BEFORE executing trade
                Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                Quantity newMakerQty = maker->remainingQuantity();
                level->updateQuantity(oldMakerQty, newMakerQty);
                
                if (maker->isFilled()) {
                    level->removeOrder(maker);
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }
            
            if (level->empty()) {
                asks_.erase(asks_.begin());
            }
        }
    } else {
        // Market sell order matches against bids
        while (!order->isFilled() && !bids_.empty()) {
            auto& [price, level] = *bids_.begin();
            
            while (!order->isFilled() && !level->empty()) {
                Order* maker = level->front();
                Quantity matchQty = std::min(order->remainingQuantity(), 
                                             maker->remainingQuantity());
                
                // Update level quantity BEFORE executing trade
                Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                Quantity newMakerQty = maker->remainingQuantity();
                level->updateQuantity(oldMakerQty, newMakerQty);
                
                if (maker->isFilled()) {
                    level->removeOrder(maker);
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }
            
            if (level->empty()) {
                bids_.erase(bids_.begin());
            }
        }
    }
    
    order->status = order->isFilled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
}

inline void OrderBook::addToBook(Order* order) {
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it == bids_.end()) {
            auto level = std::make_unique<PriceLevel>(order->price);
            level->addOrder(order);
            bids_[order->price] = std::move(level);
        } else {
            it->second->addOrder(order);
        }
    } else {
        auto it = asks_.find(order->price);
        if (it == asks_.end()) {
            auto level = std::make_unique<PriceLevel>(order->price);
            level->addOrder(order);
            asks_[order->price] = std::move(level);
        } else {
            it->second->addOrder(order);
        }
    }
}

inline void OrderBook::removeFromBook(Order* order) {
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it != bids_.end()) {
            it->second->removeOrder(order);
            if (it->second->empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(order->price);
        if (it != asks_.end()) {
            it->second->removeOrder(order);
            if (it->second->empty()) {
                asks_.erase(it);
            }
        }
    }
}

inline void OrderBook::executeTrade(Order* maker, Order* taker, Quantity quantity) {
    maker->filledQuantity += quantity;
    taker->filledQuantity += quantity;
    
    Trade trade(maker->id, taker->id, maker->price, quantity, now());
    notifyTrade(trade);
}

inline void OrderBook::notifyTrade(const Trade& trade) {
    pendingTrades_.push_back(trade);
}

inline void OrderBook::notifyOrderUpdate(const Order& order) {
    pendingUpdates_.push_back(order);
}

inline Timestamp OrderBook::now() const {
    return std::chrono::duration_cast<Timestamp>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    );
}

} // namespace orderbook
