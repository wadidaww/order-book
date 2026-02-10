#pragma once

#include "types.hpp"
#include <memory>
#include <atomic>

namespace orderbook {

// Cache-line aligned order structure for optimal memory access
struct alignas(64) Order {
    OrderId order_id{0};
    SymbolId symbol_id{0};
    Price price{0};
    Quantity quantity{0};
    Quantity filled_quantity{0};
    Timestamp timestamp{0};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    OrderStatus status{OrderStatus::Pending};
    TimeInForce time_in_force{TimeInForce::GoodTillCancel};
    
    // Intrusive linked list pointers for price level
    Order* next{nullptr};
    Order* prev{nullptr};
    
    [[nodiscard]] Quantity remaining_quantity() const noexcept {
        return quantity - filled_quantity;
    }
    
    [[nodiscard]] bool is_fillable() const noexcept {
        return status == OrderStatus::Active || status == OrderStatus::PartiallyFilled;
    }
    
    [[nodiscard]] bool is_buy() const noexcept {
        return side == Side::Buy;
    }
    
    [[nodiscard]] bool is_sell() const noexcept {
        return side == Side::Sell;
    }
    
    void reset() noexcept {
        order_id = 0;
        symbol_id = 0;
        price = 0;
        quantity = 0;
        filled_quantity = 0;
        timestamp = Timestamp{0};
        side = Side::Buy;
        type = OrderType::Limit;
        status = OrderStatus::Pending;
        time_in_force = TimeInForce::GoodTillCancel;
        next = nullptr;
        prev = nullptr;
    }
};

// Order pool for zero-allocation order management
class OrderPool {
public:
    explicit OrderPool(size_t initial_capacity = 10000)
        : capacity_(initial_capacity) {
        orders_.reserve(capacity_);
        for (size_t i = 0; i < capacity_; ++i) {
            orders_.emplace_back(std::make_unique<Order>());
            free_list_.push_back(orders_.back().get());
        }
    }
    
    // Non-copyable, non-movable for thread safety
    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;
    OrderPool(OrderPool&&) = delete;
    OrderPool& operator=(OrderPool&&) = delete;
    
    [[nodiscard]] Order* acquire() {
        if (free_list_.empty()) {
            grow();
        }
        Order* order = free_list_.back();
        free_list_.pop_back();
        order->reset();
        return order;
    }
    
    void release(Order* order) noexcept {
        if (order) {
            order->reset();
            free_list_.push_back(order);
        }
    }
    
    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }
    
    [[nodiscard]] size_t available() const noexcept {
        return free_list_.size();
    }
    
private:
    void grow() {
        const size_t old_capacity = capacity_;
        capacity_ = capacity_ * 2;
        orders_.reserve(capacity_);
        for (size_t i = old_capacity; i < capacity_; ++i) {
            orders_.emplace_back(std::make_unique<Order>());
            free_list_.push_back(orders_.back().get());
        }
    }
    
    size_t capacity_;
    std::vector<std::unique_ptr<Order>> orders_;
    std::vector<Order*> free_list_;
};

} // namespace orderbook
