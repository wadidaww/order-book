#pragma once

#include "order_book.hpp"
#include "cache.hpp"
#include "../data_structures/mpsc_ring_buffer.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <future>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace orderbook {

// Thread-safe wrapper around OrderBook that accepts requests from multiple
// concurrent producers and processes them sequentially on a dedicated worker
// thread.  Matching semantics and price-time priority are exactly those of
// OrderBook, because every mutation still runs on the single worker.
//
// Lock-free hot paths:
//   • Submission: producers write commands straight into a bounded MPSC ring
//     buffer (ds::MpscRingBuffer) with one CAS — no mutex, no per-command heap
//     allocation for the command itself.  The worker spins briefly when idle
//     and then parks on an atomic wait, so producers only pay for a wake-up
//     when the worker is actually asleep.
//   • Market data: after each batch of commands the worker publishes the top
//     kPublishedDepth levels per side, best bid/ask and counts into a seqlock
//     snapshot.  bestBid/bestAsk/spread/midPrice/getBids/getAsks/counts read
//     that snapshot without taking any lock, so readers never stall the
//     matching thread (the old shared_mutex let heavy readers starve it).
//
// Visibility guarantee: a command's future resolves only after the snapshot
// that includes the command has been published, so a thread that waited on
// a future always observes its own write through the read API.
//
// Usage:
//   ConcurrentOrderBook book;
//   book.setTradeCallback(...);            // set before submitting orders
//   auto f = book.submitAddOrder(...);     // returns std::future<bool>
//   bool ok = f.get();                     // block until processed
class ConcurrentOrderBook {
  public:
    // Number of levels per side served lock-free by getBids()/getAsks().
    // Deeper requests fall back to a shared-locked read of the live book.
    static constexpr size_t kPublishedDepth = 10;
    static constexpr size_t kDefaultQueueCapacity = 1 << 14;

    explicit ConcurrentOrderBook(size_t queueCapacity = kDefaultQueueCapacity);
    ~ConcurrentOrderBook();

    // Non-copyable, non-movable
    ConcurrentOrderBook(const ConcurrentOrderBook &) = delete;
    ConcurrentOrderBook &operator=(const ConcurrentOrderBook &) = delete;
    ConcurrentOrderBook(ConcurrentOrderBook &&) = delete;
    ConcurrentOrderBook &operator=(ConcurrentOrderBook &&) = delete;

    // Submit requests asynchronously. Thread-safe. Returns a future that
    // resolves to true on success, false on failure (e.g., duplicate ID).
    // If the command queue is full the call waits for space.
    [[nodiscard]] std::future<bool> submitAddOrder(OrderId id, Price price, Quantity quantity,
                                                   Side side, OrderType type = OrderType::Limit);

    [[nodiscard]] std::future<bool> submitCancelOrder(OrderId id);

    [[nodiscard]] std::future<bool> submitModifyOrder(OrderId id, Price newPrice,
                                                      Quantity newQuantity);

    // Fire-and-forget variants: no future, so no std::promise allocation or
    // cross-thread future hand-off — the cheapest way to feed the book.
    // Outcomes are visible through the trade / order-update callbacks and the
    // read API.  Commands from one thread are applied in submission order, so
    // a later submit*() future from the same thread doubles as a barrier.
    void postAddOrder(OrderId id, Price price, Quantity quantity, Side side,
                      OrderType type = OrderType::Limit);
    void postCancelOrder(OrderId id);
    void postModifyOrder(OrderId id, Price newPrice, Quantity newQuantity);

    // Per-order lookup reads the live book under its shared lock.
    [[nodiscard]] std::optional<Order> getOrder(OrderId id) const { return book_.getOrder(id); }

