# High-Performance Limit Order Book (LOB)

A production-ready, low-latency limit order book implementation in modern C++ (C++20) designed for institutional and high-frequency trading (HFT) applications.

## Features

### Core Engine
- **Price-Time Priority Matching**: Standard matching algorithm used in most exchanges
- **Multiple Order Types**: Limit, Market, IOC (Immediate-or-Cancel), FOK (Fill-or-Kill)
- **Order Management**: Add, cancel, and modify orders with microsecond latency
- **Trade Execution**: Efficient matching engine with configurable callbacks

### Order Book Variants
- **`OrderBook`**: Standard tree-based limit order book with O(log n) level access
- **`PriceCollarOrderBook`**: Hard price-collar variant with O(1) amortised level access via a flat array indexed by price; rejects any order outside `[lowerBound, upperBound]`
- **`ConcurrentOrderBook`**: Multi-producer wrapper that serialises all mutations on a dedicated worker thread, eliminating lock contention on the critical matching path

### Performance Optimizations
- **Boost Intrusive List** (`boost::intrusive::list`): O(1) order removal at a price level — link nodes are embedded directly in each `Order` struct, eliminating the O(n) list scan of the old `std::list<Order*>` design
- **Boost Flat Hash Map** (`boost::unordered_flat_map`): Open-addressing order-ID lookup table with ~2× better throughput than `std::unordered_map` (no pointer-chasing, better CPU cache utilization)
- **Boost Circular Buffer** (`boost::circular_buffer`): Fixed-capacity ring buffer for trade history in `Analytics` — zero allocations after construction, guaranteed O(1) push_back, eliminates the `std::deque` segment overhead
- **Boost Object Pool** (`boost::object_pool`): Chunk-based node allocator for AVL tree nodes, replacing per-node `new`/`delete` with O(1) amortised alloc/free and bulk deallocation on clear
- **Custom Memory Pool**: Per-object pool for `Order` allocation — zero-allocation hot path, improved cache locality
- **Cache-Friendly Data Structures**: Struct layout tuned to fit within one 64-byte cache line
- **Fixed-Point Arithmetic**: Prices stored as 64-bit integers (scaled by 10 000) to avoid floating-point errors
- **Inline Functions**: Header-only design for maximum compiler inlining
- **`DynamicCircularQueue`**: O(1) amortised push/pop/remove ring buffer used per price level in `PriceCollarOrderBook`

### Applications Layer
- **Market Making**: Automated quote management with configurable spread and position limits
- **Arbitrage Detection**: Multi-book cross-exchange arbitrage opportunity scanner
- **Trading Analytics**: VWAP, volatility, volume profile, order book imbalance

### Thread Safety
- **Shared Mutex**: Read-write locks allow concurrent market-data readers with exclusive writer access
- **Atomic Operations**: Lock-free position tracking in `MarketMaker`
- **Thread-Safe Callbacks**: All event notifications are dispatched outside the lock to prevent re-entrancy deadlocks
- **`ConcurrentOrderBook`**: Single-consumer command queue fully serialises the matching path

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Applications Layer                     │
│  ┌──────────┐   ┌──────────────┐   ┌───────────────────┐ │
│  │  Market  │   │  Arbitrage   │   │     Analytics     │ │
│  │  Maker   │   │   Detector   │   │ (VWAP/Volatility) │ │
│  └──────────┘   └──────────────┘   └───────────────────┘ │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │
┌──────────────────────────────────────────────────────────┐
│                   Order Book Layer                        │
│  ┌──────────────────┐  ┌──────────────────┐              │
│  │    OrderBook     │  │ConcurrentOrderBook│              │
│  │  (tree-based,    │  │  (command-queue   │              │
│  │   O(log n))      │  │   serialisation)  │              │
│  └──────────────────┘  └──────────────────┘              │
│  ┌────────────────────────────────────────────────────┐  │
│  │           PriceCollarOrderBook                     │  │
│  │  (flat-array O(1) levels, [lower, upper] collar)   │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌──────────────┐  ┌────────────────┐  ┌─────────────┐   │
│  │  PriceLevel  │  │  MemoryPool    │  │  Matching   │   │
│  │  (FIFO list) │  │  (slab alloc)  │  │   Engine    │   │
│  └──────────────┘  └────────────────┘  └─────────────┘   │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │
┌──────────────────────────────────────────────────────────┐
│                  Data Structures Layer                    │
│  ┌───────────────────────────────┐  ┌──────────────────┐ │
│  │    DynamicCircularQueue<T>    │  │   AVLTree<T>     │ │
│  │  (ring buffer, O(1) remove)   │  │  (self-balancing │ │
│  └───────────────────────────────┘  │   BST, order     │ │
│                                     │   statistics)    │ │
│                                     └──────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

