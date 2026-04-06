#include <orderbook.hpp>
#include <iostream>
#include <iomanip>

using namespace orderbook;

void print_book_state(const OrderBook &book) {
    std::cout << "\n=== Order Book State ===\n";

    // Print asks (sorted lowest to highest)
    auto asks = book.getAsks(5);
    std::cout << "\nAsks:\n";
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        double price = static_cast<double>(it->price) / PRICE_SCALE;
        std::cout << std::fixed << std::setprecision(4) << "  " << price << " : " << it->quantity
                  << " (" << it->orderCount << " orders)\n";
    }

    // Print spread
    auto bestBid = book.bestBid();
    auto bestAsk = book.bestAsk();
    if (bestBid && bestAsk) {
        double bid = static_cast<double>(*bestBid) / PRICE_SCALE;
        double ask = static_cast<double>(*bestAsk) / PRICE_SCALE;
        double spread = static_cast<double>(*bestAsk - *bestBid) / PRICE_SCALE;
        std::cout << "\n  --- " << bid << " / " << ask << " (spread: " << spread << ") ---\n";
    }

    // Print bids (sorted highest to lowest)
    auto bids = book.getBids(5);
    std::cout << "\nBids:\n";
    for (const auto &level : bids) {
        double price = static_cast<double>(level.price) / PRICE_SCALE;
        std::cout << std::fixed << std::setprecision(4) << "  " << price << " : " << level.quantity
                  << " (" << level.orderCount << " orders)\n";
    }

    std::cout << "\nTotal orders: " << book.orderCount() << "\n";
}

int main() {
    std::cout << "=== Order Book Basic Example ===\n\n";

    OrderBook book;

    // Set up callbacks
    book.setTradeCallback([](const Trade &trade) {
        double price = static_cast<double>(trade.price) / PRICE_SCALE;
        std::cout << "TRADE: " << trade.quantity << " @ " << std::fixed << std::setprecision(4)
                  << price << " (maker: " << trade.makerOrderId << ", taker: " << trade.takerOrderId
                  << ")\n";
    });

    book.setOrderUpdateCallback([](const Order &order) {
        const char *side = (order.side == Side::Buy) ? "BUY" : "SELL";
        const char *status = "";
        switch (order.status) {
        case OrderStatus::New:
            status = "NEW";
            break;
        case OrderStatus::PartiallyFilled:
            status = "PARTIAL";
            break;
        case OrderStatus::Filled:
            status = "FILLED";
            break;
        case OrderStatus::Cancelled:
            status = "CANCELLED";
            break;
        case OrderStatus::Rejected:
            status = "REJECTED";
            break;
        }
        std::cout << "ORDER UPDATE: " << order.id << " " << side << " " << status
                  << " (filled: " << order.filledQuantity << "/" << order.quantity << ")\n";
    });

    std::cout << "1. Adding buy orders...\n";
    book.addOrder(1, 100'0000, 100, Side::Buy);  // 100.00 x 100
    book.addOrder(2, 99'5000, 200, Side::Buy);   // 99.50 x 200
    book.addOrder(3, 99'0000, 150, Side::Buy);   // 99.00 x 150

    std::cout << "\n2. Adding sell orders...\n";
    book.addOrder(4, 101'0000, 100, Side::Sell);  // 101.00 x 100
    book.addOrder(5, 101'5000, 200, Side::Sell);  // 101.50 x 200
    book.addOrder(6, 102'0000, 150, Side::Sell);  // 102.00 x 150

    print_book_state(book);

    std::cout << "\n3. Adding aggressive buy order that crosses spread...\n";
    book.addOrder(7, 101'5000, 250, Side::Buy);  // 101.50 x 250

    print_book_state(book);

    std::cout << "\n4. Adding market sell order...\n";
    book.addOrder(8, 0, 150, Side::Sell, OrderType::Market);

    print_book_state(book);

    std::cout << "\n5. Cancelling order...\n";
    book.cancelOrder(6);

    print_book_state(book);

    // Analytics
    Analytics analytics;
    book.setTradeCallback([&analytics](const Trade &trade) { analytics.recordTrade(trade); });

    // Generate some trades
    std::cout << "\n6. Generating more trades for analytics...\n";
    for (int i = 0; i < 10; ++i) {
        book.addOrder(100 + i, 100'0000 + (i * 1000), 50, Side::Buy);
        book.addOrder(200 + i, 100'0000 + (i * 1000), 50, Side::Sell);
    }

    auto stats = analytics.getStatistics();
    std::cout << "\n=== Analytics ===\n";
    std::cout << "Total trades: " << stats.tradeCount << "\n";
    std::cout << "Total volume: " << stats.totalVolume << "\n";
    std::cout << "VWAP: " << std::fixed << std::setprecision(4) << stats.vwap / PRICE_SCALE << "\n";
    std::cout << "Volatility: " << stats.volatility / PRICE_SCALE << "\n";
    std::cout << "High: " << static_cast<double>(stats.high) / PRICE_SCALE << "\n";
    std::cout << "Low: " << static_cast<double>(stats.low) / PRICE_SCALE << "\n";
    std::cout << "Avg trade size: " << stats.avgTradeSize << "\n";

    // Order book imbalance
    double imbalance = Analytics::calculateImbalance(book);
    std::cout << "\nOrder book imbalance: " << std::fixed << std::setprecision(2)
              << (imbalance * 100) << "% (positive = more buy pressure)\n";

    return 0;
}
