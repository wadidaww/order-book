#pragma once

#include "types.hpp"
#include "memory_pool.hpp"
#include "../data_structures/dynamic_circular_queue.hpp"
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>
#include <algorithm>

namespace orderbook {

// ---------------------------------------------------------------------------
// PriceCollarOrderBook
// ---------------------------------------------------------------------------
// An order book that enforces a hard price collar [lowerBound, upperBound].
// Any addOrder / modifyOrder call whose price falls outside this range is
// immediately rejected (returns false / sets status to Rejected) without
// touching the book.
//
// Price levels are stored in a flat array indexed by
//   index = (price - lowerBound) / tickSize
// giving O(1) level access (vs. O(log n) with a tree-based map).
//
// Within each price level orders are held in a ds::DynamicCircularQueue,
// which provides O(1) amortised push (new order), O(1) amortised pop_front
// (matched order) and O(1) average remove (cancel order via hash-map
// lookup + tombstone).
//
// Best-bid / best-ask indices are maintained as scalars and updated lazily,
// yielding O(1) amortised reads. Over a sequence of N operations the total
// scanning work is O(N), so each operation is O(1) amortised.
//
// Thread safety: protected by a single shared_mutex (readers use a shared
// lock; writers use an exclusive lock), matching the behaviour of OrderBook.
class PriceCollarOrderBook {
  public:
    // lowerBound and upperBound are inclusive, in the same fixed-point units
    // as orderbook::Price (scaled by PRICE_SCALE).
    // tickSize is the minimum price increment (≥ 1).  Every order price must
    // satisfy  (price - lowerBound) % tickSize == 0.
    PriceCollarOrderBook(Price lowerBound, Price upperBound, Price tickSize = 1);

    ~PriceCollarOrderBook() = default;

    // Non-copyable, non-movable
    PriceCollarOrderBook(const PriceCollarOrderBook &) = delete;
    PriceCollarOrderBook &operator=(const PriceCollarOrderBook &) = delete;
    PriceCollarOrderBook(PriceCollarOrderBook &&) = delete;
    PriceCollarOrderBook &operator=(PriceCollarOrderBook &&) = delete;

    // -----------------------------------------------------------------------
    // Mutating operations
    // -----------------------------------------------------------------------

    // Add a new order. Returns false (and rejects silently) if:
    //   • the order ID already exists, or
    //   • the price is outside [lowerBound_, upperBound_].
    [[nodiscard]] bool addOrder(OrderId id, Price price, Quantity quantity, Side side,
                                OrderType type = OrderType::Limit);

    // Cancel an existing order. Returns false if the ID is unknown.
    [[nodiscard]] bool cancelOrder(OrderId id);

    // Modify price and/or quantity of an existing order.  The new price must
    // be within the collar, otherwise the call returns false.
    [[nodiscard]] bool modifyOrder(OrderId id, Price newPrice, Quantity newQuantity);

    // -----------------------------------------------------------------------
    // Read-only queries — all O(1)
    // -----------------------------------------------------------------------

    [[nodiscard]] std::optional<Order> getOrder(OrderId id) const;

    [[nodiscard]] std::optional<Price> bestBid() const;
    [[nodiscard]] std::optional<Price> bestAsk() const;
    [[nodiscard]] std::optional<Price> spread() const;
    [[nodiscard]] std::optional<Price> midPrice() const;

    [[nodiscard]] std::vector<LevelInfo> getBids(size_t depth = 10) const;
    [[nodiscard]] std::vector<LevelInfo> getAsks(size_t depth = 10) const;

    [[nodiscard]] Quantity getVolumeAtPrice(Price price, Side side) const;

    // Returns true when price is within [lowerBound_, upperBound_] and is
    // aligned to tickSize_.
    [[nodiscard]] bool priceInCollar(Price price) const noexcept;

    // Accessors for collar parameters
    [[nodiscard]] Price lowerBound() const noexcept { return lowerBound_; }
    [[nodiscard]] Price upperBound() const noexcept { return upperBound_; }
    [[nodiscard]] Price tickSize() const noexcept { return tickSize_; }

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    void setTradeCallback(TradeCallback callback) { tradeCallback_ = std::move(callback); }
    void setOrderUpdateCallback(OrderUpdateCallback callback) {
        orderUpdateCallback_ = std::move(callback);
    }

    // -----------------------------------------------------------------------
    // Maintenance
    // -----------------------------------------------------------------------

    void clear();

    [[nodiscard]] size_t orderCount() const;
    [[nodiscard]] size_t bidLevelCount() const;
    [[nodiscard]] size_t askLevelCount() const;

