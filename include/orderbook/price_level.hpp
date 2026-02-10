#pragma once

#include "order.hpp"
#include <cstddef>

namespace orderbook {

// Price level maintaining a doubly-linked list of orders at the same price
class PriceLevel {
public:
    PriceLevel() = default;
    
    // Add order to end of price level (FIFO - price-time priority)
    void add_order(Order* order) noexcept {
        if (!order) return;
        
        order->next = nullptr;
        order->prev = tail_;
        
        if (tail_) {
            tail_->next = order;
        }
        tail_ = order;
        
        if (!head_) {
            head_ = order;
        }
        
        ++order_count_;
        total_quantity_ += order->remaining_quantity();
    }
    
    // Remove specific order from price level
    void remove_order(Order* order) noexcept {
        if (!order) return;
        
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head_ = order->next;
        }
        
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail_ = order->prev;
        }
        
        total_quantity_ -= order->remaining_quantity();
        --order_count_;
        
        order->next = nullptr;
        order->prev = nullptr;
    }
    
    // Update quantity after partial fill
    void update_quantity(Quantity old_remaining, Quantity new_remaining) noexcept {
        total_quantity_ = total_quantity_ - old_remaining + new_remaining;
    }
    
    [[nodiscard]] Order* head() const noexcept {
        return head_;
    }
    
    [[nodiscard]] Order* tail() const noexcept {
        return tail_;
    }
    
    [[nodiscard]] bool empty() const noexcept {
        return head_ == nullptr;
    }
    
    [[nodiscard]] size_t order_count() const noexcept {
        return order_count_;
    }
    
    [[nodiscard]] Quantity total_quantity() const noexcept {
        return total_quantity_;
    }
    
private:
    Order* head_{nullptr};
    Order* tail_{nullptr};
    size_t order_count_{0};
    Quantity total_quantity_{0};
};

} // namespace orderbook
