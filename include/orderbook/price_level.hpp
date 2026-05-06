#pragma once

#include "types.hpp"
#include <list>
#include <memory>

namespace orderbook {

// A single price level in the standard OrderBook.
//
// Holds all resting orders at a given price in arrival (FIFO) order using a
// std::list.  Tracks the aggregate quantity across all live orders so that
// market-depth queries do not need to iterate the order list.
//
// Used by OrderBook; PriceCollarOrderBook uses its own internal Level struct
// backed by DynamicCircularQueue instead.
class PriceLevel {
  public:
    explicit PriceLevel(Price price)
        : price_(price)
        , totalQuantity_(0) {}

    // Append an order to the back of the FIFO queue and update the aggregate.
    void addOrder(Order *order) {
        orders_.push_back(order);
        totalQuantity_ += order->remainingQuantity();
    }

    // Remove an arbitrary order from the level and update the aggregate.
    // The caller must ensure the order actually belongs to this level.
    void removeOrder(Order *order) {
        orders_.remove(order);
        totalQuantity_ -= order->remainingQuantity();
    }

    // Update the aggregate quantity after a partial fill or modify.
    // oldQty is the quantity before the change; newQty is the quantity after.
    void updateQuantity(Quantity oldQty, Quantity newQty) {
        totalQuantity_ = totalQuantity_ - oldQty + newQty;
    }

    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Quantity totalQuantity() const noexcept { return totalQuantity_; }
    [[nodiscard]] size_t orderCount() const noexcept { return orders_.size(); }
    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }

    [[nodiscard]] const std::list<Order *> &orders() const noexcept { return orders_; }
    [[nodiscard]] std::list<Order *> &orders() noexcept { return orders_; }

    [[nodiscard]] Order *front() const { return orders_.empty() ? nullptr : orders_.front(); }

  private:
    Price price_;
    Quantity totalQuantity_;
    std::list<Order *> orders_;  // FIFO queue for price-time priority
};

}  // namespace orderbook
