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

    // Non-copyable, non-movable
    ConcurrentOrderBook(const ConcurrentOrderBook &) = delete;
    ConcurrentOrderBook &operator=(const ConcurrentOrderBook &) = delete;
    ConcurrentOrderBook(ConcurrentOrderBook &&) = delete;
    ConcurrentOrderBook &operator=(ConcurrentOrderBook &&) = delete;

    // Submit requests asynchronously. Thread-safe. Returns a future that
    // resolves to true on success, false on failure (e.g., duplicate ID).
    [[nodiscard]] std::future<bool> submitAddOrder(OrderId id, Price price, Quantity quantity,
                                                   Side side, OrderType type = OrderType::Limit);

    [[nodiscard]] std::future<bool> submitCancelOrder(OrderId id);

    [[nodiscard]] std::future<bool> submitModifyOrder(OrderId id, Price newPrice,
                                                      Quantity newQuantity);

    // Read-only access — delegates to the underlying OrderBook (thread-safe
    // via the shared_mutex inside OrderBook).
    [[nodiscard]] std::optional<Order> getOrder(OrderId id) const { return book_.getOrder(id); }
    [[nodiscard]] std::optional<Price> bestBid() const { return book_.bestBid(); }
    [[nodiscard]] std::optional<Price> bestAsk() const { return book_.bestAsk(); }
    [[nodiscard]] std::optional<Price> spread() const { return book_.spread(); }
    [[nodiscard]] std::optional<Price> midPrice() const { return book_.midPrice(); }
    [[nodiscard]] std::vector<LevelInfo> getBids(size_t depth = 10) const {
        return book_.getBids(depth);
    }
    [[nodiscard]] std::vector<LevelInfo> getAsks(size_t depth = 10) const {
        return book_.getAsks(depth);
    }
    [[nodiscard]] size_t orderCount() const { return book_.orderCount(); }
    [[nodiscard]] size_t bidLevelCount() const { return book_.bidLevelCount(); }
    [[nodiscard]] size_t askLevelCount() const { return book_.askLevelCount(); }

    // Callbacks are forwarded to the underlying OrderBook and will be invoked
    // on the worker thread. Set them before submitting any orders.
    void setTradeCallback(TradeCallback callback) { book_.setTradeCallback(std::move(callback)); }
    void setOrderUpdateCallback(OrderUpdateCallback callback) {
        book_.setOrderUpdateCallback(std::move(callback));
    }

  private:
    // Internal command types
    struct Command {
        enum class Type {
            AddOrder,
            CancelOrder,
            ModifyOrder,
            Stop
        };

        Type type{Type::Stop};
        OrderId id{0};
        Price price{0};
        Quantity quantity{0};
        Side side{Side::Buy};
        OrderType orderType{OrderType::Limit};
        std::promise<bool> result;
    };

    void enqueue(std::unique_ptr<Command> cmd);
    void processCommands();

    OrderBook book_;

    std::queue<std::unique_ptr<Command>> queue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::thread worker_;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

inline ConcurrentOrderBook::ConcurrentOrderBook()
    : worker_(&ConcurrentOrderBook::processCommands, this) {}

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
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push(std::move(cmd));
    }
    queueCv_.notify_one();
}

inline std::future<bool> ConcurrentOrderBook::submitAddOrder(OrderId id, Price price,
                                                             Quantity quantity, Side side,
                                                             OrderType type) {
    auto cmd = std::make_unique<Command>();
    cmd->type = Command::Type::AddOrder;
    cmd->id = id;
    cmd->price = price;
    cmd->quantity = quantity;
    cmd->side = side;
    cmd->orderType = type;
    auto future = cmd->result.get_future();
    enqueue(std::move(cmd));
    return future;
}

inline std::future<bool> ConcurrentOrderBook::submitCancelOrder(OrderId id) {
    auto cmd = std::make_unique<Command>();
    cmd->type = Command::Type::CancelOrder;
    cmd->id = id;
    auto future = cmd->result.get_future();
    enqueue(std::move(cmd));
    return future;
}

inline std::future<bool> ConcurrentOrderBook::submitModifyOrder(OrderId id, Price newPrice,
                                                                Quantity newQuantity) {
    auto cmd = std::make_unique<Command>();
    cmd->type = Command::Type::ModifyOrder;
    cmd->id = id;
    cmd->price = newPrice;
    cmd->quantity = newQuantity;
    auto future = cmd->result.get_future();
    enqueue(std::move(cmd));
    return future;
}

inline void ConcurrentOrderBook::processCommands() {
    while (true) {
        std::unique_ptr<Command> cmd;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] { return !queue_.empty(); });
            cmd = std::move(queue_.front());
            queue_.pop();
        }

        if (cmd->type == Command::Type::Stop) {
            break;
        }

        bool result = false;
        switch (cmd->type) {
        case Command::Type::AddOrder:
            result = book_.addOrder(cmd->id, cmd->price, cmd->quantity, cmd->side, cmd->orderType);
            break;
        case Command::Type::CancelOrder:
            result = book_.cancelOrder(cmd->id);
            break;
        case Command::Type::ModifyOrder:
            result = book_.modifyOrder(cmd->id, cmd->price, cmd->quantity);
            break;
        default:
            break;
        }

        try {
            cmd->result.set_value(result);
        } catch (const std::future_error &) {
            // set_value should never fail here, but guard the worker thread
            // against an unexpected exception so it continues processing.
        }
    }
}

}  // namespace orderbook
