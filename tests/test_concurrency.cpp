#include <orderbook.hpp>
#include "test_framework.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <optional>

using namespace orderbook;
using namespace test;

// ---------------------------------------------------------------------------
// ConcurrentOrderBook – basic submission tests
// ---------------------------------------------------------------------------

TEST(concurrent_submit_add_order) {
    ConcurrentOrderBook book;

    constexpr int THREADS = 4;
    constexpr int ORDERS_PER_THREAD = 25;
    constexpr int TOTAL = THREADS * ORDERS_PER_THREAD;

    // Pre-size the future vector so each thread writes to its own slots
    std::vector<std::future<bool>> futures(TOTAL);

    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ORDERS_PER_THREAD; ++i) {
                int id = t * ORDERS_PER_THREAD + i + 1;
                futures[id - 1] = book.submitAddOrder(id, 100'0000, 100, Side::Buy);
            }
        });
    }
    for (auto &th : threads)
        th.join();

    int success = 0;
    for (auto &f : futures) {
        if (f.valid() && f.get())
            ++success;
    }

    ASSERT_EQ(success, TOTAL);
    ASSERT_EQ(book.orderCount(), static_cast<size_t>(TOTAL));
}

TEST(concurrent_submit_cancel_order) {
    ConcurrentOrderBook book;

    // Add orders sequentially first
    constexpr int N = 10;
    for (int i = 1; i <= N; ++i) {
        book.submitAddOrder(i, 100'0000, 100, Side::Buy).get();
    }
    ASSERT_EQ(book.orderCount(), static_cast<size_t>(N));

    // Cancel them from multiple threads
    std::vector<std::future<bool>> futures(N);
    std::vector<std::thread> threads;
    threads.reserve(2);
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < N / 2; ++i) {
                int id = t * (N / 2) + i + 1;
                futures[id - 1] = book.submitCancelOrder(id);
            }
        });
    }
    for (auto &th : threads)
        th.join();

    int cancelled = 0;
    for (auto &f : futures) {
        if (f.valid() && f.get())
            ++cancelled;
    }

    ASSERT_EQ(cancelled, N);
    ASSERT_EQ(book.orderCount(), 0u);
}