  private:
    // Internal price level: circular queue + aggregate quantity.
    struct Level {
        ds::DynamicCircularQueue<Order> queue;
        Quantity totalQty{0};
        size_t count{0};

        void add(Order *o) {
            queue.push(o);
            totalQty += o->remainingQuantity();
            ++count;
        }

        void removeOrder(Order *o) {
            if (queue.remove(o)) {
                totalQty -= o->remainingQuantity();
                --count;
            }
        }

        void updateQty(Quantity oldQty, Quantity newQty) {
            totalQty = totalQty - oldQty + newQty;
        }

        [[nodiscard]] bool empty() const noexcept { return queue.empty(); }
        [[nodiscard]] Order *front() { return queue.front(); }
        void popFront() {
            if (Order *o = queue.front()) {
                totalQty -= o->remainingQuantity();
                --count;
                queue.popFront();
            }
        }
    };

    // Convert a valid price to a level-array index.
    [[nodiscard]] size_t toIndex(Price price) const noexcept {
        return static_cast<size_t>((price - lowerBound_) / tickSize_);
    }

    // Convert an array index back to a price.
    [[nodiscard]] Price toPrice(size_t idx) const noexcept {
        return lowerBound_ + static_cast<Price>(idx) * tickSize_;
    }

    // -----------------------------------------------------------------------
    // Internal matching
    // -----------------------------------------------------------------------

    void matchOrder(Order *order);
    void matchLimitOrder(Order *order);
    void matchMarketOrder(Order *order);

    void addToBook(Order *order);
    void removeFromBook(Order *order);

    void executeTrade(Order *maker, Order *taker, Quantity quantity);

    void notifyTrade(const Trade &trade);
    void notifyOrderUpdate(const Order &order);

    [[nodiscard]] Timestamp now() const;

    // -----------------------------------------------------------------------
    // Best-bid / best-ask index maintenance
    // -----------------------------------------------------------------------

    // Scan downward from bestBidIdx_ to find the highest occupied bid level.
    void updateBestBid();

    // Scan upward from bestAskIdx_ to find the lowest occupied ask level.
    void updateBestAsk();

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------

    Price lowerBound_;
    Price upperBound_;
    Price tickSize_;
    size_t levels_;  // total number of price levels = (upper - lower)/tick + 1

    // Flat arrays of price levels (size = levels_).
    // Index 0 corresponds to lowerBound_; index (levels_-1) to upperBound_.
    std::vector<Level> bids_;  // buy side
    std::vector<Level> asks_;  // sell side

    // Cached best-bid/ask indices (into bids_/asks_ arrays respectively).
    // kNoLevel means no orders on that side.
    static constexpr size_t kNoLevel = static_cast<size_t>(-1);
    size_t bestBidIdx_{kNoLevel};
    size_t bestAskIdx_{kNoLevel};

    // Non-empty bid level count and ask level count (for bidLevelCount / askLevelCount).
    size_t bidLevelCount_{0};
    size_t askLevelCount_{0};

    // Order lookup
    std::unordered_map<OrderId, Order *> orders_;

    // Memory pool for Order allocation
    MemoryPool<Order> orderPool_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Callbacks
    TradeCallback tradeCallback_;
    OrderUpdateCallback orderUpdateCallback_;

