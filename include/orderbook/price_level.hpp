#pragma once

#include "types.hpp"
#include <list>
#include <memory>

namespace orderbook {

// Price level containing orders at the same price
// Orders are maintained in FIFO order for price-time priority
class PriceLevel {
public:
    explicit PriceLevel(Price price) : price_(price), total_quantity_(0) {}
    
    void add_order(Order* order) {
        orders_.push_back(order);
        total_quantity_ += order->remaining_quantity();
    }
    
    void remove_order(Order* order) {
        orders_.remove(order);
        total_quantity_ -= order->remaining_quantity();
    }
    
    void update_quantity(Quantity old_qty, Quantity new_qty) {
        total_quantity_ = total_quantity_ - old_qty + new_qty;
    }
    
    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Quantity total_quantity() const noexcept { return total_quantity_; }
    [[nodiscard]] size_t order_count() const noexcept { return orders_.size(); }
    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }
    
    [[nodiscard]] const std::list<Order*>& orders() const noexcept { return orders_; }
    [[nodiscard]] std::list<Order*>& orders() noexcept { return orders_; }
    
    [[nodiscard]] Order* front() const {
        return orders_.empty() ? nullptr : orders_.front();
    }

private:
    Price price_;
    Quantity total_quantity_;
    std::list<Order*> orders_;  // FIFO queue for price-time priority
};

} // namespace orderbook
