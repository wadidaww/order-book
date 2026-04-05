# High-Performance Limit Order Book (LOB)

A production-ready, low-latency limit order book implementation in modern C++ (C++20) designed for institutional and high-frequency trading (HFT) applications.

## Features

### Core Engine
- **Price-Time Priority Matching**: Standard matching algorithm used in most exchanges
- **Multiple Order Types**: Limit, Market, IOC (Immediate-or-Cancel), FOK (Fill-or-Kill)
- **Order Management**: Add, cancel, and modify orders with microsecond latency
- **Trade Execution**: Efficient matching engine with configurable callbacks

### Performance Optimizations
- **Memory Pool**: Custom allocator for zero-allocation order management
- **Cache-Friendly Data Structures**: Optimized memory layout for L1/L2 cache
- **Lock-Free Operations**: Read-write locks for concurrent access
- **Fixed-Point Arithmetic**: Price representation avoiding floating-point errors
- **Inline Functions**: Header-only design for compiler optimizations

### Applications Layer
- **Market Making**: Automated quote management with position limits
- **Arbitrage Detection**: Multi-book arbitrage opportunity scanner
- **Trading Analytics**: VWAP, volatility, volume profile, order book imbalance

### Thread Safety
- **Shared Mutex**: Read-write locks for concurrent market data access
- **Atomic Operations**: Lock-free position tracking
- **Thread-Safe Callbacks**: Event notifications for trades and order updates

## Architecture

```
┌─────────────────────────────────────────────┐
│          Applications Layer                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  Market  │  │Arbitrage │  │Analytics │  │
│  │  Maker   │  │ Detector │  │          │  │
│  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────┘
                    ▲
                    │
┌─────────────────────────────────────────────┐
│           Order Book Engine                  │
│  ┌────────────────────────────────────────┐ │
│  │  Matching Engine (Price-Time Priority) │ │
│  └────────────────────────────────────────┘ │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │Price     │  │  Order   │  │ Memory   │  │
│  │Levels    │  │Management│  │  Pool    │  │
│  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────┘
```

## Performance Characteristics

Based on benchmarks on modern hardware (single-threaded):

- **Order Insertion**: ~0.18 μs average latency
- **Order Matching**: ~0.19 μs average latency
- **Order Cancellation**: ~0.33 μs average latency
- **Market Data Access**: ~46 ns (best bid/ask)
- **Throughput**: 5.8M+ operations/second

*Note: Actual performance depends on hardware, workload, and system configuration. Results above from Intel/AMD x86_64 with -O3 -march=native optimizations.*

## Building

### Requirements
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20 or higher

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/wadidaww/order-book.git
cd order-book

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run tests
ctest

# Run examples
./examples/basic_example
./examples/market_maker_example
./examples/arbitrage_example
./examples/performance_benchmark
```

## Usage

### Basic Order Book

```cpp
#include <orderbook.hpp>

using namespace orderbook;

