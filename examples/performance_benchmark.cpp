#include <orderbook.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>

using namespace orderbook;

class PerformanceBenchmark {
public:
    void run() {
        std::cout << "=== Order Book Performance Benchmark ===" << std::endl;
        std::cout << std::endl;
        
        benchmark_order_addition();
        benchmark_order_cancellation();
        benchmark_order_matching();
        benchmark_order_modification();
        benchmark_concurrent_operations();
        
        std::cout << std::endl;
        std::cout << "=== Benchmark Complete ===" << std::endl;
    }
    
private:
    void benchmark_order_addition() {
        std::cout << "Benchmark: Order Addition" << std::endl;
        
        OrderBook book(1);
        constexpr size_t num_orders = 100000;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<Price> price_dist(9000, 11000);
        std::uniform_int_distribution<Quantity> qty_dist(1, 1000);
        std::uniform_int_distribution<int> side_dist(0, 1);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_orders; ++i) {
            Side side = side_dist(gen) == 0 ? Side::Buy : Side::Sell;
            Price price = price_dist(gen);
            Quantity qty = qty_dist(gen);
            book.add_order(i + 1, side, price, qty);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_latency = static_cast<double>(duration.count()) / num_orders;
        double throughput = (num_orders * 1000000.0) / duration.count();
        
        std::cout << "  Orders added: " << num_orders << std::endl;
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(3) 
                  << avg_latency << " μs/order" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " orders/sec" << std::endl;
        std::cout << std::endl;
    }
    
    void benchmark_order_cancellation() {
        std::cout << "Benchmark: Order Cancellation" << std::endl;
        
        OrderBook book(1);
        constexpr size_t num_orders = 50000;
        
        // Add orders first
        std::vector<OrderId> order_ids;
        for (size_t i = 0; i < num_orders; ++i) {
            Price price = 10000 + (i % 200) - 100;
            book.add_order(i + 1, Side::Buy, price, 100);
            order_ids.push_back(i + 1);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (auto order_id : order_ids) {
            book.cancel_order(order_id);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_latency = static_cast<double>(duration.count()) / num_orders;
        double throughput = (num_orders * 1000000.0) / duration.count();
        
        std::cout << "  Orders cancelled: " << num_orders << std::endl;
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(3) 
                  << avg_latency << " μs/order" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " orders/sec" << std::endl;
        std::cout << std::endl;
    }
    
    void benchmark_order_matching() {
        std::cout << "Benchmark: Order Matching" << std::endl;
        
        size_t trade_count = 0;
        auto trade_callback = [&trade_count](const Trade&) {
            ++trade_count;
        };
        
        OrderBook book(1, trade_callback);
        constexpr size_t num_orders = 50000;
        
        // Add resting orders
        for (size_t i = 0; i < num_orders / 2; ++i) {
            book.add_order(i + 1, Side::Sell, 10100 + (i % 100), 100);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Add aggressive orders that will match
        for (size_t i = 0; i < num_orders / 2; ++i) {
            book.add_order(num_orders / 2 + i + 1, Side::Buy, 10200, 100);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_latency = static_cast<double>(duration.count()) / (num_orders / 2);
        double throughput = ((num_orders / 2) * 1000000.0) / duration.count();
        
        std::cout << "  Matching orders: " << num_orders / 2 << std::endl;
        std::cout << "  Trades executed: " << trade_count << std::endl;
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(3) 
                  << avg_latency << " μs/order" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " orders/sec" << std::endl;
        std::cout << std::endl;
    }
    
    void benchmark_order_modification() {
        std::cout << "Benchmark: Order Modification" << std::endl;
        
        OrderBook book(1);
        constexpr size_t num_orders = 50000;
        
        // Add orders first
        std::vector<OrderId> order_ids;
        for (size_t i = 0; i < num_orders; ++i) {
            book.add_order(i + 1, Side::Buy, 10000, 100);
            order_ids.push_back(i + 1);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (auto order_id : order_ids) {
            book.modify_order(order_id, 10050, 150);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_latency = static_cast<double>(duration.count()) / num_orders;
        double throughput = (num_orders * 1000000.0) / duration.count();
        
        std::cout << "  Orders modified: " << num_orders << std::endl;
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(3) 
                  << avg_latency << " μs/order" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " orders/sec" << std::endl;
        std::cout << std::endl;
    }
    
    void benchmark_concurrent_operations() {
        std::cout << "Benchmark: Concurrent Operations (Single-threaded simulation)" << std::endl;
        
        OrderBook book(1);
        constexpr size_t num_operations = 100000;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> op_dist(0, 2);  // 0=add, 1=cancel, 2=modify
        std::uniform_int_distribution<Price> price_dist(9000, 11000);
        std::uniform_int_distribution<Quantity> qty_dist(1, 1000);
        std::uniform_int_distribution<int> side_dist(0, 1);
        
        std::vector<OrderId> active_orders;
        OrderId next_order_id = 1;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_operations; ++i) {
            int operation = op_dist(gen);
            
            if (operation == 0 || active_orders.empty()) {
                // Add order
                Side side = side_dist(gen) == 0 ? Side::Buy : Side::Sell;
                Price price = price_dist(gen);
                Quantity qty = qty_dist(gen);
                if (book.add_order(next_order_id, side, price, qty)) {
                    active_orders.push_back(next_order_id);
                }
                ++next_order_id;
            } else if (operation == 1 && !active_orders.empty()) {
                // Cancel order
                size_t idx = gen() % active_orders.size();
                book.cancel_order(active_orders[idx]);
                active_orders.erase(active_orders.begin() + idx);
            } else if (!active_orders.empty()) {
                // Modify order
                size_t idx = gen() % active_orders.size();
                Price new_price = price_dist(gen);
                Quantity new_qty = qty_dist(gen);
                book.modify_order(active_orders[idx], new_price, new_qty);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_latency = static_cast<double>(duration.count()) / num_operations;
        double throughput = (num_operations * 1000000.0) / duration.count();
        
        std::cout << "  Total operations: " << num_operations << std::endl;
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(3) 
                  << avg_latency << " μs/operation" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " operations/sec" << std::endl;
        std::cout << std::endl;
    }
};

int main() {
    PerformanceBenchmark benchmark;
    benchmark.run();
    return 0;
}