    // Lock-free market data (see class comment).  When called from a
    // callback running on the worker thread these read the live book instead,
    // so callbacks see the state produced by the command that triggered them.
    [[nodiscard]] std::optional<Price> bestBid() const;
    [[nodiscard]] std::optional<Price> bestAsk() const;
    [[nodiscard]] std::optional<Price> spread() const;
    [[nodiscard]] std::optional<Price> midPrice() const;
    [[nodiscard]] std::vector<LevelInfo> getBids(size_t depth = 10) const;
    [[nodiscard]] std::vector<LevelInfo> getAsks(size_t depth = 10) const;
    [[nodiscard]] size_t orderCount() const;
    [[nodiscard]] size_t bidLevelCount() const;
    [[nodiscard]] size_t askLevelCount() const;

    // Callbacks are forwarded to the underlying OrderBook and will be invoked
    // on the internal worker thread.  They must be set **before** submitting
    // any orders to avoid missing early notifications.
    void setTradeCallback(TradeCallback callback) { book_.setTradeCallback(std::move(callback)); }
    void setOrderUpdateCallback(OrderUpdateCallback callback) {
        book_.setOrderUpdateCallback(std::move(callback));
    }

  private:
    // Internal command, constructed in place inside a ring-buffer slot.
    // The promise lives in manually managed storage so that fire-and-forget
    // commands (post*) carry none: a default-constructed std::promise would
    // heap-allocate its shared state.  Field order keeps the command at 56
    // bytes so slot + sequence number fit one cache line.
    struct Command {
        enum class Type : uint8_t {
            AddOrder,
            CancelOrder,
            ModifyOrder,
            Stop
        };

        Command(Type t, OrderId i, Price p, Quantity q, Side s, OrderType ot, std::promise<bool> *r)
            : id(i)
            , price(p)
            , quantity(q)
            , type(t)
            , side(s)
            , orderType(ot)
            , hasResult(r != nullptr) {
            if (r)
                ::new (resultStorage) std::promise<bool>(std::move(*r));
        }

        ~Command() {
            if (hasResult)
                result().~promise();
        }

        Command(const Command &) = delete;
        Command &operator=(const Command &) = delete;

        std::promise<bool> &result() {
            return *std::launder(reinterpret_cast<std::promise<bool> *>(resultStorage));
        }

        OrderId id;
        Price price;
        Quantity quantity;
        alignas(std::promise<bool>) unsigned char resultStorage[sizeof(std::promise<bool>)];
        Type type;
        Side side;
        OrderType orderType;
        bool hasResult;
    };

    // Seqlock-protected market-data snapshot.  Single writer (the worker);
    // any number of readers.  Every field is an atomic accessed with relaxed
    // ordering, and the version counter plus fences provide consistency:
    // an odd version means a write is in progress, and a reader retries if
    // the version changed while it was reading.
    struct LevelSlot {
        std::atomic<Price> price{0};
        std::atomic<Quantity> quantity{0};
        std::atomic<size_t> orderCount{0};
    };

    struct alignas(detail::destructive_interference_size) Snapshot {
        std::atomic<uint64_t> version{0};
        std::atomic<Price> bestBid{0};
        std::atomic<Price> bestAsk{0};
        std::atomic<size_t> orderCount{0};
        std::atomic<size_t> bidLevels{0};
        std::atomic<size_t> askLevels{0};
        std::array<LevelSlot, kPublishedDepth> bids;
        std::array<LevelSlot, kPublishedDepth> asks;
    };

    static_assert(sizeof(Command) + sizeof(std::atomic<size_t>) <=
                      detail::destructive_interference_size,
                  "Command no longer fits one cache line with its slot sequence");

    // Most commands drained before publishing one snapshot and resolving
    // their futures.  Bounds the extra latency a backlog can add.
    static constexpr size_t kMaxBatch = 64;
    // Idle worker: spin, then yield, then park on sleeping_.
    static constexpr unsigned kSpinIterations = 2048;
    static constexpr unsigned kYieldIterations = 64;

    void enqueue(Command::Type type, OrderId id, Price price, Quantity quantity, Side side,
                 OrderType orderType, std::promise<bool> *result);
    std::future<bool> enqueueWithResult(Command::Type type, OrderId id, Price price,
                                        Quantity quantity, Side side, OrderType orderType);
    void processCommands();
    bool execute(const Command &cmd);
    void publishSnapshot();