## Performance Characteristics

Based on benchmarks on modern hardware (single-threaded, `-O3 -march=native`):

| Operation             | Latency (avg) | Throughput      |
|-----------------------|--------------|-----------------|
| Order Insertion       | ~0.12 μs     | 8M+ ops/sec     |
| Order Matching        | ~0.13 μs     | 7M+ ops/sec     |
| Order Cancellation    | ~0.15 μs     | 6M+ ops/sec     |
| Market Data Access    | ~35 ns       | best bid/ask    |
| Overall Throughput    | —            | 8M+ ops/sec     |

*Improvements over the baseline (std::unordered_map + std::list + std::deque) come primarily from:*
- *`boost::unordered_flat_map`: ~2× faster order-ID lookup (open addressing vs. chaining)*
- *`boost::intrusive::list`: O(1) cancel at any position in a price level (was O(n))*
- *`boost::circular_buffer`: zero steady-state allocations in Analytics*
- *`boost::object_pool`: chunk-allocated AVL tree nodes, bulk-freed on clear*

`PriceCollarOrderBook` achieves O(1) amortised level access (vs. O(log n) for `OrderBook`) at the cost of a fixed memory footprint proportional to the collar width divided by tick size.

*Note: Actual performance depends on hardware, workload, and system configuration.*

## Building

### Requirements
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20 or higher
- **Boost 1.74 or higher** (header-only; no compiled Boost libraries required)

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
    auto spread  = book.spread();

    // Get market depth
    auto bids = book.getBids(10);  // Top 10 bid levels
    auto asks = book.getAsks(10);  // Top 10 ask levels

    return 0;
}
```

### Price Collar Order Book

`PriceCollarOrderBook` enforces a hard price range `[lowerBound, upperBound]` and
provides O(1) amortised level access via a flat array indexed by
`(price - lowerBound) / tickSize`.

```cpp
#include <orderbook.hpp>

using namespace orderbook;

