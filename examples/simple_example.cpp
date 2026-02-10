#include <orderbook.hpp>
#include <iostream>
#include <iomanip>

using namespace orderbook;

int main() {
    std::cout << "=== Simple Order Book Example ===" << std::endl;
    std::cout << std::endl;
    
    // Create trade callback
    auto trade_callback = [](const Trade& trade) {
        std::cout << "TRADE: "
                  << "Buyer=" << trade.buyer_order_id
                  << " Seller=" << trade.seller_order_id
                  << " Price=" << trade.price
                  << " Quantity=" << trade.quantity
                  << std::endl;
    };
    
    // Create execution callback
    auto exec_callback = [](const ExecutionReport& report) {
        std::cout << "EXEC: "
                  << "OrderId=" << report.order_id
                  << " Type=" << to_string(report.exec_type)
                  << " Status=" << to_string(report.order_status)
                  << " Side=" << to_string(report.side)
                  << " Price=" << report.price
                  << " Filled=" << report.filled_quantity << "/" << report.quantity
                  << std::endl;
    };
    
    // Create order book for symbol 1
    OrderBook book(1, trade_callback, exec_callback);
    
    std::cout << "Adding orders to the book..." << std::endl;
    std::cout << std::endl;
    
    // Add some buy orders
    book.add_order(1, Side::Buy, 10000, 100);  // Buy 100 @ 100.00
    book.add_order(2, Side::Buy, 9950, 150);   // Buy 150 @ 99.50
    book.add_order(3, Side::Buy, 9900, 200);   // Buy 200 @ 99.00
    
    // Add some sell orders
    book.add_order(4, Side::Sell, 10050, 100);  // Sell 100 @ 100.50
    book.add_order(5, Side::Sell, 10100, 150);  // Sell 150 @ 101.00
    book.add_order(6, Side::Sell, 10150, 200);  // Sell 200 @ 101.50
    
    std::cout << std::endl;
    std::cout << "Order book state:" << std::endl;
    
    // Display market data
    auto best_bid_price = book.best_bid();
    auto best_ask_price = book.best_ask();
    auto spread_value = book.spread();
    auto mid_price_value = book.mid_price();
    
    if (best_bid_price) {
        std::cout << "Best Bid: " << *best_bid_price / 100.0 << std::endl;
    }
    if (best_ask_price) {
        std::cout << "Best Ask: " << *best_ask_price / 100.0 << std::endl;
    }
    if (spread_value) {
        std::cout << "Spread: " << *spread_value / 100.0 << std::endl;
    }
    if (mid_price_value) {
        std::cout << "Mid Price: " << *mid_price_value / 100.0 << std::endl;
    }
    
    auto [bid_depth, ask_depth] = book.depth();
    std::cout << "Depth: " << bid_depth << " bid levels, " << ask_depth << " ask levels" << std::endl;
    
    // Get market depth
    std::cout << std::endl;
    std::cout << "Market Depth (Top 5 levels):" << std::endl;
    std::cout << std::setw(15) << "BID PRICE" << std::setw(15) << "BID QTY" << "  |  "
              << std::setw(15) << "ASK PRICE" << std::setw(15) << "ASK QTY" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    auto [bid_levels, ask_levels] = book.get_depth(5);
    
    size_t max_levels = std::max(bid_levels.size(), ask_levels.size());
    for (size_t i = 0; i < max_levels; ++i) {
        if (i < bid_levels.size()) {
            std::cout << std::setw(15) << bid_levels[i].price / 100.0
                      << std::setw(15) << bid_levels[i].quantity;
        } else {
            std::cout << std::setw(30) << "";
        }
        
        std::cout << "  |  ";
        
        if (i < ask_levels.size()) {
            std::cout << std::setw(15) << ask_levels[i].price / 100.0
                      << std::setw(15) << ask_levels[i].quantity;
        }
        
        std::cout << std::endl;
    }
    
    // Execute a marketable order that will match
    std::cout << std::endl;
    std::cout << "Adding marketable buy order (will match)..." << std::endl;
    std::cout << std::endl;
    book.add_order(7, Side::Buy, 10100, 75);  // Buy 75 @ 101.00 (will match with order 4)
    
    std::cout << std::endl;
    std::cout << "Updated market state:" << std::endl;
    best_bid_price = book.best_bid();
    best_ask_price = book.best_ask();
    if (best_bid_price) {
        std::cout << "Best Bid: " << *best_bid_price / 100.0 << std::endl;
    }
    if (best_ask_price) {
        std::cout << "Best Ask: " << *best_ask_price / 100.0 << std::endl;
    }
    
    // Cancel an order
    std::cout << std::endl;
    std::cout << "Cancelling order 2..." << std::endl;
    std::cout << std::endl;
    book.cancel_order(2);
    
    auto [new_bid_depth, new_ask_depth] = book.depth();
    std::cout << "Updated Depth: " << new_bid_depth << " bid levels, " 
              << new_ask_depth << " ask levels" << std::endl;
    
    return 0;
}