TEST(concurrent_matching) {
    ConcurrentOrderBook book;

    std::atomic<int> tradeCount{0};
    book.setTradeCallback(
        [&](const Trade &) { tradeCount.fetch_add(1, std::memory_order_relaxed); });

    constexpr int PAIRS = 5;

    // Pre-size futures so each thread writes to its own indices — no shared
    // state, no synchronization needed between the two producer threads.
    std::vector<std::future<bool>> sellFutures(PAIRS);
    std::vector<std::future<bool>> buyFutures(PAIRS);

    std::thread sellThread([&]() {
        for (int i = 0; i < PAIRS; ++i) {
            sellFutures[i] = book.submitAddOrder(i + 1, 100'0000, 100, Side::Sell);
        }
    });
    std::thread buyThread([&]() {
        for (int i = 0; i < PAIRS; ++i) {
            buyFutures[i] = book.submitAddOrder(PAIRS + i + 1, 100'0000, 100, Side::Buy);
        }
    });

    sellThread.join();
    buyThread.join();

    for (auto &f : sellFutures)
        f.get();
    for (auto &f : buyFutures)
        f.get();

    ASSERT_EQ(tradeCount.load(), PAIRS);
    // All orders were matched; no resting levels should remain
    ASSERT_EQ(book.bidLevelCount(), 0u);
    ASSERT_EQ(book.askLevelCount(), 0u);
}

TEST(concurrent_duplicate_id_rejected) {
    ConcurrentOrderBook book;

    // Same ID submitted from two threads — exactly one should succeed
    constexpr OrderId SHARED_ID = 42;

    auto f1 = book.submitAddOrder(SHARED_ID, 100'0000, 100, Side::Buy);
    auto f2 = book.submitAddOrder(SHARED_ID, 100'0000, 100, Side::Buy);

    bool r1 = f1.get();
    bool r2 = f2.get();

    // Exactly one must succeed
    ASSERT_TRUE(r1 != r2);
    ASSERT_EQ(book.orderCount(), 1u);
}

TEST(concurrent_modify_order) {
    ConcurrentOrderBook book;

    book.submitAddOrder(1, 100'0000, 100, Side::Buy).get();
    ASSERT_EQ(book.orderCount(), 1u);

    bool ok = book.submitModifyOrder(1, 99'0000, 200).get();
    ASSERT_TRUE(ok);
    ASSERT_EQ(book.orderCount(), 1u);

    auto order = book.getOrder(1);
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->price, 99'0000);
}

// ---------------------------------------------------------------------------
// Callback re-entrancy: verify callbacks are NOT invoked while the lock is
// held — i.e., a callback that reads the book does not deadlock.
// ---------------------------------------------------------------------------

TEST(callback_no_deadlock) {
    OrderBook book;

    bool callbackReadOk = false;
    book.setTradeCallback([&](const Trade &) {
        // Calling back into the book from inside the trade callback.
        // With the old (lock-while-notifying) design this would deadlock;
        // with the new deferred design it should complete normally.
        auto bid = book.bestBid();
        callbackReadOk = true;
        (void)bid;
    });

    book.addOrder(1, 100'0000, 100, Side::Sell);
    book.addOrder(2, 100'0000, 100, Side::Buy);

    ASSERT_TRUE(callbackReadOk);
}

// ---------------------------------------------------------------------------
// Lock-free submission / snapshot reads
// ---------------------------------------------------------------------------

// A future resolving implies the read API already reflects that command.
TEST(concurrent_read_your_writes) {
    ConcurrentOrderBook book;
    for (int i = 1; i <= 50; ++i) {
        ASSERT_TRUE(book.submitAddOrder(i, 100'0000 - i * 100, 10, Side::Buy).get());
        ASSERT_EQ(book.orderCount(), static_cast<size_t>(i));
        ASSERT_EQ(book.bestBid().value(), 100'0000 - 100);
        ASSERT_EQ(book.bidLevelCount(), static_cast<size_t>(i));
    }
    ASSERT_TRUE(book.submitAddOrder(1000, 101'0000, 5, Side::Sell).get());
    ASSERT_EQ(book.bestAsk().value(), 101'0000);
    ASSERT_EQ(book.spread().value(), 101'0000 - 99'9900);
    ASSERT_EQ(book.midPrice().value(), (101'0000 + 99'9900) / 2);
}

// getBids/getAsks match the plain OrderBook for depths inside and beyond the
// lock-free snapshot (deep requests fall back to the live book).
TEST(concurrent_depth_matches_order_book) {
    ConcurrentOrderBook cbook;
    OrderBook ref;
    OrderId id = 1;
    for (int level = 0; level < 25; ++level) {
        for (int k = 0; k <= level % 3; ++k, ++id) {
            Price bid = 100'0000 - level * 100;
            Price ask = 101'0000 + level * 100;
            ASSERT_TRUE(cbook.submitAddOrder(id, bid, 10 + k, Side::Buy).get());
            ASSERT_TRUE(ref.addOrder(id, bid, 10 + k, Side::Buy));
            ++id;
            ASSERT_TRUE(cbook.submitAddOrder(id, ask, 20 + k, Side::Sell).get());
            ASSERT_TRUE(ref.addOrder(id, ask, 20 + k, Side::Sell));
        }
    }
    auto same = [](const std::vector<LevelInfo> &a, const std::vector<LevelInfo> &b) {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i].price != b[i].price || a[i].quantity != b[i].quantity ||
                a[i].orderCount != b[i].orderCount)
                return false;
        }
        return true;
    };
    for (size_t depth : {0u, 1u, 5u, 10u, 11u, 25u, 100u}) {
        ASSERT_TRUE(same(cbook.getBids(depth), ref.getBids(depth)));
        ASSERT_TRUE(same(cbook.getAsks(depth), ref.getAsks(depth)));
    }
    ASSERT_EQ(cbook.orderCount(), ref.orderCount());
    ASSERT_EQ(cbook.bidLevelCount(), ref.bidLevelCount());
    ASSERT_EQ(cbook.askLevelCount(), ref.askLevelCount());
}

// Readers spinning on the lock-free API must never observe a torn snapshot.
// Writers only add non-crossing orders, so every consistent snapshot has
// bid < ask, strictly ordered levels and level counts within the totals.
TEST(concurrent_readers_see_consistent_snapshots) {
    ConcurrentOrderBook book;
    std::atomic<bool> done{false};
    std::atomic<int> violations{0};
    std::atomic<long> reads{0};

    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r) {
        readers.emplace_back([&] {
            long n = 0;
            while (!done.load(std::memory_order_relaxed)) {
                auto spread = book.spread();
                if (spread && *spread <= 0)
                    violations.fetch_add(1);
                auto bids = book.getBids(10);
                for (size_t i = 1; i < bids.size(); ++i) {
                    if (bids[i].price >= bids[i - 1].price)
                        violations.fetch_add(1);
                }
                auto asks = book.getAsks(10);
                for (size_t i = 1; i < asks.size(); ++i) {
                    if (asks[i].price <= asks[i - 1].price)
                        violations.fetch_add(1);
                }
                for (const auto &l : bids) {
                    if (l.orderCount == 0 || l.quantity == 0)
                        violations.fetch_add(1);
                }
                ++n;
                // Let the writer run on machines with fewer cores than threads
                // (and under Valgrind, which runs one thread at a time).
                std::this_thread::yield();
            }
            reads.fetch_add(n);
        });
    }

    constexpr int N = 2000;
    std::vector<std::future<bool>> futures;
    futures.reserve(2 * N);
    for (int i = 0; i < N; ++i) {
        Price offset = (i % 40) * 100;
        futures.push_back(book.submitAddOrder(2 * i + 1, 100'0000 - offset, 1 + i % 7, Side::Buy));
        futures.push_back(book.submitAddOrder(2 * i + 2, 100'0100 + offset, 1 + i % 5, Side::Sell));
        if (i % 3 == 0)
            futures.push_back(book.submitCancelOrder(2 * i + 1));
    }
    for (auto &f : futures)
        f.get();
    done.store(true);
    for (auto &t : readers)
        t.join();

    ASSERT_EQ(violations.load(), 0);
    ASSERT_TRUE(reads.load() > 0);
    ASSERT_EQ(book.bidLevelCount(), 40u);
    ASSERT_EQ(book.askLevelCount(), 40u);
}

// Fire-and-forget commands from several threads, with a tiny command queue
// to force back-pressure; a final future per producer acts as a barrier.
TEST(concurrent_post_with_small_queue) {
    ConcurrentOrderBook book(8);
    std::atomic<int> updates{0};
    book.setOrderUpdateCallback([&](const Order &) { updates.fetch_add(1); });

    constexpr int THREADS = 3;
    constexpr int PER_THREAD = 2000;
    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t] {
            OrderId base = static_cast<OrderId>(t) * PER_THREAD;
            for (int i = 1; i <= PER_THREAD; ++i)
                book.postAddOrder(base + i, 90'0000 + i, 1, Side::Buy);
            for (int i = 1; i <= PER_THREAD; i += 2)
                book.postCancelOrder(base + i);
            book.submitCancelOrder(0).get();  // unknown id; just waits
        });
    }
    for (auto &th : threads)
        th.join();

    ASSERT_EQ(book.orderCount(), static_cast<size_t>(THREADS * PER_THREAD / 2));
    ASSERT_EQ(updates.load(), THREADS * PER_THREAD * 3 / 2);
}

// Callbacks run on the worker thread and must see the live book state that
// the triggering command produced, not the previous snapshot.
TEST(concurrent_callback_reads_live_book) {
    ConcurrentOrderBook book;
    std::optional<Price> seenAsk;
    size_t seenCount = 0;
    book.setOrderUpdateCallback([&](const Order &o) {
        if (o.id == 2) {
            seenAsk = book.bestAsk();
            seenCount = book.orderCount();
        }
    });
    ASSERT_TRUE(book.submitAddOrder(1, 100'0000, 10, Side::Buy).get());
    ASSERT_TRUE(book.submitAddOrder(2, 101'0000, 10, Side::Sell).get());
    ASSERT_TRUE(seenAsk.has_value());
    ASSERT_EQ(*seenAsk, 101'0000);
    ASSERT_EQ(seenCount, 2u);
}

// The destructor drains every command submitted before it.
TEST(concurrent_destructor_drains_queue) {
    std::atomic<int> updates{0};
    {
        ConcurrentOrderBook book;
        book.setOrderUpdateCallback([&](const Order &) { updates.fetch_add(1); });
        for (int i = 1; i <= 500; ++i)
            book.postAddOrder(i, 100'0000, 1, Side::Buy);
    }
    ASSERT_EQ(updates.load(), 500);
}

int main() {
    return TestSuite::instance().run();
}