int main() {
    // Collar from 90.00 to 110.00, tick size 0.01
    // Prices are fixed-point integers scaled by PRICE_SCALE (10 000)
    Price lower = 90'0000;   // 90.0000
    Price upper = 110'0000;  // 110.0000
    Price tick  = 100;       // 0.0100

    PriceCollarOrderBook book(lower, upper, tick);

    book.setTradeCallback([](const Trade& t) {
        std::cout << "Trade " << t.quantity << " @ " << t.price << "\n";
    });

    // Accepted — price is within collar
    book.addOrder(1, 100'0000, 50, Side::Buy);

    // Rejected — price is outside collar (returns false)
    bool ok = book.addOrder(2, 120'0000, 50, Side::Sell);
    assert(!ok);

    std::cout << book.priceInCollar(95'0000) << "\n";  // 1 (true)
    std::cout << book.priceInCollar(85'0000) << "\n";  // 0 (false)

    return 0;
}
```

### Concurrent Order Book

`ConcurrentOrderBook` wraps `OrderBook` with a single-consumer command queue,
allowing multiple producer threads to submit orders without contending on the
matching path.

```cpp
#include <orderbook.hpp>

using namespace orderbook;

int main() {
    ConcurrentOrderBook book;

    book.setTradeCallback([](const Trade& t) {
        // Invoked on the internal worker thread
        std::cout << "Trade " << t.quantity << " @ " << t.price << "\n";
    });

    // Returns a std::future<bool> — non-blocking for the caller
    auto f1 = book.submitAddOrder(1, 100'0000, 100, Side::Buy);
    auto f2 = book.submitAddOrder(2,  99'0000, 100, Side::Sell);

    bool ok1 = f1.get();  // block until the worker processes the command
    bool ok2 = f2.get();

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
    MarketMakerConfig config;
    config.spreadTicks = 10;      // 10 ticks spread
    config.quoteSize   = 100;     // 100 units per quote
    config.maxPosition = 1000;    // max 1000 units inventory

    MarketMaker mm(book, config);
    mm.start();

    // Market maker continuously quotes around mid price
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
        std::cout << "Buy  @ " << opportunity->buyPrice
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
    std::cout << "VWAP: "         << stats.vwap         << "\n";
    std::cout << "Volatility: "   << stats.volatility   << "\n";
    std::cout << "Total volume: " << stats.totalVolume  << "\n";

    // Order book imbalance (positive = buy-heavy, negative = sell-heavy)
    double imbalance = Analytics::calculateImbalance(book);
    std::cout << "Imbalance: " << (imbalance * 100) << "%\n";

    return 0;
}
```

## Design Decisions

### Price Representation
Prices are stored as 64-bit integers using fixed-point arithmetic (scaled by `PRICE_SCALE = 10 000`). This avoids floating-point precision issues common in financial applications.

```cpp
Price price = 100'0000;  // Represents 100.0000
Price price =  99'5500;  // Represents  99.5500
```

### Memory Management
A custom slab memory pool (`MemoryPool<T>`) is used for order allocation to:
- Eliminate per-order heap allocation overhead on the hot path
- Improve cache locality by keeping orders contiguous in memory
- Reduce fragmentation and memory management costs

AVL tree nodes are allocated via `boost::object_pool`, which pre-allocates nodes in large chunks and releases them all at once on `clear()`, eliminating per-node `new`/`delete` overhead.

### Boost Intrusive List in PriceLevel
Each `Order` carries a `boost::intrusive::list_member_hook` (two raw pointers, 16 bytes). `PriceLevel` stores an `boost::intrusive::list<Order>` that uses these embedded hooks instead of separate list nodes. Benefits:

- **O(1) cancel**: `iterator_to(*order)` converts a pointer to an iterator in constant time (hook offset is a compile-time constant), making `removeOrder` O(1) regardless of how many orders are at that price level.
- **Zero allocation**: no heap calls during add or remove; all storage lives inside the `Order` struct.
- **Better cache locality**: iterating the queue touches only `Order` objects (no pointer chase through separate list nodes).

### Boost Unordered Flat Map for Order Lookup
`boost::unordered_flat_map` uses open addressing with a flat contiguous array, which:
- Eliminates the per-bucket linked-list pointer chase of `std::unordered_map`
- Gives ~2× better lookup and insert throughput in benchmarks
- Has significantly lower memory overhead per entry

### Boost Circular Buffer for Analytics
`boost::circular_buffer<Trade>` pre-allocates a fixed-size contiguous array. When the buffer is full, `push_back` overwrites the oldest entry automatically — no `pop_front`, no reallocation, no branch.

### Data Structures
- **`OrderBook`** stores price levels in `std::map` (red-black tree), giving O(log n) insertion/lookup and naturally sorted iteration for market depth.
- **`PriceCollarOrderBook`** stores price levels in a flat `std::vector` indexed by `(price − lowerBound) / tickSize`, giving O(1) level access. Each level holds a `DynamicCircularQueue<Order>` for O(1) amortised push/pop/remove.
- **`DynamicCircularQueue<T>`** is a ring-buffer queue with tombstone-based removal backed by a hash map (`item → slot index`) for O(1) average arbitrary removal. The buffer is compacted in-place rather than grown when there is sufficient dead space.
- **`AVLTree<T>`** is a self-balancing binary search tree that additionally tracks subtree `size` and `lessThan` counts, supporting O(log n) order-statistic queries (`findKthNode`).

### Thread Safety
The order books use `std::shared_mutex` (read-write lock) to allow:
- Multiple concurrent readers for market data access
- Exclusive writer access for order mutations
- All callbacks dispatched **outside** the lock to prevent re-entrancy deadlocks

`ConcurrentOrderBook` takes a different approach: a single background worker thread drains a mutex-protected `std::queue<Command>`, so the matching path is never contended.

### Order Matching
The matching engine implements price-time priority:
1. Best price has priority (highest bid, lowest ask)
2. Among same-price orders, earlier orders have priority (FIFO)
3. Partial fills are supported
4. Special order types (IOC, FOK) are handled appropriately

## Testing

The project includes comprehensive test suites under `tests/`:

```bash
# Run all tests
cd build
ctest

# Or run individual test executables directly
./tests/test_order_book
./tests/test_matching
./tests/test_analytics
./tests/test_concurrency
./tests/test_performance
./tests/test_price_collar_order_book
```

Test coverage includes:
- Order management (add, cancel, modify)
- Price-time priority matching
- Partial fills and multiple matches
- Special order types (IOC, FOK)
- Market data queries
- Analytics calculations (including circular buffer capacity cap and resize)
- Intrusive list O(1) mid-level cancel
- Boost flat hash map duplicate rejection
- Thread safety (basic concurrency)
- Price collar enforcement (reject out-of-range orders, tick alignment)
- `DynamicCircularQueue` operations (push, pop, arbitrary remove, compaction, growth)
- `AVLTree` operations (insert, remove, order-statistic queries)

## Benchmarks

### Running the benchmarks

```bash
# Build first (if not already built)
mkdir build && cd build && cmake .. && cmake --build .

# Standard OrderBook benchmark
./examples/performance_benchmark
```

### OrderBook (tree-based, O(log n) level access)

The `performance_benchmark` executable runs five micro-benchmarks against `OrderBook`
on a single thread with `-O3 -march=native`:

| Benchmark | What is measured | Typical result |
|---|---|---|
| Order insertion | 100,000 resting limit orders added at 1,000 distinct price levels | ~0.18 μs / order |
| Order matching | 5,000 aggressive buy orders crossing a pre-populated sell side | ~0.19 μs / order |
| Order cancellation | 25,000 cancel requests against a live book | ~0.33 μs / order |
| Market data access | 1,000,000 `bestBid` + `bestAsk` + `midPrice` round-trips | ~46 ns / query |
| Mixed throughput | 100,000 random buy/sell inserts with natural matching | 5.8 M+ ops/sec |

Run and interpret:

```bash
./examples/performance_benchmark
# Sample output:
# 1. Order Insertion Benchmark
#    Inserted 100000 orders in 17.842 ms
#    Average latency: 0.178 μs
#    Throughput: 5606979 ops/sec
#
# 2. Order Matching Benchmark
#    Matched 5000 orders in 0.943 ms
#    Average matching latency: 0.189 μs
#
# 3. Order Cancellation Benchmark
#    Cancelled 25000 orders in 8.281 ms
#    Average cancellation latency: 0.331 μs
#
# 4. Market Data Access Benchmark
#    1000000 market data queries in 46.123 ms
#    Average query latency: 46 ns
#
# 5. Overall Throughput Benchmark
#    Processed 100000 mixed operations in 17 ms
#    Generated 12048 trades
#    Overall throughput: 5882352 ops/sec
```

### PriceCollarOrderBook (flat-array, O(1) amortised level access)

`PriceCollarOrderBook` stores price levels in a flat array indexed by
`(price - lowerBound) / tickSize`, so level lookups and best-bid/ask updates
are O(1) amortised instead of O(log n). This yields lower and more
predictable latency for books with a bounded, known price range.

Key performance differences vs `OrderBook`:

| Operation | OrderBook | PriceCollarOrderBook |
|---|---|---|
| Level access | O(log n) — red-black tree traversal | O(1) — direct array index |
| Best-bid / best-ask update | O(log n) | O(1) amortised (lazy scan) |
| Memory footprint | Proportional to active levels | Fixed: `(upperBound - lowerBound) / tickSize + 1` levels |
| Order insertion / cancellation | O(log n) | O(1) amortised |
| Per-level FIFO queue | `std::list` | `DynamicCircularQueue` — O(1) amortised remove |

When to prefer `PriceCollarOrderBook`:
- The instrument's valid price range is known in advance (e.g. an exchange-imposed collar)
- The tick size is coarse enough that the flat array fits in cache
- Ultra-low and consistent per-operation latency is required

When to prefer `OrderBook`:
- The price range is unbounded or very wide
- Memory footprint must remain proportional to active liquidity

### ConcurrentOrderBook (multi-producer, single-consumer worker thread)

`ConcurrentOrderBook` wraps `OrderBook` with a lock-free-style command queue,
serialising all mutations on a dedicated worker thread. This design is optimised
for **throughput under contention** from multiple producer threads:

| Aspect | Detail |
|---|---|
| Producer latency | Near-zero blocking — `submitAddOrder` enqueues a command and returns a `std::future<bool>` immediately |
| Worker throughput | Matches `OrderBook` throughput on the single matching thread |
| Scalability | Throughput increases as more producers are added (up to the worker's saturation point) |
| Read-path | Delegates directly to the underlying `OrderBook` shared_mutex (concurrent readers) |

```bash
# Concurrency stress test (part of the test suite)
cd build && ctest -R ConcurrencyTests -V
```

### Regression benchmarks (CI)

`tests/test_performance.cpp` runs conservative throughput and latency assertions
automatically on every build, catching severe regressions without requiring
manual inspection:

| Test | Threshold |
|---|---|
| `order_insertion_throughput` | ≥ 100,000 ops/sec |
| `order_cancellation_throughput` | ≥ 50,000 ops/sec |
| `order_matching_throughput` | ≥ 50,000 ops/sec |
| `market_data_latency` | ≤ 5,000 ns / query |
| `mixed_workload_throughput` | ≥ 50,000 ops/sec |

```bash
cd build && ./tests/test_performance
```

*Note: All figures above are from a single-threaded run on Intel/AMD x86_64 with `-O3 -march=native`. Actual results vary with hardware and system load.*

## API Reference

### OrderBook

**Methods:**
- `addOrder(id, price, qty, side, type)` — Add new order; returns `false` on duplicate ID
- `cancelOrder(id)` — Cancel existing order; returns `false` if ID not found
- `modifyOrder(id, price, qty)` — Cancel-replace an order; returns `false` if ID not found
- `getOrder(id)` — Retrieve a copy of the order (`std::optional<Order>`)
- `bestBid()` — Best bid price (`std::optional<Price>`)
- `bestAsk()` — Best ask price (`std::optional<Price>`)
- `spread()` — Bid-ask spread (`std::optional<Price>`)
- `midPrice()` — Mid price (`std::optional<Price>`)
- `getBids(depth)` — Top-N bid levels as `vector<LevelInfo>`
- `getAsks(depth)` — Top-N ask levels as `vector<LevelInfo>`
- `getVolumeAtPrice(price, side)` — Total resting quantity at a price level
- `orderCount()` / `bidLevelCount()` / `askLevelCount()` — Aggregate counters
- `clear()` — Remove all orders and reset the book

**Callbacks:**
- `setTradeCallback(fn)` — Invoked after each trade execution (outside the lock)
- `setOrderUpdateCallback(fn)` — Invoked after each order status change (outside the lock)

---

### PriceCollarOrderBook

Same interface as `OrderBook` plus:

**Constructor:**
- `PriceCollarOrderBook(lowerBound, upperBound, tickSize = 1)` — Both bounds inclusive, in fixed-point units. Throws `std::invalid_argument` if the parameters are invalid.

**Additional methods:**
- `priceInCollar(price)` — Returns `true` if `price` is within `[lowerBound, upperBound]` and tick-aligned
- `lowerBound()` / `upperBound()` / `tickSize()` — Accessors for collar parameters

**Behaviour differences:**
- `addOrder` / `modifyOrder` return `false` immediately if the price violates the collar (no book mutation occurs)
- Level access is O(1) amortised (flat array indexed by price)

---

### ConcurrentOrderBook

**Methods:**
- `submitAddOrder(id, price, qty, side, type)` → `std::future<bool>`
- `submitCancelOrder(id)` → `std::future<bool>`
- `submitModifyOrder(id, newPrice, newQty)` → `std::future<bool>`
- All read-only methods from `OrderBook` (`getOrder`, `bestBid`, `bestAsk`, `spread`, `midPrice`, `getBids`, `getAsks`, `orderCount`, …)
- `setTradeCallback(fn)` / `setOrderUpdateCallback(fn)` — Must be set **before** submitting any orders

---

### Analytics

**Methods:**
- `recordTrade(trade)` — Append a trade to the rolling history window
- `calculateVwap()` — Volume-weighted average price over the history window
- `calculateVolatility()` — Population standard deviation of trade prices
- `getStatistics()` — Aggregate `Statistics` struct: VWAP, volatility, high, low, total volume, trade count, average trade size
- `getVolumeProfile()` — Price → volume distribution over the history window
- `calculateImbalance(book, depth)` — `(bidVol − askVol) / (bidVol + askVol)` over top-`depth` levels; positive = buy-heavy
- `clear()` — Reset the trade history
- `setMaxHistory(n)` — Cap the rolling window to the most recent `n` trades (default: 10 000)

---

### MarketMaker

**Configuration (`MarketMakerConfig`):**
- `spreadTicks` — Half-spread in fixed-point tick units (default: 10)
- `quoteSize` — Units per bid/ask quote (default: 100)
- `maxPosition` — Inventory limit in units; quotes on a side are suppressed when the limit is reached (default: 1 000)
- `enabled` — Toggle quoting without stopping the thread (default: `true`)

**Methods:**
- `start()` — Launch the quoting thread
- `stop()` — Signal the thread to stop and join it
- `updateConfig(config)` — Hot-update configuration while running
- `position()` — Read the current net inventory (atomic)

---

### ArbitrageDetector

**Methods:**
- `detect()` — Scan all book pairs for crossed markets; returns the most profitable `Opportunity` (if any) as `std::optional<Opportunity>`
- `profitPercentage(opp)` — `(profit / buyPrice) × 100`

**`Opportunity` struct fields:**
- `buyPrice` / `sellPrice` — Prices at which to trade
- `maxQuantity` — Maximum executable quantity (min of the two top-level quantities)
- `profit` — `sellPrice − buyPrice` in fixed-point units
- `buyBookIdx` / `sellBookIdx` — Indices into the `books` vector

---

### DynamicCircularQueue\<T\>

A ring-buffer queue of non-owning `T*` pointers supporting O(1) amortised arbitrary removal.

**Methods:**
- `push(item)` — Append to the back; O(1) amortised
- `front()` — Return the front live pointer (skips tombstones); O(1) amortised
- `popFront()` — Remove the front live element; O(1) amortised
- `remove(item)` — Mark an arbitrary element as removed via hash-map lookup; O(1) average
- `empty()` / `size()` / `capacity()` — Non-mutating queries

---

### AVLTree\<T\>

A self-balancing BST with augmented order-statistic metadata.

**Methods:**
- `insert(value)` — Insert value; returns `false` if already present
- `remove(value)` — Remove value; returns `false` if not found
- `findKthNode(k)` — Return the k-th smallest element (1-indexed) as `std::optional<Node>`
- `clear()` — Delete all nodes

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
