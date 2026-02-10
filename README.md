# High-Performance Limit Order Book (C++20)

A production-ready, ultra-low-latency limit order book implementation designed for institutional trading and high-frequency trading (HFT) applications.

## Features

### Core Functionality
- **Price-Time Priority Matching**: Industry-standard FIFO matching algorithm
- **Full Order Lifecycle**: Add, cancel, modify operations with microsecond latencies
- **Multiple Order Types**: Limit, Market, Stop Loss, Stop Limit
- **Time-In-Force Support**: GTC, IOC, FOK, GTD
- **Thread-Safe Operations**: Shared mutex for concurrent read access
- **Zero-Allocation Design**: Memory pool for order management

### Performance Optimizations
- **Cache-Aligned Data Structures**: 64-byte aligned orders for optimal CPU cache utilization
- **Intrusive Linked Lists**: No heap allocations during order operations
- **Fixed-Point Arithmetic**: Integer-based price representation for deterministic performance
- **Lock-Free Read Operations**: Concurrent market data queries without blocking
- **Compiler Optimizations**: `-O3 -march=native -flto` for maximum performance

### Trading Applications
- **Market Analytics**: VWAP, TWAP, volatility calculations
- **Market Making**: Simple market maker implementation with configurable spreads
- **Arbitrage Detection**: Cross-exchange opportunity identification
- **Real-Time Market Data**: Order book depth, best bid/ask, spread tracking

## Architecture

### Data Structures

```
OrderBook
├── Price Levels (std::map)
│   ├── Bid Side (highest to lowest)
│   └── Ask Side (lowest to highest)
├── Price Level (intrusive doubly-linked list)
│   └── Orders (FIFO queue)
└── Order Pool (memory pool allocator)
```

### Key Components

- **Order**: Cache-aligned structure with intrusive list pointers
- **PriceLevel**: Manages orders at a specific price with O(1) operations
- **OrderBook**: Main matching engine with price-time priority
- **OrderPool**: Zero-allocation memory pool for order objects
- **MarketAnalytics**: Real-time statistics and trading metrics

## Performance Characteristics

### Latency Targets (x86_64, Release build)
- Order Add (non-matching): < 500 ns (p99)
- Order Cancel: < 400 ns (p99)
- Order Modify: < 600 ns (p99)
- Order Matching: < 1000 ns (p99)

### Throughput
- > 1M orders/sec (single-threaded)
- > 500K matches/sec (single-threaded)

## Building

### Prerequisites
- CMake 3.20+
- C++20 compliant compiler (GCC 11+, Clang 13+, MSVC 2019+)

### Build Instructions

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Build Options
- `BUILD_EXAMPLES=ON`: Build example programs (default: ON)
- `BUILD_TESTS=ON`: Build test suite (default: ON)
- `BUILD_BENCHMARKS=ON`: Build performance benchmarks (default: ON)

## Usage

### Basic Example

```cpp
#include <orderbook.hpp>

using namespace orderbook;

// Create order book with callbacks
auto trade_callback = [](const Trade& trade) {
    std::cout << "Trade executed: " << trade.quantity 
              << " @ " << trade.price << std::endl;
};

OrderBook book(1, trade_callback);

// Add orders
book.add_order(1, Side::Buy, 10000, 100);   // Buy 100 @ 100.00
book.add_order(2, Side::Sell, 10100, 100);  // Sell 100 @ 101.00

// Query market data
auto best_bid = book.best_bid();
auto best_ask = book.best_ask();
auto spread = book.spread();
auto mid_price = book.mid_price();

// Cancel order
book.cancel_order(1);

// Modify order
book.modify_order(2, 10050, 150);
```

### Market Making Example

```cpp
#include <orderbook.hpp>

using namespace orderbook;

OrderBook book(1);
MarketAnalytics analytics;

// Create market maker with 1.00 spread, 100 quantity
SimpleMarketMaker mm(100, 100);

// Generate quotes based on market conditions
auto quote = mm.generate_quote(book, analytics);

// Place market maker orders
book.add_order(id++, Side::Buy, quote.bid_price, quote.bid_quantity);
book.add_order(id++, Side::Sell, quote.ask_price, quote.ask_quantity);
```

### Arbitrage Detection

```cpp
#include <orderbook.hpp>

using namespace orderbook;

OrderBook exchange1(1);
OrderBook exchange2(1);

ArbitrageDetector detector;

// Detect arbitrage opportunities
auto opportunity = detector.detect(exchange1, exchange2, 
                                   transaction_cost);

if (opportunity) {
    std::cout << "Arbitrage: Buy @ " << opportunity->buy_price
              << " Sell @ " << opportunity->sell_price
              << " Profit: " << opportunity->profit_bps << " bps"
              << std::endl;
}
```

## Examples

Run the included examples:

```bash
# Simple order book operations
./examples/simple_example

# Market making simulation
./examples/market_maker_example

# Arbitrage detection
./examples/arbitrage_example

# Performance benchmark
./examples/performance_benchmark
```

## Testing

```bash
cd build
ctest --verbose
```

Or run tests directly:

```bash
./tests/test_order_book
```

## Benchmarks

Run latency benchmarks:

```bash
./benchmarks/latency_benchmark
```

Expected output:
```
=== Ultra Low-Latency Benchmark ===

Benchmark: Single Order Add Latency (warm cache)
  Samples: 10000
  Mean: 450.23 ns
  Min: 320 ns
  Max: 2140 ns
  ...
```

## Design Decisions

### Why Fixed-Point Arithmetic?
- Deterministic performance (no floating-point rounding)
- Faster than floating-point on most architectures
- Exact price representation

### Why Intrusive Lists?
- Zero allocations during order operations
- Cache-friendly memory layout
- Predictable latency

### Why std::map for Price Levels?
- Automatic price ordering (no manual sorting)
- Logarithmic lookup (acceptable for limited price levels)
- Red-black tree implementation (balanced)
- Could be replaced with a custom skip list or array-based structure for even better performance

## Thread Safety

The OrderBook uses `std::shared_mutex` for thread safety:
- Multiple concurrent readers (market data queries)
- Single writer (order operations)
- Lock-free for read-only operations when no writes pending

For true lock-free operation in multi-threaded environments, consider:
- Per-symbol order books with thread affinity
- Lock-free queues for order submission
- Atomic operations for critical paths

## Future Enhancements

- [ ] SIMD optimizations for order matching
- [ ] Lock-free data structures for multi-threaded access
- [ ] Order book snapshots for recovery
- [ ] Market replay for backtesting
- [ ] Additional order types (Iceberg, TWAP, VWAP)
- [ ] Cross-order book trading
- [ ] FIX protocol adapter
- [ ] Market data feed handlers

## Performance Tuning

### Compiler Flags
```bash
# Maximum optimization
-O3 -march=native -mtune=native -flto

# Profile-guided optimization (2-step process)
1. cmake -DCMAKE_CXX_FLAGS="-fprofile-generate" ..
2. Run benchmarks
3. cmake -DCMAKE_CXX_FLAGS="-fprofile-use" ..
```

### System Tuning
```bash
# CPU affinity
taskset -c 0 ./benchmark

# Disable frequency scaling
cpupower frequency-set -g performance

# Huge pages (Linux)
echo 1024 > /proc/sys/vm/nr_hugepages
```

## License

MIT License - see LICENSE file for details

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style
- All tests pass
- New features include tests
- Performance benchmarks show no regression

## References

- "Trading and Exchanges" by Larry Harris
- "High-Performance Trading" by Michael Perklin
- "Algorithmic Trading" by Ernest Chan
- ITCH Protocol Specification
- FIX Protocol Specification