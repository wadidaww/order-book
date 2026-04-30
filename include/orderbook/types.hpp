#pragma once

#include "cache.hpp"
#include <cstdint>
#include <string>
#include <chrono>
#include <boost/intrusive/list.hpp>

namespace orderbook {

// Type aliases for better readability and easy modifications
using OrderId = uint64_t;
using Price = int64_t;  // Fixed-point representation (scaled by 10000)
using Quantity = uint64_t;
using Timestamp = std::chrono::nanoseconds;

// Price scale factor for fixed-point arithmetic
constexpr int64_t PRICE_SCALE = 10000;

// Order side
enum class Side : uint8_t {
    Buy,
    Sell
};

// Order type
enum class OrderType : uint8_t {
    Limit,
    Market,
    IOC,  // Immediate or Cancel
    FOK   // Fill or Kill
};

// Order status
enum class OrderStatus : uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

// Trade information
struct Trade {
    OrderId makerOrderId;
    OrderId takerOrderId;
    Price price;
    Quantity quantity;
    Timestamp timestamp;

    Trade(OrderId maker, OrderId taker, Price p, Quantity q, Timestamp ts)
        : makerOrderId(maker)
        , takerOrderId(taker)
        , price(p)
        , quantity(q)
        , timestamp(ts) {}
};

// Order structure optimized for cache locality.
//
// The priceQueueHook embeds the doubly-linked-list pointers required by
// boost::intrusive::list directly inside the Order, avoiding a separate
// list-node allocation and enabling O(1) removal (via iterator_to) instead
// of the O(n) scan that std::list::remove would require.
//
// normal_link mode is chosen deliberately:
//   • It stores only the two raw prev/next pointers (16 bytes), keeping the
//     struct within one 64-byte cache line.
//   • Unlike safe_link it does NOT assert "is the hook still linked?" at
//     destruction time, which lets the MemoryPool::clear() fast-path reset
//     memory without walking every live order.
struct Order {
    // Intrusive-list hook — must be the FIRST non-trivial member so that
    // iterator_to() resolves to a zero-byte offset on common ABIs, keeping
    // the hot-path pointer arithmetic as cheap as possible.
    boost::intrusive::list_member_hook<
        boost::intrusive::link_mode<boost::intrusive::normal_link>>
        priceQueueHook;

    OrderId id;
    Price price;
    Quantity quantity;
    Quantity filledQuantity{0};
    Side side;
    OrderType type;
    OrderStatus status{OrderStatus::New};
    Timestamp timestamp;

    Order(OrderId orderId, Price p, Quantity q, Side s, OrderType t, Timestamp ts)
        : id(orderId)
        , price(p)
        , quantity(q)
        , side(s)
        , type(t)
        , timestamp(ts) {}

    [[nodiscard]] Quantity remainingQuantity() const noexcept { return quantity - filledQuantity; }

    [[nodiscard]] bool isFilled() const noexcept { return filledQuantity >= quantity; }
};

// Verify the entire Order fits within a single cache line so that the matching
// engine never splits a hot Order across two cache lines.
// Layout (64-bit ABI):
//   priceQueueHook  16 B  (2 × void* in normal_link mode)
//   id               8 B
//   price            8 B
//   quantity         8 B
//   filledQuantity   8 B
//   side+type+status 3 B  + 5 B padding
//   timestamp        8 B
//   ─────────────── 64 B  exactly one cache line
static_assert(sizeof(Order) <= detail::constructive_interference_size,
              "Order exceeds one cache line — re-examine the struct layout");

// Price level statistics
struct LevelInfo {
    Price price;
    Quantity quantity;
    size_t orderCount;

    LevelInfo(Price p, Quantity q, size_t count)
        : price(p)
        , quantity(q)
        , orderCount(count) {}
};

}  // namespace orderbook
