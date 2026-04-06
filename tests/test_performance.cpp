#include <orderbook.hpp>
#include "test_framework.hpp"
#include <chrono>
#include <sstream>
#include <vector>

using namespace orderbook;
using namespace test;
using namespace std::chrono;

// Conservative performance thresholds chosen to catch severe regressions
// while remaining stable on slow CI machines (benchmarks show 4-5M ops/sec;
// thresholds are set at ~1/50th of that to give ample headroom).
static constexpr double INSERTION_OPS_PER_SEC_MIN    = 100'000.0;
static constexpr double CANCELLATION_OPS_PER_SEC_MIN =  50'000.0;
static constexpr double MATCHING_OPS_PER_SEC_MIN     =  50'000.0;
static constexpr double MARKET_DATA_LATENCY_NS_MAX   =   5'000.0; // ns per query
static constexpr double THROUGHPUT_OPS_PER_SEC_MIN   =  50'000.0;

// Helper: throw a descriptive error when a threshold is violated
static void check_throughput(double actual, double minimum, const char* label) {
    if (actual < minimum) {
        std::ostringstream ss;
        ss << label << " throughput too low: "
           << static_cast<long long>(actual) << " ops/sec"
           << " (minimum required: " << static_cast<long long>(minimum) << " ops/sec)";
        throw std::runtime_error(ss.str());
    }
}

static void check_latency(double actual_ns, double max_ns, const char* label) {
    if (actual_ns > max_ns) {
        std::ostringstream ss;
        ss << label << " latency too high: "
           << static_cast<long long>(actual_ns) << " ns"
           << " (maximum allowed: " << static_cast<long long>(max_ns) << " ns)";
        throw std::runtime_error(ss.str());
    }
}

TEST(order_insertion_throughput) {
    OrderBook book;
    constexpr size_t N = 10'000;

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        Price price = 100'0000 + static_cast<Price>(i % 1000) * 100;
        book.addOrder(i, price, 100, Side::Buy);
    }
    auto end = high_resolution_clock::now();

    double us = duration_cast<microseconds>(end - start).count();
    double ops_per_sec = (N * 1'000'000.0) / us;

    check_throughput(ops_per_sec, INSERTION_OPS_PER_SEC_MIN, "order insertion");
}

TEST(order_cancellation_throughput) {
    OrderBook book;
    constexpr size_t N = 10'000;

    std::vector<OrderId> ids;
    ids.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        Price price = 100'0000 + static_cast<Price>(i % 1000) * 100;
        book.addOrder(i, price, 100, Side::Buy);
        ids.push_back(i);
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        book.cancelOrder(ids[i]);
    }
    auto end = high_resolution_clock::now();

    double us = duration_cast<microseconds>(end - start).count();
    double ops_per_sec = (N * 1'000'000.0) / us;

    check_throughput(ops_per_sec, CANCELLATION_OPS_PER_SEC_MIN, "order cancellation");
}

TEST(order_matching_throughput) {
    OrderBook book;
    constexpr size_t RESTING = 5'000;
    constexpr size_t AGGRESSIVE = 1'000;

    // Pre-populate resting sell orders
    for (size_t i = 0; i < RESTING; ++i) {
        Price price = 101'0000 + static_cast<Price>(i % 100) * 100;
        book.addOrder(i, price, 100, Side::Sell);
    }

    // Measure aggressive buy orders that match resting liquidity
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < AGGRESSIVE; ++i) {
        book.addOrder(RESTING + i, 101'0000, 100, Side::Buy);
    }
    auto end = high_resolution_clock::now();

    double us = duration_cast<microseconds>(end - start).count();
    double ops_per_sec = (AGGRESSIVE * 1'000'000.0) / us;

    check_throughput(ops_per_sec, MATCHING_OPS_PER_SEC_MIN, "order matching");
}

TEST(market_data_latency) {
    OrderBook book;

    // Populate a realistic order book with 500 bid and 500 ask levels
    for (size_t i = 0; i < 500; ++i) {
        book.addOrder(i,         100'0000 - static_cast<Price>(i) * 100, 100, Side::Buy);
        book.addOrder(500 + i,   101'0000 + static_cast<Price>(i) * 100, 100, Side::Sell);
    }

    constexpr size_t N = 100'000;

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        volatile auto bid = book.bestBid();
        (void)bid;
        volatile auto ask = book.bestAsk();
        (void)ask;
        volatile auto mid = book.midPrice();
        (void)mid;
    }
    auto end = high_resolution_clock::now();

    double ns_total = static_cast<double>(duration_cast<nanoseconds>(end - start).count());
    double avg_ns = ns_total / static_cast<double>(N);

    check_latency(avg_ns, MARKET_DATA_LATENCY_NS_MAX, "market data query");
}

TEST(mixed_workload_throughput) {
    OrderBook book;
    constexpr size_t N = 10'000;

    // Alternate buy and sell orders around a mid price so some matches occur
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        if (i % 2 == 0) {
            book.addOrder(i, 100'0000 + static_cast<Price>(i % 50) * 100, 100, Side::Buy);
        } else {
            book.addOrder(i, 100'0000 - static_cast<Price>(i % 50) * 100, 100, Side::Sell);
        }
    }
    auto end = high_resolution_clock::now();

    double us = duration_cast<microseconds>(end - start).count();
    double ops_per_sec = (N * 1'000'000.0) / us;

    check_throughput(ops_per_sec, THROUGHPUT_OPS_PER_SEC_MIN, "mixed workload");
}

int main() {
    return TestSuite::instance().run();
}
