#pragma once

#include <cstdint>
#include <string>
#include <chrono>

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

// Order structure optimized for cache locality
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
