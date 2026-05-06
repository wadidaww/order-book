#pragma once

#include "cache.hpp"
#include <cstdint>
#include <string>
#include <chrono>

namespace orderbook {

// Type aliases for better readability and easy type-width changes
using OrderId = uint64_t;
using Price = int64_t;  // Fixed-point integer scaled by PRICE_SCALE (10 000)
using Quantity = uint64_t;
using Timestamp = std::chrono::nanoseconds;

// Fixed-point scale factor: 1 Price unit = 1/PRICE_SCALE of a currency unit.
// Example: Price 100'0000 represents 100.0000; Price 99'5500 represents 99.55.
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

// Order struct optimised to fit within a single cache line (verified by the
// static_assert below).  Hot fields (price, quantity, side, type, status) are
// grouped first so that the matching engine's read-heavy access pattern stays
// within the first 32 bytes.
struct Order {
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
// engine never splits a hot Order across two cache lines.  If this fires,
// re-examine the struct layout (reorder or remove fields).
static_assert(sizeof(Order) <= detail::constructive_interference_size,
              "Order exceeds one cache line — re-examine the struct layout");

// Snapshot of a single price level returned by getBids() / getAsks().
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
