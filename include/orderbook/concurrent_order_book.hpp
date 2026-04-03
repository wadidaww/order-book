#pragma once

#include "order_book.hpp"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>

namespace orderbook {

// Thread-safe wrapper around OrderBook that accepts requests from multiple
// concurrent producers and processes them sequentially on a dedicated worker
// thread, eliminating lock contention on the critical matching path.
//
// Usage:
//   ConcurrentOrderBook book;
//   book.set_trade_callback(...);          // set before submitting orders
//   auto f = book.submit_add_order(...);   // returns std::future<bool>
//   bool ok = f.get();                     // block until processed
class ConcurrentOrderBook {
public:
    ConcurrentOrderBook();
    ~ConcurrentOrderBook();

    // Non-copyable
    ConcurrentOrderBook(const ConcurrentOrderBook&) = delete;
    ConcurrentOrderBook& operator=(const ConcurrentOrderBook&) = delete;

    // Submit requests asynchronously. Thread-safe. Returns a future that
    // resolves to true on success, false on failure (e.g., duplicate ID).
    [[nodiscard]] std::future<bool> submit_add_order(
        OrderId id, Price price, Quantity quantity,
        Side side, OrderType type = OrderType::Limit);

    [[nodiscard]] std::future<bool> submit_cancel_order(OrderId id);

    [[nodiscard]] std::future<bool> submit_modify_order(
        OrderId id, Price new_price, Quantity new_quantity);

    // Read-only access — delegates to the underlying OrderBook (thread-safe
    // via the shared_mutex inside OrderBook).
    [[nodiscard]] std::optional<Order>  get_order(OrderId id)        const { return book_.get_order(id); }
    [[nodiscard]] std::optional<Price>  best_bid()                   const { return book_.best_bid(); }
    [[nodiscard]] std::optional<Price>  best_ask()                   const { return book_.best_ask(); }
    [[nodiscard]] std::optional<Price>  spread()                     const { return book_.spread(); }
    [[nodiscard]] std::optional<Price>  mid_price()                  const { return book_.mid_price(); }
    [[nodiscard]] std::vector<LevelInfo> get_bids(size_t depth = 10) const { return book_.get_bids(depth); }
    [[nodiscard]] std::vector<LevelInfo> get_asks(size_t depth = 10) const { return book_.get_asks(depth); }
    [[nodiscard]] size_t order_count()                               const { return book_.order_count(); }
    [[nodiscard]] size_t bid_level_count()                           const { return book_.bid_level_count(); }
    [[nodiscard]] size_t ask_level_count()                           const { return book_.ask_level_count(); }

    // Callbacks are forwarded to the underlying OrderBook and will be invoked
    // on the worker thread. Set them before submitting any orders.
    void set_trade_callback(TradeCallback callback) {
        book_.set_trade_callback(std::move(callback));
    }
    void set_order_update_callback(OrderUpdateCallback callback) {
        book_.set_order_update_callback(std::move(callback));
    }

private:
    // Internal command types
    struct Command {
        enum class Type { AddOrder, CancelOrder, ModifyOrder, Stop };

        Type     type{Type::Stop};
        OrderId  id{0};
        Price    price{0};
        Quantity quantity{0};
        Side     side{Side::Buy};
        OrderType order_type{OrderType::Limit};
        std::promise<bool> result;
    };

    void enqueue(std::unique_ptr<Command> cmd);
    void process_commands();

    OrderBook book_;

    std::queue<std::unique_ptr<Command>> queue_;
    std::mutex                           queue_mutex_;
    std::condition_variable              queue_cv_;
    std::thread                          worker_;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

inline ConcurrentOrderBook::ConcurrentOrderBook()
    : worker_(&ConcurrentOrderBook::process_commands, this) {}

inline ConcurrentOrderBook::~ConcurrentOrderBook() {
    // Send a Stop sentinel so the worker exits cleanly
    auto stop = std::make_unique<Command>();
    stop->type = Command::Type::Stop;
    enqueue(std::move(stop));
    if (worker_.joinable()) {
        worker_.join();
    }
}

inline void ConcurrentOrderBook::enqueue(std::unique_ptr<Command> cmd) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(cmd));
    }
    queue_cv_.notify_one();
}

inline std::future<bool> ConcurrentOrderBook::submit_add_order(
    OrderId id, Price price, Quantity quantity, Side side, OrderType type) {
    auto cmd = std::make_unique<Command>();
    cmd->type       = Command::Type::AddOrder;
    cmd->id         = id;
    cmd->price      = price;
    cmd->quantity   = quantity;
    cmd->side       = side;
    cmd->order_type = type;
    auto future = cmd->result.get_future();
    enqueue(std::move(cmd));
    return future;
}

inline std::future<bool> ConcurrentOrderBook::submit_cancel_order(OrderId id) {
    auto cmd = std::make_unique<Command>();
    cmd->type = Command::Type::CancelOrder;
    cmd->id   = id;
    auto future = cmd->result.get_future();
    enqueue(std::move(cmd));
    return future;
}

inline std::future<bool> ConcurrentOrderBook::submit_modify_order(
    OrderId id, Price new_price, Quantity new_quantity) {
    auto cmd = std::make_unique<Command>();
    cmd->type     = Command::Type::ModifyOrder;
    cmd->id       = id;
    cmd->price    = new_price;
    cmd->quantity = new_quantity;
    auto future = cmd->result.get_future();
    enqueue(std::move(cmd));
    return future;
}

inline void ConcurrentOrderBook::process_commands() {
    while (true) {
        std::unique_ptr<Command> cmd;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !queue_.empty(); });
            cmd = std::move(queue_.front());
            queue_.pop();
        }

        if (cmd->type == Command::Type::Stop) {
            break;
        }

        bool result = false;
        switch (cmd->type) {
            case Command::Type::AddOrder:
                result = book_.add_order(
                    cmd->id, cmd->price, cmd->quantity,
                    cmd->side, cmd->order_type);
                break;
            case Command::Type::CancelOrder:
                result = book_.cancel_order(cmd->id);
                break;
            case Command::Type::ModifyOrder:
                result = book_.modify_order(cmd->id, cmd->price, cmd->quantity);
                break;
            default:
                break;
        }

        try {
            cmd->result.set_value(result);
        } catch (const std::future_error&) {
            // set_value should never fail here, but guard the worker thread
            // against an unexpected exception so it continues processing.
        }
    }
}

} // namespace orderbook
