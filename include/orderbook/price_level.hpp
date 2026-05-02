#pragma once

#include "types.hpp"
#include <boost/intrusive/list.hpp>

namespace orderbook {

// Convenience alias for the member-hook option used by PriceLevel's FIFO queue.
// Using member_hook<> (rather than base_hook<>) keeps the hook inside the Order
// struct without requiring Order to inherit from a hook base class.
using PriceQueueMemberHook = boost::intrusive::member_hook<
    Order,
    boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
    &Order::priceQueueHook>;

// Price level containing all resting orders at the same price.
// Orders are maintained in arrival (FIFO) order for price-time priority.
//
// Key performance property: boost::intrusive::list stores its link nodes
// *inside* the Order objects (via priceQueueHook), so:
//   • addOrder   — O(1) push_back, zero allocation
//   • removeOrder — O(1) via iterator_to(), no list scan
//   • front()    — O(1) pointer access
// Compare to std::list<Order*> where removeOrder required an O(n) scan.
class PriceLevel {
  public:
    explicit PriceLevel(Price price)
        : price_(price)
        , totalQuantity_(0) {}

    // Intrusive lists do not own their elements, so no special destructor logic
    // is required — the Orders live (and die) in the MemoryPool.
    ~PriceLevel() = default;

    // Non-copyable (intrusive containers are non-copyable by design)
    PriceLevel(const PriceLevel &) = delete;
    PriceLevel &operator=(const PriceLevel &) = delete;

    void addOrder(Order *order) {
        orders_.push_back(*order);
        totalQuantity_ += order->remainingQuantity();
    }

    // O(1) removal: iterator_to() converts an Order* to the list iterator in
    // constant time by computing the hook offset within the Order struct.
    void removeOrder(Order *order) {
        orders_.erase(orders_.iterator_to(*order));
        totalQuantity_ -= order->remainingQuantity();
    }

    void updateQuantity(Quantity oldQty, Quantity newQty) {
        totalQuantity_ = totalQuantity_ - oldQty + newQty;
    }

    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Quantity totalQuantity() const noexcept { return totalQuantity_; }
    [[nodiscard]] size_t orderCount() const noexcept { return orders_.size(); }
    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }

    [[nodiscard]] Order *front() const {
        return orders_.empty() ? nullptr : const_cast<Order *>(&orders_.front());
    }

  private:
    Price price_;
    Quantity totalQuantity_;
    // Intrusive FIFO queue — elements are Order objects stored in the MemoryPool.
    // The list itself is just a head/tail pointer pair; no separate nodes are
    // allocated, making it significantly more cache-friendly than std::list.
    boost::intrusive::list<Order, PriceQueueMemberHook> orders_;
};

}  // namespace orderbook
