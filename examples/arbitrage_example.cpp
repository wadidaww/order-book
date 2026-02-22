#include <orderbook.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>

using namespace orderbook;

void print_book(const std::string& name, const OrderBook& book) {
    auto bid = book.best_bid();
    auto ask = book.best_ask();
    
    std::cout << name << ": ";
    if (bid && ask) {
        double bid_price = static_cast<double>(*bid) / PRICE_SCALE;
        double ask_price = static_cast<double>(*ask) / PRICE_SCALE;
        std::cout << std::fixed << std::setprecision(4)
                  << bid_price << " / " << ask_price;
    } else {
        std::cout << "No quotes";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== Arbitrage Detection Example ===\n\n";
    
    // Create multiple order books representing different exchanges
    auto book1 = std::make_unique<OrderBook>();
    auto book2 = std::make_unique<OrderBook>();
    auto book3 = std::make_unique<OrderBook>();
    
    std::cout << "1. Setting up order books for 3 exchanges...\n\n";
    
    // Exchange 1 - Slightly higher prices
    book1->add_order(1, 100'0000, 100, Side::Buy);
    book1->add_order(2, 99'5000, 150, Side::Buy);
    book1->add_order(3, 101'0000, 100, Side::Sell);
    book1->add_order(4, 101'5000, 150, Side::Sell);
    
    // Exchange 2 - Normal prices
    book2->add_order(11, 99'8000, 120, Side::Buy);
    book2->add_order(12, 99'3000, 180, Side::Buy);
    book2->add_order(13, 100'5000, 120, Side::Sell);
    book2->add_order(14, 101'0000, 180, Side::Sell);
    
    // Exchange 3 - Lower prices (arbitrage opportunity!)
    book3->add_order(21, 101'2000, 200, Side::Buy);  // Higher bid!
    book3->add_order(22, 100'7000, 250, Side::Buy);
    book3->add_order(23, 102'0000, 200, Side::Sell);
    book3->add_order(24, 102'5000, 250, Side::Sell);
    
    std::cout << "Current market state:\n";
    print_book("Exchange 1", *book1);
    print_book("Exchange 2", *book2);
    print_book("Exchange 3", *book3);
    
    // Create arbitrage detector
    std::vector<OrderBook*> books = {book1.get(), book2.get(), book3.get()};
    ArbitrageDetector detector(books);
    
    std::cout << "\n2. Detecting arbitrage opportunities...\n\n";
    
    auto opportunity = detector.detect();
    
    if (opportunity) {
        double buy_price = static_cast<double>(opportunity->buy_price) / PRICE_SCALE;
        double sell_price = static_cast<double>(opportunity->sell_price) / PRICE_SCALE;
        double profit = static_cast<double>(opportunity->profit) / PRICE_SCALE;
        double profit_pct = ArbitrageDetector::profit_percentage(*opportunity);
        
        std::cout << "*** ARBITRAGE OPPORTUNITY DETECTED ***\n";
        std::cout << "Buy at Exchange " << (opportunity->buy_book_idx + 1) 
                  << " @ " << std::fixed << std::setprecision(4) << buy_price << "\n";
        std::cout << "Sell at Exchange " << (opportunity->sell_book_idx + 1) 
                  << " @ " << sell_price << "\n";
        std::cout << "Max quantity: " << opportunity->max_quantity << "\n";
        std::cout << "Profit per unit: " << profit << "\n";
        std::cout << "Profit percentage: " << std::setprecision(2) << profit_pct << "%\n";
        std::cout << "Total potential profit: " 
                  << std::fixed << std::setprecision(2)
                  << (profit * opportunity->max_quantity) << "\n";
        
        // Execute arbitrage
        std::cout << "\n3. Executing arbitrage trade...\n\n";
        
        OrderBook* buy_book = books[opportunity->buy_book_idx];
        OrderBook* sell_book = books[opportunity->sell_book_idx];
        
        Quantity exec_qty = std::min(opportunity->max_quantity, Quantity{100});
        
        // Buy on cheaper exchange
        bool buy_success = buy_book->add_order(
            1000, opportunity->buy_price, exec_qty, Side::Buy
        );
        
        // Sell on expensive exchange
        bool sell_success = sell_book->add_order(
            2000, opportunity->sell_price, exec_qty, Side::Sell
        );
        
        if (buy_success && sell_success) {
            std::cout << "Arbitrage executed successfully!\n";
            std::cout << "Bought " << exec_qty << " @ " << buy_price << "\n";
            std::cout << "Sold " << exec_qty << " @ " << sell_price << "\n";
            std::cout << "Realized profit: " << std::fixed << std::setprecision(2)
                      << (profit * exec_qty) << "\n";
        }
        
    } else {
        std::cout << "No arbitrage opportunities detected.\n";
    }
    
    std::cout << "\n4. Creating artificial arbitrage opportunity...\n\n";
    
    // Add orders that create clear arbitrage
    book1->add_order(100, 98'0000, 50, Side::Sell);  // Cheap on exchange 1
    book2->add_order(200, 99'0000, 50, Side::Buy);   // Expensive on exchange 2
    
    std::cout << "Updated market state:\n";
    print_book("Exchange 1", *book1);
    print_book("Exchange 2", *book2);
    print_book("Exchange 3", *book3);
    
    opportunity = detector.detect();
    
    if (opportunity) {
        double buy_price = static_cast<double>(opportunity->buy_price) / PRICE_SCALE;
        double sell_price = static_cast<double>(opportunity->sell_price) / PRICE_SCALE;
        double profit_pct = ArbitrageDetector::profit_percentage(*opportunity);
        
        std::cout << "\n*** NEW ARBITRAGE DETECTED ***\n";
        std::cout << "Buy @ " << std::fixed << std::setprecision(4) << buy_price 
                  << " on Exchange " << (opportunity->buy_book_idx + 1) << "\n";
        std::cout << "Sell @ " << sell_price 
                  << " on Exchange " << (opportunity->sell_book_idx + 1) << "\n";
        std::cout << "Profit: " << std::setprecision(2) << profit_pct << "%\n";
    }
    
    std::cout << "\n=== Arbitrage detection complete ===\n";
    
    return 0;
}
