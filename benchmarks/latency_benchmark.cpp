#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <orderbook.hpp>
#include <vector>

using namespace orderbook;

class LatencyBenchmark {
public:
  void run() {
    std::cout << "=== Ultra Low-Latency Benchmark ===" << std::endl;
    std::cout << std::endl;

    benchmark_single_order_latency();
    benchmark_matching_latency();
    benchmark_cancel_latency();
    benchmark_modify_latency();
    benchmark_percentiles();

    std::cout << std::endl;
    std::cout << "=== Benchmark Complete ===" << std::endl;
  }

private:
  void benchmark_single_order_latency() {
    std::cout << "Benchmark: Single Order Add Latency (warm cache)"
              << std::endl;

    OrderBook book;
    constexpr size_t iterations = 10000;
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
      book.add_order(i, 10000 + i, 100, Side::Buy);
    }

    OrderId order_id = 100000;

    for (size_t i = 0; i < iterations; ++i) {
      auto start = std::chrono::high_resolution_clock::now();
      book.add_order(order_id++, 10000 + (i % 100), 100, Side::Buy);
      auto end = std::chrono::high_resolution_clock::now();

      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      latencies.push_back(duration.count());
    }

    print_latency_stats(latencies, "ns");
    std::cout << std::endl;
  }

  void benchmark_matching_latency() {
    std::cout << "Benchmark: Order Matching Latency" << std::endl;

    constexpr size_t iterations = 10000;
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);

    for (size_t i = 0; i < iterations; ++i) {
      OrderBook book;

      // Add resting order
      book.add_order(1, 10000, 100, Side::Sell);

      // Measure matching order latency
      auto start = std::chrono::high_resolution_clock::now();
      book.add_order(2, 10000, 50, Side::Buy);
      auto end = std::chrono::high_resolution_clock::now();

      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      latencies.push_back(duration.count());
    }

    print_latency_stats(latencies, "ns");
    std::cout << std::endl;
  }

  void benchmark_cancel_latency() {
    std::cout << "Benchmark: Order Cancel Latency" << std::endl;

    constexpr size_t iterations = 10000;
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);

    for (size_t i = 0; i < iterations; ++i) {
      OrderBook book;

      // Add order
      book.add_order(1, 10000, 100, Side::Buy);

      // Measure cancel latency
      auto start = std::chrono::high_resolution_clock::now();
      book.cancel_order(1);
      auto end = std::chrono::high_resolution_clock::now();

      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      latencies.push_back(duration.count());
    }

    print_latency_stats(latencies, "ns");
    std::cout << std::endl;
  }

  void benchmark_modify_latency() {
    std::cout << "Benchmark: Order Modify Latency" << std::endl;

    constexpr size_t iterations = 10000;
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);

    for (size_t i = 0; i < iterations; ++i) {
      OrderBook book;

      // Add order
      book.add_order(1, 10000, 100, Side::Buy);

      // Measure modify latency
      auto start = std::chrono::high_resolution_clock::now();
      book.modify_order(1, 10050, 150);
      auto end = std::chrono::high_resolution_clock::now();

      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      latencies.push_back(duration.count());
    }

    print_latency_stats(latencies, "ns");
    std::cout << std::endl;
  }

  void benchmark_percentiles() {
    std::cout << "Benchmark: Latency Distribution (Mixed Operations)"
              << std::endl;

    OrderBook book;
    constexpr size_t iterations = 100000;
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);

    OrderId order_id = 1;
    std::vector<OrderId> active_orders;

    // Mixed workload
    for (size_t i = 0; i < iterations; ++i) {
      auto start = std::chrono::high_resolution_clock::now();

      if (i % 3 == 0 && !active_orders.empty()) {
        // Cancel operation
        size_t idx = i % active_orders.size();
        book.cancel_order(active_orders[idx]);
        active_orders.erase(active_orders.begin() + idx);
      } else if (i % 5 == 0 && !active_orders.empty()) {
        // Modify operation
        size_t idx = i % active_orders.size();
        book.modify_order(active_orders[idx], 10000 + (i % 200), 100);
      } else {
        // Add operation
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price =
            side == Side::Buy ? (10000 - (i % 50)) : (10000 + (i % 50));
        book.add_order(order_id, price, 100, side);
        active_orders.push_back(order_id);
        ++order_id;
      }

      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      latencies.push_back(duration.count());
    }

    print_latency_stats(latencies, "ns");
    print_percentiles(latencies);
    std::cout << std::endl;
  }

  void print_latency_stats(const std::vector<int64_t> &latencies,
                           const std::string &unit) {
    if (latencies.empty())
      return;

    int64_t sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    double mean = static_cast<double>(sum) / latencies.size();

    int64_t min_latency = *std::min_element(latencies.begin(), latencies.end());
    int64_t max_latency = *std::max_element(latencies.begin(), latencies.end());

    std::cout << "  Samples: " << latencies.size() << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(2) << mean << " "
              << unit << std::endl;
    std::cout << "  Min: " << min_latency << " " << unit << std::endl;
    std::cout << "  Max: " << max_latency << " " << unit << std::endl;
  }

  void print_percentiles(std::vector<int64_t> latencies) {
    std::sort(latencies.begin(), latencies.end());

    auto get_percentile = [&latencies](double p) {
      size_t idx = static_cast<size_t>(latencies.size() * p / 100.0);
      idx = std::min(idx, latencies.size() - 1);
      return latencies[idx];
    };

    std::cout << "  Percentiles:" << std::endl;
    std::cout << "    50th: " << get_percentile(50) << " ns" << std::endl;
    std::cout << "    75th: " << get_percentile(75) << " ns" << std::endl;
    std::cout << "    90th: " << get_percentile(90) << " ns" << std::endl;
    std::cout << "    95th: " << get_percentile(95) << " ns" << std::endl;
    std::cout << "    99th: " << get_percentile(99) << " ns" << std::endl;
    std::cout << "    99.9th: " << get_percentile(99.9) << " ns" << std::endl;
    std::cout << "    99.99th: " << get_percentile(99.99) << " ns" << std::endl;
  }
};

int main() {
  LatencyBenchmark benchmark;
  benchmark.run();
  return 0;
}