int main() {
    OrderBook book;
    
    // Set up callbacks
    book.setTradeCallback([](const Trade& trade) {
        std::cout << "Trade: " << trade.quantity 
                  << " @ " << trade.price << "\n";
    });
    
    // Add orders
    book.addOrder(1, 100'0000, 100, Side::Buy);    // Buy 100 @ 100.00
    book.addOrder(2, 101'0000, 100, Side::Sell);   // Sell 100 @ 101.00
    
    // Get market data
    auto bestBid = book.bestBid();
    auto bestAsk = book.bestAsk();
    auto spread = book.spread();
    
    // Get market depth
    auto bids = book.getBids(10);  // Top 10 bid levels
    auto asks = book.getAsks(10);  // Top 10 ask levels
    
    return 0;
}
```

### Market Making

```cpp
#include <orderbook.hpp>

using namespace orderbook;

int main() {
    OrderBook book;
    
    // Configure market maker
    MarketMaker::Config config;
    config.spreadTicks = 10;       // 10 ticks spread
    config.quoteSize = 100;        // 100 units per quote
    config.maxPosition = 1000;     // Max 1000 units inventory
    
    MarketMaker mm(book, config);
    mm.start();
    
    // Market maker will continuously quote around mid price
    // ...
    
    mm.stop();
    return 0;
}
```

### Arbitrage Detection

```cpp
#include <orderbook.hpp>

using namespace orderbook;

int main() {
    OrderBook book1, book2, book3;
    
    // ... populate books ...
    
    std::vector<OrderBook*> books = {&book1, &book2, &book3};
    ArbitrageDetector detector(books);
    
    auto opportunity = detector.detect();
    if (opportunity) {
        std::cout << "Arbitrage found!\n";
        std::cout << "Buy @ " << opportunity->buyPrice 
                  << " on book " << opportunity->buyBookIdx << "\n";
        std::cout << "Sell @ " << opportunity->sellPrice 
                  << " on book " << opportunity->sellBookIdx << "\n";
        std::cout << "Profit: " 
                  << ArbitrageDetector::profitPercentage(*opportunity) 
                  << "%\n";
    }
    
    return 0;
}
```

### Analytics

```cpp
#include <orderbook.hpp>

using namespace orderbook;

int main() {
    OrderBook book;
    Analytics analytics;
    
    book.setTradeCallback([&](const Trade& trade) {
        analytics.recordTrade(trade);
    });
    
    // ... execute trades ...
    
    // Get statistics
    auto stats = analytics.getStatistics();
    std::cout << "VWAP: " << stats.vwap << "\n";
    std::cout << "Volatility: " << stats.volatility << "\n";
    std::cout << "Total volume: " << stats.totalVolume << "\n";
    
    // Order book imbalance
    double imbalance = Analytics::calculateImbalance(book);
    std::cout << "Imbalance: " << (imbalance * 100) << "%\n";
    
    return 0;
}
```

## Design Decisions

### Price Representation
Prices are stored as 64-bit integers using fixed-point arithmetic (scaled by 10,000). This avoids floating-point precision issues common in financial applications.

```cpp
Price price = 100'0000;  // Represents 100.00
Price price = 99'5500;   // Represents 99.55
```

### Memory Management
A custom memory pool is used for order allocation to:
- Eliminate allocation overhead during order processing
- Improve cache locality by keeping orders contiguous in memory
- Reduce fragmentation and memory management costs

### Thread Safety
The order book uses shared mutexes (read-write locks) to allow:
- Multiple concurrent readers for market data access
- Exclusive writer access for order modifications
- Lock-free atomic operations where applicable

### Order Matching
The matching engine implements price-time priority:
1. Best price has priority (highest bid, lowest ask)
2. Among same-price orders, earlier orders have priority (FIFO)
3. Partial fills are supported
4. Special order types (IOC, FOK) are handled appropriately

## Testing

The project includes comprehensive tests:

```bash
# Run all tests
cd build
ctest

# Or run test executable directly
./tests/test_order_book
```

Test coverage includes:
- Order management (add, cancel, modify)
- Price-time priority matching
- Partial fills and multiple matches
- Special order types (IOC, FOK)
- Market data queries
- Analytics calculations
- Thread safety (basic)

## Benchmarks

Run performance benchmarks:

```bash
./examples/performance_benchmark
```

This measures:
- Order insertion latency
- Matching engine latency
- Order cancellation latency
- Market data access latency
- Overall throughput

## API Reference

### OrderBook

**Methods:**
- `addOrder(id, price, qty, side, type)` - Add new order
- `cancelOrder(id)` - Cancel existing order
- `modifyOrder(id, price, qty)` - Modify order (cancel-replace)
- `getOrder(id)` - Retrieve order details
- `bestBid()` - Get best bid price
- `bestAsk()` - Get best ask price
- `spread()` - Get bid-ask spread
- `midPrice()` - Get mid price
- `getBids(depth)` - Get bid levels
- `getAsks(depth)` - Get ask levels
- `getVolumeAtPrice(price, side)` - Get volume at price level

**Callbacks:**
- `setTradeCallback(callback)` - Called when trades execute
- `setOrderUpdateCallback(callback)` - Called when order status changes

### Analytics

**Methods:**
- `recordTrade(trade)` - Record trade for analysis
- `calculateVwap()` - Volume weighted average price
- `calculateVolatility()` - Price volatility (std dev)
- `getStatistics()` - Comprehensive statistics
- `getVolumeProfile()` - Price-volume distribution
- `calculateImbalance(book, depth)` - Order book imbalance

### MarketMaker

**Methods:**
- `start()` - Start market making
- `stop()` - Stop market making
- `updateConfig(config)` - Update configuration
- `position()` - Get current position

### ArbitrageDetector

**Methods:**
- `detect()` - Detect arbitrage opportunities
- `profitPercentage(opp)` - Calculate profit percentage

## Production Considerations

For production use, consider:

1. **Persistence**: Add order book snapshots and replay logs
2. **Risk Management**: Implement position limits, circuit breakers
3. **Monitoring**: Add metrics, logging, and alerting
4. **Network**: Integrate with exchange APIs and market data feeds
5. **Backtesting**: Add historical data replay capabilities
6. **Order ID Generation**: Use distributed ID generation for multiple instances
7. **Self-Trade Prevention**: Add logic to prevent self-matching
8. **Time-in-Force**: Add more order time qualifiers (GTC, GTD, etc.)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Disclaimer

This is a reference implementation for educational and development purposes. While designed with performance and correctness in mind, it should be thoroughly tested and audited before use in production trading systems. The authors assume no liability for any financial losses incurred through use of this software.