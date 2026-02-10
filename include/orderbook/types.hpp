#pragma once

#include <cstdint>
#include <chrono>
#include <string_view>
#include <compare>

namespace orderbook {

// Type aliases for clarity and potential future optimization
using OrderId = uint64_t;
using Price = int64_t;  // Fixed-point representation (e.g., price * 10000)
using Quantity = uint64_t;
using Timestamp = std::chrono::nanoseconds;
using SymbolId = uint32_t;

// Order side
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

// Order type
enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1,
    StopLoss = 2,
    StopLimit = 3
};

// Order status
enum class OrderStatus : uint8_t {
    Pending = 0,
    Active = 1,
    PartiallyFilled = 2,
    Filled = 3,
    Cancelled = 4,
    Rejected = 5
};

// Time in force
enum class TimeInForce : uint8_t {
    GoodTillCancel = 0,
    ImmediateOrCancel = 1,
    FillOrKill = 2,
    GoodTillDay = 3
};

// Execution type for trade callbacks
enum class ExecutionType : uint8_t {
    New = 0,
    PartialFill = 1,
    Fill = 2,
    Cancelled = 3,
    Replaced = 4,
    Rejected = 5
};

// Convert side to string
[[nodiscard]] constexpr std::string_view to_string(Side side) noexcept {
    switch (side) {
        case Side::Buy: return "Buy";
        case Side::Sell: return "Sell";
    }
    return "Unknown";
}

// Convert order type to string
[[nodiscard]] constexpr std::string_view to_string(OrderType type) noexcept {
    switch (type) {
        case OrderType::Limit: return "Limit";
        case OrderType::Market: return "Market";
        case OrderType::StopLoss: return "StopLoss";
        case OrderType::StopLimit: return "StopLimit";
    }
    return "Unknown";
}

// Convert order status to string
[[nodiscard]] constexpr std::string_view to_string(OrderStatus status) noexcept {
    switch (status) {
        case OrderStatus::Pending: return "Pending";
        case OrderStatus::Active: return "Active";
        case OrderStatus::PartiallyFilled: return "PartiallyFilled";
        case OrderStatus::Filled: return "Filled";
        case OrderStatus::Cancelled: return "Cancelled";
        case OrderStatus::Rejected: return "Rejected";
    }
    return "Unknown";
}

// Convert execution type to string
[[nodiscard]] constexpr std::string_view to_string(ExecutionType exec_type) noexcept {
    switch (exec_type) {
        case ExecutionType::New: return "New";
        case ExecutionType::PartialFill: return "PartialFill";
        case ExecutionType::Fill: return "Fill";
        case ExecutionType::Cancelled: return "Cancelled";
        case ExecutionType::Replaced: return "Replaced";
        case ExecutionType::Rejected: return "Rejected";
    }
    return "Unknown";
}

// Price level key with comparison optimized for map lookups
struct PriceLevelKey {
    Price price;
    Side side;

    [[nodiscard]] auto operator<=>(const PriceLevelKey&) const noexcept = default;
};

// Trade execution information
struct Trade {
    OrderId buyer_order_id;
    OrderId seller_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    SymbolId symbol_id;
};

// Order execution report
struct ExecutionReport {
    OrderId order_id;
    SymbolId symbol_id;
    ExecutionType exec_type;
    OrderStatus order_status;
    Side side;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    Quantity remaining_quantity;
    Timestamp timestamp;
};

} // namespace orderbook
