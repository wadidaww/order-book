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
    OrderId maker_order_id;
    OrderId taker_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    
    Trade(OrderId maker, OrderId taker, Price p, Quantity q, Timestamp ts)
        : maker_order_id(maker)
        , taker_order_id(taker)
        , price(p)
        , quantity(q)
        , timestamp(ts) {}
};

// Order structure optimized for cache locality
struct Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    Side side;
    OrderType type;
    OrderStatus status;
    Timestamp timestamp;
    
    Order(OrderId order_id, Price p, Quantity q, Side s, OrderType t, Timestamp ts)
        : id(order_id)
        , price(p)
        , quantity(q)
        , filled_quantity(0)
        , side(s)
        , type(t)
        , status(OrderStatus::New)
        , timestamp(ts) {}
    
    [[nodiscard]] Quantity remaining_quantity() const noexcept {
        return quantity - filled_quantity;
    }
    
    [[nodiscard]] bool is_filled() const noexcept {
        return filled_quantity >= quantity;
    }
};

// Price level statistics
struct LevelInfo {
    Price price;
    Quantity quantity;
    size_t order_count;
    
    LevelInfo(Price p, Quantity q, size_t count)
        : price(p), quantity(q), order_count(count) {}
};

} // namespace orderbook