    // Pending notifications (dispatched outside the lock)
    std::vector<Trade> pendingTrades_;
    std::vector<Order> pendingUpdates_;
};

// ===========================================================================
// Inline implementation
// ===========================================================================

inline PriceCollarOrderBook::PriceCollarOrderBook(Price lowerBound, Price upperBound,
                                                  Price tickSize)
    : lowerBound_(lowerBound)
    , upperBound_(upperBound)
    , tickSize_(tickSize) {
    if (lowerBound_ >= upperBound_) {
        throw std::invalid_argument("lowerBound must be strictly less than upperBound");
    }
    if (tickSize_ <= 0) {
        throw std::invalid_argument("tickSize must be positive");
    }
    if ((upperBound_ - lowerBound_) % tickSize_ != 0) {
        throw std::invalid_argument(
            "collar width (upperBound - lowerBound) must be divisible by tickSize");
    }
    levels_ = static_cast<size_t>((upperBound_ - lowerBound_) / tickSize_) + 1;
    bids_.resize(levels_);
    asks_.resize(levels_);
}

inline bool PriceCollarOrderBook::priceInCollar(Price price) const noexcept {
    return price >= lowerBound_ && price <= upperBound_ &&
           (price - lowerBound_) % tickSize_ == 0;
}

inline bool PriceCollarOrderBook::addOrder(OrderId id, Price price, Quantity quantity, Side side,
                                           OrderType type) {
    std::vector<Trade> localTrades;
    std::vector<Order> localUpdates;

    {
        std::unique_lock lock(mutex_);

        // Reject out-of-collar prices for limit orders
        if (type == OrderType::Limit && !priceInCollar(price)) {
            return false;
        }

        if (orders_.find(id) != orders_.end()) {
            return false;
        }

        auto *order = orderPool_.allocate(id, price, quantity, side, type, now());
        orders_[id] = order;

        matchOrder(order);

        if (!order->isFilled() && order->type == OrderType::Limit) {
            addToBook(order);
        }

        notifyOrderUpdate(*order);

        localTrades.swap(pendingTrades_);
        localUpdates.swap(pendingUpdates_);
    }

    for (const auto &t : localTrades) {
        if (tradeCallback_)
            tradeCallback_(t);
    }
    for (const auto &u : localUpdates) {
        if (orderUpdateCallback_)
            orderUpdateCallback_(u);
    }

    return true;
}

inline bool PriceCollarOrderBook::cancelOrder(OrderId id) {
    std::vector<Order> localUpdates;

    {
        std::unique_lock lock(mutex_);

        auto it = orders_.find(id);
        if (it == orders_.end()) {
            return false;
        }

        Order *order = it->second;

        if (order->status == OrderStatus::New || order->status == OrderStatus::PartiallyFilled) {
            removeFromBook(order);
        }

        order->status = OrderStatus::Cancelled;
        notifyOrderUpdate(*order);

        orders_.erase(it);
        orderPool_.deallocate(order);

        localUpdates.swap(pendingUpdates_);
    }

    for (const auto &u : localUpdates) {
        if (orderUpdateCallback_)
            orderUpdateCallback_(u);
    }

    return true;
}

inline bool PriceCollarOrderBook::modifyOrder(OrderId id, Price newPrice, Quantity newQuantity) {
    std::vector<Trade> localTrades;
    std::vector<Order> localUpdates;

    {
        std::unique_lock lock(mutex_);

        if (!priceInCollar(newPrice)) {
            return false;
        }

        auto it = orders_.find(id);
        if (it == orders_.end()) {
            return false;
        }

        Order *order = it->second;

        removeFromBook(order);

        order->price = newPrice;
        order->quantity = order->filledQuantity + newQuantity;

        matchOrder(order);

        if (!order->isFilled() && order->type == OrderType::Limit) {
            addToBook(order);
        }

        notifyOrderUpdate(*order);

        localTrades.swap(pendingTrades_);
        localUpdates.swap(pendingUpdates_);
    }

    for (const auto &t : localTrades) {
        if (tradeCallback_)
            tradeCallback_(t);
    }
    for (const auto &u : localUpdates) {
        if (orderUpdateCallback_)
            orderUpdateCallback_(u);
    }

    return true;
}

inline std::optional<Order> PriceCollarOrderBook::getOrder(OrderId id) const {
    std::shared_lock lock(mutex_);

    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    return *it->second;
}

inline std::optional<Price> PriceCollarOrderBook::bestBid() const {
    std::shared_lock lock(mutex_);

    if (bestBidIdx_ == kNoLevel) {
        return std::nullopt;
    }
    return toPrice(bestBidIdx_);
}

inline std::optional<Price> PriceCollarOrderBook::bestAsk() const {
    std::shared_lock lock(mutex_);

    if (bestAskIdx_ == kNoLevel) {
        return std::nullopt;
    }
    return toPrice(bestAskIdx_);
}

inline std::optional<Price> PriceCollarOrderBook::spread() const {
    std::shared_lock lock(mutex_);

    if (bestBidIdx_ == kNoLevel || bestAskIdx_ == kNoLevel) {
        return std::nullopt;
    }
    return toPrice(bestAskIdx_) - toPrice(bestBidIdx_);
}

inline std::optional<Price> PriceCollarOrderBook::midPrice() const {
    std::shared_lock lock(mutex_);

    if (bestBidIdx_ == kNoLevel || bestAskIdx_ == kNoLevel) {
        return std::nullopt;
    }
    return (toPrice(bestBidIdx_) + toPrice(bestAskIdx_)) / 2;
}

inline std::vector<LevelInfo> PriceCollarOrderBook::getBids(size_t depth) const {
    std::shared_lock lock(mutex_);

    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, bidLevelCount_));

    if (bestBidIdx_ == kNoLevel) {
        return result;
    }

    // Iterate from bestBidIdx_ downwards
    size_t count = 0;
    for (size_t i = bestBidIdx_; count < depth; --i) {
        if (!bids_[i].empty()) {
            result.emplace_back(toPrice(i), bids_[i].totalQty, bids_[i].count);
            ++count;
        }
        if (i == 0)
            break;
    }

    return result;
}

