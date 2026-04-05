#include <orderbook.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>

using namespace orderbook;

void print_book(const std::string& name, const OrderBook& book) {
    auto bid = book.bestBid();
    auto ask = book.bestAsk();
    
    std::cout << name << ": ";
    if (bid && ask) {
        double bidPrice = static_cast<double>(*bid) / PRICE_SCALE;
        double askPrice = static_cast<double>(*ask) / PRICE_SCALE;
        std::cout << std::fixed << std::setprecision(4)
                  << bidPrice << " / " << askPrice;
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
    book1->addOrder(1, 100'0000, 100, Side::Buy);
    book1->addOrder(2, 99'5000, 150, Side::Buy);
    book1->addOrder(3, 101'0000, 100, Side::Sell);
    book1->addOrder(4, 101'5000, 150, Side::Sell);
    
    // Exchange 2 - Normal prices
    book2->addOrder(11, 99'8000, 120, Side::Buy);
    book2->addOrder(12, 99'3000, 180, Side::Buy);
    book2->addOrder(13, 100'5000, 120, Side::Sell);
    book2->addOrder(14, 101'0000, 180, Side::Sell);
    
    // Exchange 3 - Lower prices (arbitrage opportunity!)
    book3->addOrder(21, 101'2000, 200, Side::Buy);  // Higher bid!
    book3->addOrder(22, 100'7000, 250, Side::Buy);
    book3->addOrder(23, 102'0000, 200, Side::Sell);
    book3->addOrder(24, 102'5000, 250, Side::Sell);
    
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
        double buyPrice = static_cast<double>(opportunity->buyPrice) / PRICE_SCALE;
        double sellPrice = static_cast<double>(opportunity->sellPrice) / PRICE_SCALE;
        double profit = static_cast<double>(opportunity->profit) / PRICE_SCALE;
        double profitPct = ArbitrageDetector::profitPercentage(*opportunity);
        
        std::cout << "*** ARBITRAGE OPPORTUNITY DETECTED ***\n";
        std::cout << "Buy at Exchange " << (opportunity->buyBookIdx + 1) 
                  << " @ " << std::fixed << std::setprecision(4) << buyPrice << "\n";
        std::cout << "Sell at Exchange " << (opportunity->sellBookIdx + 1) 
                  << " @ " << sellPrice << "\n";
        std::cout << "Max quantity: " << opportunity->maxQuantity << "\n";
        std::cout << "Profit per unit: " << profit << "\n";
        std::cout << "Profit percentage: " << std::setprecision(2) << profitPct << "%\n";
        std::cout << "Total potential profit: " 
                  << std::fixed << std::setprecision(2)
                  << (profit * opportunity->maxQuantity) << "\n";
        
        // Execute arbitrage
        std::cout << "\n3. Executing arbitrage trade...\n\n";
        
        OrderBook* buyBook = books[opportunity->buyBookIdx];
        OrderBook* sellBook = books[opportunity->sellBookIdx];
        
        Quantity execQty = std::min(opportunity->maxQuantity, Quantity{100});
        
        // Buy on cheaper exchange
        bool buySuccess = buyBook->addOrder(
            1000, opportunity->buyPrice, execQty, Side::Buy
        );
        
        // Sell on expensive exchange
        bool sellSuccess = sellBook->addOrder(
            2000, opportunity->sellPrice, execQty, Side::Sell
        );
        
        if (buySuccess && sellSuccess) {
            std::cout << "Arbitrage executed successfully!\n";
            std::cout << "Bought " << execQty << " @ " << buyPrice << "\n";
            std::cout << "Sold " << execQty << " @ " << sellPrice << "\n";
            std::cout << "Realized profit: " << std::fixed << std::setprecision(2)
                      << (profit * execQty) << "\n";
        }
        
    } else {
        std::cout << "No arbitrage opportunities detected.\n";
    }
    
    std::cout << "\n4. Creating artificial arbitrage opportunity...\n\n";
    
    // Add orders that create clear arbitrage
    book1->addOrder(100, 98'0000, 50, Side::Sell);  // Cheap on exchange 1
    book2->addOrder(200, 99'0000, 50, Side::Buy);   // Expensive on exchange 2
    
    std::cout << "Updated market state:\n";
    print_book("Exchange 1", *book1);
    print_book("Exchange 2", *book2);
    print_book("Exchange 3", *book3);
    
    opportunity = detector.detect();
    
    if (opportunity) {
        double buyPrice = static_cast<double>(opportunity->buyPrice) / PRICE_SCALE;
        double sellPrice = static_cast<double>(opportunity->sellPrice) / PRICE_SCALE;
        double profitPct = ArbitrageDetector::profitPercentage(*opportunity);
        
        std::cout << "\n*** NEW ARBITRAGE DETECTED ***\n";
        std::cout << "Buy @ " << std::fixed << std::setprecision(4) << buyPrice 
                  << " on Exchange " << (opportunity->buyBookIdx + 1) << "\n";
        std::cout << "Sell @ " << sellPrice 
                  << " on Exchange " << (opportunity->sellBookIdx + 1) << "\n";
        std::cout << "Profit: " << std::setprecision(2) << profitPct << "%\n";
    }
    
    std::cout << "\n=== Arbitrage detection complete ===\n";
    
    return 0;
}
