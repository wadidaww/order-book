#pragma once

#include "types.hpp"
#include <list>
#include <memory>

namespace orderbook {

// Price level containing orders at the same price
// Orders are maintained in FIFO order for price-time priority
class PriceLevel {
public:
    explicit PriceLevel(Price price) : price_(price), totalQuantity_(0) {}
    
    void addOrder(Order* order) {
        orders_.push_back(order);
        totalQuantity_ += order->remainingQuantity();
    }
    
    void removeOrder(Order* order) {
        orders_.remove(order);
        totalQuantity_ -= order->remainingQuantity();
    }
    
    void updateQuantity(Quantity oldQty, Quantity newQty) {
        totalQuantity_ = totalQuantity_ - oldQty + newQty;
    }
    
    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Quantity totalQuantity() const noexcept { return totalQuantity_; }
    [[nodiscard]] size_t orderCount() const noexcept { return orders_.size(); }
    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }
    
    [[nodiscard]] const std::list<Order*>& orders() const noexcept { return orders_; }
    [[nodiscard]] std::list<Order*>& orders() noexcept { return orders_; }
    
    [[nodiscard]] Order* front() const {
        return orders_.empty() ? nullptr : orders_.front();
    }

private:
    Price price_;
    Quantity totalQuantity_;
    std::list<Order*> orders_;  // FIFO queue for price-time priority
};

} // namespace orderbook