inline std::vector<LevelInfo> PriceCollarOrderBook::getAsks(size_t depth) const {
    std::shared_lock lock(mutex_);

    std::vector<LevelInfo> result;
    result.reserve(std::min(depth, askLevelCount_));

    if (bestAskIdx_ == kNoLevel) {
        return result;
    }

    size_t count = 0;
    for (size_t i = bestAskIdx_; i < levels_ && count < depth; ++i) {
        if (!asks_[i].empty()) {
            result.emplace_back(toPrice(i), asks_[i].totalQty, asks_[i].count);
            ++count;
        }
    }

    return result;
}

inline Quantity PriceCollarOrderBook::getVolumeAtPrice(Price price, Side side) const {
    std::shared_lock lock(mutex_);

    if (!priceInCollar(price)) {
        return 0;
    }

    const size_t idx = toIndex(price);
    if (side == Side::Buy) {
        return bids_[idx].totalQty;
    }
    return asks_[idx].totalQty;
}

inline void PriceCollarOrderBook::clear() {
    std::unique_lock lock(mutex_);

    bids_.clear();
    asks_.clear();
    bids_.resize(levels_);
    asks_.resize(levels_);
    orders_.clear();
    orderPool_.clear();
    bestBidIdx_ = kNoLevel;
    bestAskIdx_ = kNoLevel;
    bidLevelCount_ = 0;
    askLevelCount_ = 0;
}

inline size_t PriceCollarOrderBook::orderCount() const {
    std::shared_lock lock(mutex_);
    return orders_.size();
}

inline size_t PriceCollarOrderBook::bidLevelCount() const {
    std::shared_lock lock(mutex_);
    return bidLevelCount_;
}

