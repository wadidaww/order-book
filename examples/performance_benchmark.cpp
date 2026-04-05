#include <orderbook.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>

using namespace orderbook;
using namespace std::chrono;

class PerformanceBenchmark {
public:
    void run() {
        std::cout << "=== Order Book Performance Benchmark ===\n\n";
        
        benchmark_order_insertion();
        benchmark_order_matching();
        benchmark_order_cancellation();
        benchmark_market_data_access();
        benchmark_throughput();
    }

private:
    void benchmark_order_insertion() {
        std::cout << "1. Order Insertion Benchmark\n";
        
        OrderBook book;
        constexpr size_t N = 100'000;
        
        auto start = high_resolution_clock::now();
        
        for (size_t i = 0; i < N; ++i) {
            Price price = 100'0000 + (i % 1000) * 100;
            book.addOrder(i, price, 100, Side::Buy);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double avgLatency = static_cast<double>(duration.count()) / N;
        double opsPerSec = (N * 1'000'000.0) / duration.count();
        
        std::cout << "  Inserted " << N << " orders in " 
                  << duration.count() / 1000.0 << " ms\n";
        std::cout << "  Average latency: " << std::fixed << std::setprecision(3)
                  << avgLatency << " μs\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " ops/sec\n\n";
    }
    
    void benchmark_order_matching() {
        std::cout << "2. Order Matching Benchmark\n";
        
        OrderBook book;
        constexpr size_t N = 50'000;
        
        // Pre-populate with resting orders
        for (size_t i = 0; i < N; ++i) {
            Price price = 100'0000 - (i % 100) * 100;
            book.addOrder(i, price, 100, Side::Buy);
        }
        
        for (size_t i = N; i < 2 * N; ++i) {
            Price price = 101'0000 + (i % 100) * 100;
            book.addOrder(i, price, 100, Side::Sell);
        }
        
        // Measure matching aggressive orders
        size_t matchCount = 0;
        auto start = high_resolution_clock::now();
        
        for (size_t i = 0; i < N / 10; ++i) {
            book.addOrder(2 * N + i, 101'0000, 100, Side::Buy);
            matchCount++;
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double avgLatency = static_cast<double>(duration.count()) / matchCount;
        
        std::cout << "  Matched " << matchCount << " orders in " 
                  << duration.count() / 1000.0 << " ms\n";
        std::cout << "  Average matching latency: " << std::fixed << std::setprecision(3)
                  << avgLatency << " μs\n\n";
    }
    
    void benchmark_order_cancellation() {
        std::cout << "3. Order Cancellation Benchmark\n";
        
        OrderBook book;
        constexpr size_t N = 50'000;
        
        // Add orders
        std::vector<OrderId> orderIds;
        for (size_t i = 0; i < N; ++i) {
            Price price = 100'0000 + (i % 1000) * 100;
            book.addOrder(i, price, 100, Side::Buy);
            orderIds.push_back(i);
        }
        
        // Measure cancellations
        auto start = high_resolution_clock::now();
        
        for (size_t i = 0; i < N / 2; ++i) {
            book.cancelOrder(orderIds[i]);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double avgLatency = static_cast<double>(duration.count()) / (N / 2);
        
        std::cout << "  Cancelled " << N / 2 << " orders in " 
                  << duration.count() / 1000.0 << " ms\n";
        std::cout << "  Average cancellation latency: " << std::fixed << std::setprecision(3)
                  << avgLatency << " μs\n\n";
    }
    
    void benchmark_market_data_access() {
        std::cout << "4. Market Data Access Benchmark\n";
        
        OrderBook book;
        
        // Populate book
        for (size_t i = 0; i < 1000; ++i) {
            book.addOrder(i, 100'0000 - i * 100, 100, Side::Buy);
            book.addOrder(1000 + i, 101'0000 + i * 100, 100, Side::Sell);
        }
        
        constexpr size_t N = 1'000'000;
        
        auto start = high_resolution_clock::now();
        
        for (size_t i = 0; i < N; ++i) {
            volatile auto bid = book.bestBid();
            volatile auto ask = book.bestAsk();
            volatile auto mid = book.midPrice();
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);
        
        double avgLatency = static_cast<double>(duration.count()) / N;
        
        std::cout << "  " << N << " market data queries in " 
                  << duration.count() / 1'000'000.0 << " ms\n";
        std::cout << "  Average query latency: " << std::fixed << std::setprecision(0)
                  << avgLatency << " ns\n\n";
    }
    
    void benchmark_throughput() {
        std::cout << "5. Overall Throughput Benchmark\n";
        
        OrderBook book;
        std::mt19937 gen(42);
        std::uniform_int_distribution<> sideDist(0, 1);
        std::uniform_int_distribution<> priceDist(-100, 100);
        std::uniform_int_distribution<> qtyDist(10, 200);
        
        constexpr size_t N = 100'000;
        size_t tradeCount = 0;
        
        book.setTradeCallback([&tradeCount](const Trade&) {
            tradeCount++;
        });
        
        auto start = high_resolution_clock::now();
        
        for (size_t i = 0; i < N; ++i) {
            Side side = sideDist(gen) == 0 ? Side::Buy : Side::Sell;
            Price base = 100'0000;
            Price offset = priceDist(gen) * 100;
            Price price = base + offset;
            Quantity qty = qtyDist(gen);
            
            book.addOrder(i, price, qty, side);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        double opsPerSec = (N * 1000.0) / duration.count();
        
        std::cout << "  Processed " << N << " mixed operations in " 
                  << duration.count() << " ms\n";
        std::cout << "  Generated " << tradeCount << " trades\n";
        std::cout << "  Overall throughput: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " ops/sec\n";
        std::cout << "  Book depth: " << book.orderCount() << " orders\n\n";
    }
};

int main() {
    PerformanceBenchmark benchmark;
    benchmark.run();
    
    std::cout << "=== Benchmark Complete ===\n";
    std::cout << "\nNote: These benchmarks are single-threaded.\n";
    std::cout << "Production performance may vary based on hardware and workload.\n";
    
    return 0;
}
