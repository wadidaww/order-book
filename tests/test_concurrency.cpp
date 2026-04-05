#include <orderbook.hpp>
#include "test_framework.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <future>

using namespace orderbook;
using namespace test;

// ---------------------------------------------------------------------------
// ConcurrentOrderBook – basic submission tests
// ---------------------------------------------------------------------------

TEST(concurrent_submit_add_order) {
    ConcurrentOrderBook book;

    constexpr int THREADS           = 4;
    constexpr int ORDERS_PER_THREAD = 25;
    constexpr int TOTAL             = THREADS * ORDERS_PER_THREAD;

    // Pre-size the future vector so each thread writes to its own slots
    std::vector<std::future<bool>> futures(TOTAL);

    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ORDERS_PER_THREAD; ++i) {
                int id = t * ORDERS_PER_THREAD + i + 1;
                futures[id - 1] = book.submitAddOrder(
                    id, 100'0000, 100, Side::Buy);
            }
        });
    }
    for (auto& th : threads) th.join();

    int success = 0;
    for (auto& f : futures) {
        if (f.valid() && f.get()) ++success;
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
    for (auto& th : threads) th.join();

    int cancelled = 0;
    for (auto& f : futures) {
        if (f.valid() && f.get()) ++cancelled;
    }

    ASSERT_EQ(cancelled, N);
    ASSERT_EQ(book.orderCount(), 0u);
}

TEST(concurrent_matching) {
    ConcurrentOrderBook book;

    std::atomic<int> tradeCount{0};
    book.setTradeCallback([&](const Trade&) {
        tradeCount.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int PAIRS = 5;

    // Pre-size futures so each thread writes to its own indices — no shared
    // state, no synchronization needed between the two producer threads.
    std::vector<std::future<bool>> sellFutures(PAIRS);
    std::vector<std::future<bool>> buyFutures(PAIRS);

    std::thread sellThread([&]() {
        for (int i = 0; i < PAIRS; ++i) {
            sellFutures[i] = book.submitAddOrder(
                i + 1, 100'0000, 100, Side::Sell);
        }
    });
    std::thread buyThread([&]() {
        for (int i = 0; i < PAIRS; ++i) {
            buyFutures[i] = book.submitAddOrder(
                PAIRS + i + 1, 100'0000, 100, Side::Buy);
        }
    });

    sellThread.join();
    buyThread.join();

    for (auto& f : sellFutures) f.get();
    for (auto& f : buyFutures)  f.get();

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
    book.setTradeCallback([&](const Trade&) {
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

int main() {
    return TestSuite::instance().run();
}
