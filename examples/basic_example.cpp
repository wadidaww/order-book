#include <orderbook.hpp>
#include <iostream>
#include <iomanip>

using namespace orderbook;

void print_book_state(const OrderBook& book) {
    std::cout << "\n=== Order Book State ===\n";
    
    // Print asks (sorted lowest to highest)
    auto asks = book.get_asks(5);
    std::cout << "\nAsks:\n";
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        double price = static_cast<double>(it->price) / PRICE_SCALE;
        std::cout << std::fixed << std::setprecision(4)
                  << "  " << price << " : " << it->quantity 
                  << " (" << it->order_count << " orders)\n";
    }
    
    // Print spread
    auto best_bid = book.best_bid();
    auto best_ask = book.best_ask();
    if (best_bid && best_ask) {
        double bid = static_cast<double>(*best_bid) / PRICE_SCALE;
        double ask = static_cast<double>(*best_ask) / PRICE_SCALE;
        double spread = static_cast<double>(*best_ask - *best_bid) / PRICE_SCALE;
        std::cout << "\n  --- " << bid << " / " << ask 
                  << " (spread: " << spread << ") ---\n";
    }
    
    // Print bids (sorted highest to lowest)
    auto bids = book.get_bids(5);
    std::cout << "\nBids:\n";
    for (const auto& level : bids) {
        double price = static_cast<double>(level.price) / PRICE_SCALE;
        std::cout << std::fixed << std::setprecision(4)
                  << "  " << price << " : " << level.quantity 
                  << " (" << level.order_count << " orders)\n";
    }
    
    std::cout << "\nTotal orders: " << book.order_count() << "\n";
}

int main() {
    std::cout << "=== Order Book Basic Example ===\n\n";
    
    OrderBook book;
    
    // Set up callbacks
    book.set_trade_callback([](const Trade& trade) {
        double price = static_cast<double>(trade.price) / PRICE_SCALE;
        std::cout << "TRADE: " << trade.quantity << " @ " 
                  << std::fixed << std::setprecision(4) << price
                  << " (maker: " << trade.maker_order_id 
                  << ", taker: " << trade.taker_order_id << ")\n";
    });
    
    book.set_order_update_callback([](const Order& order) {
        const char* side = (order.side == Side::Buy) ? "BUY" : "SELL";
        const char* status = "";
        switch (order.status) {
            case OrderStatus::New: status = "NEW"; break;
            case OrderStatus::PartiallyFilled: status = "PARTIAL"; break;
            case OrderStatus::Filled: status = "FILLED"; break;
            case OrderStatus::Cancelled: status = "CANCELLED"; break;
            case OrderStatus::Rejected: status = "REJECTED"; break;
        }
        std::cout << "ORDER UPDATE: " << order.id << " " << side 
                  << " " << status << " (filled: " << order.filled_quantity 
                  << "/" << order.quantity << ")\n";
    });
    
    std::cout << "1. Adding buy orders...\n";
    book.add_order(1, 100'0000, 100, Side::Buy);   // 100.00 x 100
    book.add_order(2, 99'5000, 200, Side::Buy);    // 99.50 x 200
    book.add_order(3, 99'0000, 150, Side::Buy);    // 99.00 x 150
    
    std::cout << "\n2. Adding sell orders...\n";
    book.add_order(4, 101'0000, 100, Side::Sell);  // 101.00 x 100
    book.add_order(5, 101'5000, 200, Side::Sell);  // 101.50 x 200
    book.add_order(6, 102'0000, 150, Side::Sell);  // 102.00 x 150
    
    print_book_state(book);
    
    std::cout << "\n3. Adding aggressive buy order that crosses spread...\n";
    book.add_order(7, 101'5000, 250, Side::Buy);   // 101.50 x 250
    
    print_book_state(book);
    
    std::cout << "\n4. Adding market sell order...\n";
    book.add_order(8, 0, 150, Side::Sell, OrderType::Market);
    
    print_book_state(book);
    
    std::cout << "\n5. Cancelling order...\n";
    book.cancel_order(6);
    
    print_book_state(book);
    
    // Analytics
    Analytics analytics;
    book.set_trade_callback([&analytics](const Trade& trade) {
        analytics.record_trade(trade);
    });
    
    // Generate some trades
    std::cout << "\n6. Generating more trades for analytics...\n";
    for (int i = 0; i < 10; ++i) {
        book.add_order(100 + i, 100'0000 + (i * 1000), 50, Side::Buy);
        book.add_order(200 + i, 100'0000 + (i * 1000), 50, Side::Sell);
    }
    
    auto stats = analytics.get_statistics();
    std::cout << "\n=== Analytics ===\n";
    std::cout << "Total trades: " << stats.trade_count << "\n";
    std::cout << "Total volume: " << stats.total_volume << "\n";
    std::cout << "VWAP: " << std::fixed << std::setprecision(4) 
              << stats.vwap / PRICE_SCALE << "\n";
    std::cout << "Volatility: " << stats.volatility / PRICE_SCALE << "\n";
    std::cout << "High: " << static_cast<double>(stats.high) / PRICE_SCALE << "\n";
    std::cout << "Low: " << static_cast<double>(stats.low) / PRICE_SCALE << "\n";
    std::cout << "Avg trade size: " << stats.avg_trade_size << "\n";
    
    // Order book imbalance
    double imbalance = Analytics::calculate_imbalance(book);
    std::cout << "\nOrder book imbalance: " << std::fixed << std::setprecision(2)
              << (imbalance * 100) << "% (positive = more buy pressure)\n";
    
    return 0;
}