inline size_t PriceCollarOrderBook::askLevelCount() const {
    std::shared_lock lock(mutex_);
    return askLevelCount_;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

inline void PriceCollarOrderBook::updateBestBid() {
    if (bestBidIdx_ == kNoLevel) {
        return;
    }
    while (bestBidIdx_ != kNoLevel && bids_[bestBidIdx_].empty()) {
        if (bestBidIdx_ == 0) {
            bestBidIdx_ = kNoLevel;
        } else {
            --bestBidIdx_;
        }
    }
}

inline void PriceCollarOrderBook::updateBestAsk() {
    if (bestAskIdx_ == kNoLevel) {
        return;
    }
    while (bestAskIdx_ < levels_ && asks_[bestAskIdx_].empty()) {
        ++bestAskIdx_;
    }
    if (bestAskIdx_ >= levels_) {
        bestAskIdx_ = kNoLevel;
    }
}

inline void PriceCollarOrderBook::addToBook(Order *order) {
    const size_t idx = toIndex(order->price);

    if (order->side == Side::Buy) {
        const bool wasEmpty = bids_[idx].empty();
        bids_[idx].add(order);
        if (wasEmpty) {
            ++bidLevelCount_;
        }
        if (bestBidIdx_ == kNoLevel || idx > bestBidIdx_) {
            bestBidIdx_ = idx;
        }
    } else {
        const bool wasEmpty = asks_[idx].empty();
        asks_[idx].add(order);
        if (wasEmpty) {
            ++askLevelCount_;
        }
        if (bestAskIdx_ == kNoLevel || idx < bestAskIdx_) {
            bestAskIdx_ = idx;
        }
    }
}

inline void PriceCollarOrderBook::removeFromBook(Order *order) {
    const size_t idx = toIndex(order->price);

    if (order->side == Side::Buy) {
        bids_[idx].removeOrder(order);
        if (bids_[idx].empty()) {
            --bidLevelCount_;
            if (idx == bestBidIdx_) {
                updateBestBid();
            }
        }
    } else {
        asks_[idx].removeOrder(order);
        if (asks_[idx].empty()) {
            --askLevelCount_;
            if (idx == bestAskIdx_) {
                updateBestAsk();
            }
        }
    }
}

inline void PriceCollarOrderBook::matchOrder(Order *order) {
    if (order->type == OrderType::Market) {
        matchMarketOrder(order);
    } else {
        matchLimitOrder(order);
    }
}

inline void PriceCollarOrderBook::matchLimitOrder(Order *order) {
    if (order->side == Side::Buy) {
        // Match buy against asks starting from bestAsk
        while (!order->isFilled() && bestAskIdx_ != kNoLevel) {
            // Can we cross at this level?
            if (order->price < toPrice(bestAskIdx_)) {
                break;
            }

            Level &level = asks_[bestAskIdx_];
            while (!order->isFilled() && !level.empty()) {
                Order *maker = level.front();
                const Quantity matchQty =
                    std::min(order->remainingQuantity(), maker->remainingQuantity());

                const Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                const Quantity newMakerQty = maker->remainingQuantity();
                level.updateQty(oldMakerQty, newMakerQty);

                if (maker->isFilled()) {
                    level.popFront();
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }

            if (level.empty()) {
                --askLevelCount_;
                updateBestAsk();
            }
        }
    } else {
        // Match sell against bids starting from bestBid
        while (!order->isFilled() && bestBidIdx_ != kNoLevel) {
            if (order->price > toPrice(bestBidIdx_)) {
                break;
            }

            Level &level = bids_[bestBidIdx_];
            while (!order->isFilled() && !level.empty()) {
                Order *maker = level.front();
                const Quantity matchQty =
                    std::min(order->remainingQuantity(), maker->remainingQuantity());

                const Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                const Quantity newMakerQty = maker->remainingQuantity();
                level.updateQty(oldMakerQty, newMakerQty);

                if (maker->isFilled()) {
                    level.popFront();
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }

            if (level.empty()) {
                --bidLevelCount_;
                updateBestBid();
            }
        }
    }

    // Update taker order status
    if (order->isFilled()) {
        order->status = OrderStatus::Filled;
    } else if (order->filledQuantity > 0) {
        order->status = OrderStatus::PartiallyFilled;
    }

    // Handle IOC / FOK
    if (order->type == OrderType::IOC && !order->isFilled()) {
        order->status = OrderStatus::Cancelled;
    } else if (order->type == OrderType::FOK && order->filledQuantity != order->quantity) {
        order->status = OrderStatus::Rejected;
        order->filledQuantity = 0;
    }
}

inline void PriceCollarOrderBook::matchMarketOrder(Order *order) {
    if (order->side == Side::Buy) {
        while (!order->isFilled() && bestAskIdx_ != kNoLevel) {
            Level &level = asks_[bestAskIdx_];
            while (!order->isFilled() && !level.empty()) {
                Order *maker = level.front();
                const Quantity matchQty =
                    std::min(order->remainingQuantity(), maker->remainingQuantity());

                const Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                const Quantity newMakerQty = maker->remainingQuantity();
                level.updateQty(oldMakerQty, newMakerQty);

                if (maker->isFilled()) {
                    level.popFront();
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }

            if (level.empty()) {
                --askLevelCount_;
                updateBestAsk();
            }
        }
    } else {
        while (!order->isFilled() && bestBidIdx_ != kNoLevel) {
            Level &level = bids_[bestBidIdx_];
            while (!order->isFilled() && !level.empty()) {
                Order *maker = level.front();
                const Quantity matchQty =
                    std::min(order->remainingQuantity(), maker->remainingQuantity());

                const Quantity oldMakerQty = maker->remainingQuantity();
                executeTrade(maker, order, matchQty);
                const Quantity newMakerQty = maker->remainingQuantity();
                level.updateQty(oldMakerQty, newMakerQty);

                if (maker->isFilled()) {
                    level.popFront();
                    maker->status = OrderStatus::Filled;
                    notifyOrderUpdate(*maker);
                } else {
                    maker->status = OrderStatus::PartiallyFilled;
                    notifyOrderUpdate(*maker);
                }
            }

            if (level.empty()) {
                --bidLevelCount_;
                updateBestBid();
            }
        }
    }

    order->status = order->isFilled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
}

inline void PriceCollarOrderBook::executeTrade(Order *maker, Order *taker, Quantity quantity) {
    maker->filledQuantity += quantity;
    taker->filledQuantity += quantity;

    Trade trade(maker->id, taker->id, maker->price, quantity, now());
    notifyTrade(trade);
}

inline void PriceCollarOrderBook::notifyTrade(const Trade &trade) {
    pendingTrades_.push_back(trade);
}

inline void PriceCollarOrderBook::notifyOrderUpdate(const Order &order) {
    pendingUpdates_.push_back(order);
}

inline Timestamp PriceCollarOrderBook::now() const {
    return std::chrono::duration_cast<Timestamp>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
}

}  // namespace orderbook
