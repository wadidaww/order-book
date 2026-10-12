// Benchmark for ConcurrentOrderBook: multi-producer submission throughput,
// round-trip latency, and market-data reads running alongside writers.
//
// Usage: concurrent_benchmark [ops_per_producer]
#include <orderbook.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace orderbook;
using Clock = std::chrono::steady_clock;

namespace {

constexpr Price MID = 100'0000;
constexpr Price TICK = 100;
constexpr size_t WINDOW = 256;  // outstanding futures per producer

// Deterministic order flow for one producer:
//   35% passive limit adds 1-10 ticks from the mid (resting liquidity)
//   15% aggressive limit adds that cross the mid and trade
//   15% deep limit adds 40-60 ticks away (never reached by aggressive flow)
//   25% cancels, 10% modifies of deep orders.
// Modifies only target deep orders because OrderBook::modifyOrder does not
// support orders that were already filled.
void produce(ConcurrentOrderBook &book, int producer, size_t ops) {
    std::mt19937_64 rng(1234 + producer);
    std::uniform_int_distribution<int> pct(0, 99);
    std::vector<OrderId> near;
    std::vector<std::pair<OrderId, Side>> deep;
    std::deque<std::future<bool>> pending;
    OrderId nextId = (static_cast<OrderId>(producer) + 1) << 40;

    auto sideSign = [](Side s) { return s == Side::Buy ? -1 : 1; };
    auto randomSide = [&] { return (rng() & 1) ? Side::Buy : Side::Sell; };

    for (size_t i = 0; i < ops; ++i) {
        int r = pct(rng);
        if (r < 35) {
            Side side = randomSide();
            Price price = MID + sideSign(side) * static_cast<Price>(1 + rng() % 10) * TICK;
            near.push_back(nextId);
            pending.push_back(book.submitAddOrder(nextId++, price, 1 + rng() % 100, side));
        } else if (r < 50) {
            Side side = randomSide();
            Price price = MID - sideSign(side) * static_cast<Price>(1 + rng() % 5) * TICK;
            pending.push_back(book.submitAddOrder(nextId++, price, 1 + rng() % 50, side));
        } else if (r < 65 || deep.empty()) {
            Side side = randomSide();
            Price price = MID + sideSign(side) * static_cast<Price>(40 + rng() % 21) * TICK;
            deep.emplace_back(nextId, side);
            pending.push_back(book.submitAddOrder(nextId++, price, 1 + rng() % 100, side));
        } else if (r < 90) {
            OrderId id;
            if (!near.empty() && (deep.size() < 64 || (rng() & 1))) {
                size_t idx = rng() % near.size();
                id = near[idx];
                near[idx] = near.back();
                near.pop_back();
            } else {
                size_t idx = rng() % deep.size();
                id = deep[idx].first;
                deep[idx] = deep.back();
                deep.pop_back();
            }
            pending.push_back(book.submitCancelOrder(id));
        } else {
            auto [id, side] = deep[rng() % deep.size()];
            Price price = MID + sideSign(side) * static_cast<Price>(40 + rng() % 21) * TICK;
            pending.push_back(book.submitModifyOrder(id, price, 1 + rng() % 100));
        }
        if (pending.size() >= WINDOW) {
            pending.front().get();
            pending.pop_front();
        }
    }
    for (auto &f : pending)
        f.get();
}

double seconds(Clock::duration d) {
    return std::chrono::duration<double>(d).count();
}

void benchThroughput(size_t opsPerProducer) {
    std::cout << "1. Pipelined submission throughput (" << WINDOW
              << " in-flight futures per producer)\n";
    unsigned hw = std::max(2u, std::thread::hardware_concurrency());
    for (int producers : {1, 2, 3}) {
        if (static_cast<unsigned>(producers) >= hw)
            break;
        ConcurrentOrderBook book;
        auto start = Clock::now();
        std::vector<std::thread> threads;
        for (int p = 0; p < producers; ++p)
            threads.emplace_back(produce, std::ref(book), p, opsPerProducer);
        for (auto &t : threads)
            t.join();
        double s = seconds(Clock::now() - start);
        double total = static_cast<double>(opsPerProducer) * producers;
        std::cout << "  producers=" << producers << "  " << std::fixed << std::setprecision(2)
                  << total / s / 1e6 << " M ops/s  (" << std::setprecision(1) << s * 1e3
                  << " ms)\n";
    }
    std::cout << "\n";
}

void benchPostThroughput(size_t opsPerProducer) {
    std::cout << "4. Fire-and-forget throughput (post*, one submit*() barrier at the end)\n";
    unsigned hw = std::max(2u, std::thread::hardware_concurrency());
    for (int producers : {1, 2, 3}) {
        if (static_cast<unsigned>(producers) >= hw)
            break;
        ConcurrentOrderBook book;
        auto start = Clock::now();
        std::vector<std::thread> threads;
        for (int p = 0; p < producers; ++p) {
            threads.emplace_back([&book, p, opsPerProducer] {
                OrderId base = (static_cast<OrderId>(p) + 1) << 40;
                for (size_t i = 0; i < opsPerProducer; ++i) {
                    Side side = (i & 1) ? Side::Buy : Side::Sell;
                    OrderId id = base + i;
                    if (i % 4 == 3) {
                        book.postCancelOrder(id - 2);
                    } else {
                        Price offset = static_cast<Price>(1 + (i * 7) % 10) * TICK;
                        book.postAddOrder(id, side == Side::Buy ? MID - offset : MID + offset, 10,
                                          side);
                    }
                }
                book.submitCancelOrder(0).get();  // barrier: FIFO per producer
            });
        }
        for (auto &t : threads)
            t.join();
        double s = seconds(Clock::now() - start);
        double total = static_cast<double>(opsPerProducer) * producers;
        std::cout << "  producers=" << producers << "  " << std::fixed << std::setprecision(2)
                  << total / s / 1e6 << " M ops/s  (" << std::setprecision(1) << s * 1e3
                  << " ms)\n";
    }
    std::cout << "\n";
}

void benchLatency(size_t ops) {
    std::cout << "2. Round-trip latency (submit + future.get(), one producer)\n";
    ConcurrentOrderBook book;
    std::vector<double> ns;
    ns.reserve(ops);
    for (size_t i = 0; i < ops; ++i) {
        Side side = (i & 1) ? Side::Buy : Side::Sell;
        Price price = MID + (side == Side::Buy ? -1 : 1) * static_cast<Price>(1 + i % 50) * TICK;
        auto t0 = Clock::now();
        book.submitAddOrder(i + 1, price, 10, side).get();
        ns.push_back(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
    }
    std::sort(ns.begin(), ns.end());
    auto pct = [&](double p) { return ns[static_cast<size_t>(p * (ns.size() - 1))]; };
    std::cout << std::fixed << std::setprecision(0) << "  p50=" << pct(0.50)
              << " ns  p90=" << pct(0.90) << " ns  p99=" << pct(0.99) << " ns  p99.9=" << pct(0.999)
              << " ns\n\n";
}

void benchReadersWithWriter(size_t ops) {
    std::cout << "3. Market-data readers alongside one writer\n";
    unsigned hw = std::max(2u, std::thread::hardware_concurrency());
    for (int readers : {1, 2}) {
        if (static_cast<unsigned>(readers) + 2 > hw)
            break;
        ConcurrentOrderBook book;
        std::atomic<bool> done{false};
        std::atomic<uint64_t> reads{0};
        std::vector<std::thread> threads;
        for (int r = 0; r < readers; ++r) {
            threads.emplace_back([&] {
                uint64_t n = 0;
                uint64_t sink = 0;
                while (!done.load(std::memory_order_relaxed)) {
                    sink += book.bestBid().value_or(0);
                    sink += book.bestAsk().value_or(0);
                    sink += book.spread().value_or(0);
                    if ((n & 7) == 0)
                        sink += book.getBids(5).size() + book.getAsks(5).size();
                    ++n;
                }
                reads.fetch_add(n + (sink == 42 ? 1 : 0), std::memory_order_relaxed);
            });
        }
        auto start = Clock::now();
        produce(book, 0, ops);
        double s = seconds(Clock::now() - start);
        done.store(true);
        for (auto &t : threads)
            t.join();
        std::cout << "  readers=" << readers << "  writer " << std::fixed << std::setprecision(2)
                  << static_cast<double>(ops) / s / 1e6 << " M ops/s,  reads "
                  << static_cast<double>(reads.load()) / s / 1e6 << " M/s\n";
    }
    std::cout << "\n";
}

}  // namespace

int main(int argc, char **argv) {
    size_t ops = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 500'000;
    std::cout << "=== ConcurrentOrderBook Benchmark (" << ops << " ops per producer) ===\n\n";
    benchThroughput(ops);
    benchLatency(std::min<size_t>(ops, 200'000));
    benchReadersWithWriter(ops);
    benchPostThroughput(ops);
    return 0;
}