    template <typename Fn> auto readSnapshot(Fn &&fn) const;
    // Nullopt when more levels are requested than the snapshot holds.
    std::optional<std::vector<LevelInfo>>
    readLevels(const std::array<LevelSlot, kPublishedDepth> &levels,
               const std::atomic<size_t> &levelCount, size_t depth) const;
    [[nodiscard]] bool onWorkerThread() const {
        return std::this_thread::get_id() == worker_.get_id();
    }

    OrderBook book_;

    // Read-mostly by market-data threads; written once per batch by the worker.
    Snapshot snapshot_;

    // Command queue: hot between producers and the worker.  Its head and tail
    // live on their own cache lines inside MpscRingBuffer.
    ds::MpscRingBuffer<Command> queue_;

    // True while the worker is parked; producers wake it only in that case.
    alignas(detail::destructive_interference_size) std::atomic<bool> sleeping_{false};

    std::thread worker_;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

inline ConcurrentOrderBook::ConcurrentOrderBook(size_t queueCapacity)
    : queue_(queueCapacity)
    , worker_(&ConcurrentOrderBook::processCommands, this) {}

inline ConcurrentOrderBook::~ConcurrentOrderBook() {
    // Send a Stop sentinel so the worker exits cleanly after draining every
    // command submitted before it.
    enqueue(Command::Type::Stop, OrderId{0}, Price{0}, Quantity{0}, Side::Buy, OrderType::Limit,
            nullptr);
    if (worker_.joinable()) {
        worker_.join();
    }
}

inline void ConcurrentOrderBook::enqueue(Command::Type type, OrderId id, Price price,
                                         Quantity quantity, Side side, OrderType orderType,
                                         std::promise<bool> *result) {
    queue_.emplace(type, id, price, quantity, side, orderType, result);

    // Pairs with the worker's seq_cst store to sleeping_ followed by its
    // seq_cst re-check of the queue: either the worker sees our command, or
    // we see that it is asleep and wake it.
    if (sleeping_.load(std::memory_order_seq_cst) && sleeping_.exchange(false)) {
        sleeping_.notify_one();
    }
}

inline std::future<bool> ConcurrentOrderBook::enqueueWithResult(Command::Type type, OrderId id,
                                                                Price price, Quantity quantity,
                                                                Side side, OrderType orderType) {
    std::promise<bool> promise;
    auto future = promise.get_future();
    enqueue(type, id, price, quantity, side, orderType, &promise);
    return future;
}

inline std::future<bool> ConcurrentOrderBook::submitAddOrder(OrderId id, Price price,
                                                             Quantity quantity, Side side,
                                                             OrderType type) {
    return enqueueWithResult(Command::Type::AddOrder, id, price, quantity, side, type);
}

inline std::future<bool> ConcurrentOrderBook::submitCancelOrder(OrderId id) {
    return enqueueWithResult(Command::Type::CancelOrder, id, Price{0}, Quantity{0}, Side::Buy,
                             OrderType::Limit);
}

inline std::future<bool> ConcurrentOrderBook::submitModifyOrder(OrderId id, Price newPrice,
                                                                Quantity newQuantity) {
    return enqueueWithResult(Command::Type::ModifyOrder, id, newPrice, newQuantity, Side::Buy,
                             OrderType::Limit);
}

inline void ConcurrentOrderBook::postAddOrder(OrderId id, Price price, Quantity quantity, Side side,
                                              OrderType type) {
    enqueue(Command::Type::AddOrder, id, price, quantity, side, type, nullptr);
}

inline void ConcurrentOrderBook::postCancelOrder(OrderId id) {
    enqueue(Command::Type::CancelOrder, id, Price{0}, Quantity{0}, Side::Buy, OrderType::Limit,
            nullptr);
}

inline void ConcurrentOrderBook::postModifyOrder(OrderId id, Price newPrice, Quantity newQuantity) {
    enqueue(Command::Type::ModifyOrder, id, newPrice, newQuantity, Side::Buy, OrderType::Limit,
            nullptr);
}

inline bool ConcurrentOrderBook::execute(const Command &cmd) {
    switch (cmd.type) {
    case Command::Type::AddOrder:
        return book_.addOrder(cmd.id, cmd.price, cmd.quantity, cmd.side, cmd.orderType);
    case Command::Type::CancelOrder:
        return book_.cancelOrder(cmd.id);
    case Command::Type::ModifyOrder:
        return book_.modifyOrder(cmd.id, cmd.price, cmd.quantity);
    default:
        return false;
    }
}

inline void ConcurrentOrderBook::processCommands() {
    std::vector<std::pair<std::promise<bool>, bool>> completed;
    completed.reserve(kMaxBatch);
    bool stop = false;
    unsigned idle = 0;

    while (!stop) {
        size_t drained = 0;
        while (drained < kMaxBatch && queue_.tryConsume([&](Command &cmd) {
            if (cmd.type == Command::Type::Stop) {
                stop = true;
                return;
            }
            bool ok = execute(cmd);
            if (cmd.hasResult)
                completed.emplace_back(std::move(cmd.result()), ok);
            ++drained;
        })) {
            if (stop)
                break;
        }

        if (drained != 0) {
            // Publish before resolving futures: read-your-writes for callers.
            publishSnapshot();
            for (auto &[promise, result] : completed) {
                try {
                    promise.set_value(result);
                } catch (const std::future_error &) {
                    // set_value should never fail here, but guard the worker
                    // thread so it continues processing.
                }
            }
            completed.clear();
            idle = 0;
            continue;
        }
        if (stop)
            break;

        // Queue empty: back off progressively, then park.
        if (++idle < kSpinIterations) {
            ds::cpuRelax();
        } else if (idle < kSpinIterations + kYieldIterations) {
            std::this_thread::yield();
        } else {
            sleeping_.store(true, std::memory_order_seq_cst);
            if (queue_.hasPending()) {
                sleeping_.store(false, std::memory_order_relaxed);
            } else {
                sleeping_.wait(true, std::memory_order_acquire);
            }
            idle = 0;
        }
    }
}

inline void ConcurrentOrderBook::publishSnapshot() {
    Snapshot &s = snapshot_;
    uint64_t v = s.version.load(std::memory_order_relaxed);
    s.version.store(v + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);

    constexpr auto relaxed = std::memory_order_relaxed;
    size_t nb = 0;
    size_t na = 0;
    auto counts = book_.visitTopLevels(
        kPublishedDepth,
        [&](Price p, Quantity q, size_t c) {
            s.bids[nb].price.store(p, relaxed);
            s.bids[nb].quantity.store(q, relaxed);
            s.bids[nb].orderCount.store(c, relaxed);
            ++nb;
        },
        [&](Price p, Quantity q, size_t c) {
            s.asks[na].price.store(p, relaxed);
            s.asks[na].quantity.store(q, relaxed);
            s.asks[na].orderCount.store(c, relaxed);
            ++na;
        });
    s.bestBid.store(nb ? s.bids[0].price.load(relaxed) : 0, relaxed);
    s.bestAsk.store(na ? s.asks[0].price.load(relaxed) : 0, relaxed);
    s.orderCount.store(counts.orders, relaxed);
    s.bidLevels.store(counts.bidLevels, relaxed);
    s.askLevels.store(counts.askLevels, relaxed);

    s.version.store(v + 2, std::memory_order_release);
}

template <typename Fn> inline auto ConcurrentOrderBook::readSnapshot(Fn &&fn) const {
    for (unsigned spins = 0;; ++spins) {
        uint64_t v1 = snapshot_.version.load(std::memory_order_acquire);
        if (v1 & 1) {
            // Publish in progress.  Yield eventually in case the worker was
            // preempted mid-publish on an oversubscribed machine.
            if (spins < 64)
                ds::cpuRelax();
            else
                std::this_thread::yield();
            continue;
        }
        auto result = fn(snapshot_);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (snapshot_.version.load(std::memory_order_relaxed) == v1)
            return result;
    }
}

inline std::optional<Price> ConcurrentOrderBook::bestBid() const {
    if (onWorkerThread())
        return book_.bestBid();
    return readSnapshot([](const Snapshot &s) -> std::optional<Price> {
        if (s.bidLevels.load(std::memory_order_relaxed) == 0)
            return std::nullopt;
        return s.bestBid.load(std::memory_order_relaxed);
    });
}

inline std::optional<Price> ConcurrentOrderBook::bestAsk() const {
    if (onWorkerThread())
        return book_.bestAsk();
    return readSnapshot([](const Snapshot &s) -> std::optional<Price> {
        if (s.askLevels.load(std::memory_order_relaxed) == 0)
            return std::nullopt;
        return s.bestAsk.load(std::memory_order_relaxed);
    });
}

inline std::optional<Price> ConcurrentOrderBook::spread() const {
    if (onWorkerThread())
        return book_.spread();
    return readSnapshot([](const Snapshot &s) -> std::optional<Price> {
        if (s.bidLevels.load(std::memory_order_relaxed) == 0 ||
            s.askLevels.load(std::memory_order_relaxed) == 0)
            return std::nullopt;
        return s.bestAsk.load(std::memory_order_relaxed) -
               s.bestBid.load(std::memory_order_relaxed);
    });
}

inline std::optional<Price> ConcurrentOrderBook::midPrice() const {
    if (onWorkerThread())
        return book_.midPrice();
    return readSnapshot([](const Snapshot &s) -> std::optional<Price> {
        if (s.bidLevels.load(std::memory_order_relaxed) == 0 ||
            s.askLevels.load(std::memory_order_relaxed) == 0)
            return std::nullopt;
        return (s.bestBid.load(std::memory_order_relaxed) +
                s.bestAsk.load(std::memory_order_relaxed)) /
               2;
    });
}

inline std::optional<std::vector<LevelInfo>>
ConcurrentOrderBook::readLevels(const std::array<LevelSlot, kPublishedDepth> &levels,
                                const std::atomic<size_t> &levelCount, size_t depth) const {
    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, kPublishedDepth));
    bool complete = readSnapshot([&](const Snapshot &) {
        result.clear();
        size_t total = levelCount.load(std::memory_order_relaxed);
        if (depth > kPublishedDepth && total > kPublishedDepth)
            return false;  // more levels requested than were published
        size_t n = std::min(depth, total);
        for (size_t i = 0; i < n; ++i) {
            result.emplace_back(levels[i].price.load(std::memory_order_relaxed),
                                levels[i].quantity.load(std::memory_order_relaxed),
                                levels[i].orderCount.load(std::memory_order_relaxed));
        }
        return true;
    });
    if (!complete)
        return std::nullopt;
    return result;
}

inline std::vector<LevelInfo> ConcurrentOrderBook::getBids(size_t depth) const {
    if (!onWorkerThread()) {
        if (auto levels = readLevels(snapshot_.bids, snapshot_.bidLevels, depth))
            return std::move(*levels);
    }
    return book_.getBids(depth);
}

inline std::vector<LevelInfo> ConcurrentOrderBook::getAsks(size_t depth) const {
    if (!onWorkerThread()) {
        if (auto levels = readLevels(snapshot_.asks, snapshot_.askLevels, depth))
            return std::move(*levels);
    }
    return book_.getAsks(depth);
}

inline size_t ConcurrentOrderBook::orderCount() const {
    if (onWorkerThread())
        return book_.orderCount();
    return snapshot_.orderCount.load(std::memory_order_acquire);
}

inline size_t ConcurrentOrderBook::bidLevelCount() const {
    if (onWorkerThread())
        return book_.bidLevelCount();
    return snapshot_.bidLevels.load(std::memory_order_acquire);
}

inline size_t ConcurrentOrderBook::askLevelCount() const {
    if (onWorkerThread())
        return book_.askLevelCount();
    return snapshot_.askLevels.load(std::memory_order_acquire);
}

}  // namespace orderbook
