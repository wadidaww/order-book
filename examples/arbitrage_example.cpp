#include <orderbook.hpp>
#include <iostream>
#include <iomanip>

using namespace orderbook;

int main() {
    std::cout << "=== Arbitrage Detection Example ===" << std::endl;
    std::cout << std::endl;
    
    // Create two order books for the same symbol on different exchanges
    OrderBook exchange1(1);
    OrderBook exchange2(1);
    
    std::cout << "Setting up order books on two exchanges..." << std::endl;
    std::cout << std::endl;
    
    // Exchange 1: Lower prices
    exchange1.add_order(1, Side::Buy, 9950, 100);
    exchange1.add_order(2, Side::Buy, 9900, 150);
    exchange1.add_order(3, Side::Sell, 10050, 100);
    exchange1.add_order(4, Side::Sell, 10100, 150);
    
    // Exchange 2: Higher prices (arbitrage opportunity)
    exchange2.add_order(11, Side::Buy, 10150, 100);
    exchange2.add_order(12, Side::Buy, 10100, 150);
    exchange2.add_order(13, Side::Sell, 10250, 100);
    exchange2.add_order(14, Side::Sell, 10300, 150);
    
    // Display market state
    auto print_book_state = [](const std::string& name, const OrderBook& book) {
        std::cout << name << ":" << std::endl;
        auto bid = book.best_bid();
        auto ask = book.best_ask();
        auto mid = book.mid_price();
        
        if (bid) std::cout << "  Best Bid: " << *bid / 100.0 << std::endl;
        if (ask) std::cout << "  Best Ask: " << *ask / 100.0 << std::endl;
        if (mid) std::cout << "  Mid Price: " << *mid / 100.0 << std::endl;
        std::cout << std::endl;
    };
    
    print_book_state("Exchange 1", exchange1);
    print_book_state("Exchange 2", exchange2);
    
    // Create arbitrage detector
    ArbitrageDetector detector;
    
    // Check for arbitrage opportunities
    std::cout << "Checking for arbitrage opportunities..." << std::endl;
    std::cout << std::endl;
    
    // Without transaction costs
    auto opportunity1 = detector.detect(exchange1, exchange2, 0);
    
    if (opportunity1) {
        std::cout << "*** ARBITRAGE OPPORTUNITY FOUND ***" << std::endl;
        std::cout << "  Buy at: " << opportunity1->buy_price / 100.0 << std::endl;
        std::cout << "  Sell at: " << opportunity1->sell_price / 100.0 << std::endl;
        std::cout << "  Quantity: " << opportunity1->quantity << std::endl;
        std::cout << "  Profit: " << (opportunity1->sell_price - opportunity1->buy_price) / 100.0 
                  << " (" << opportunity1->profit_bps << " bps)" << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << "No arbitrage opportunity found (without costs)" << std::endl;
        std::cout << std::endl;
    }
    
    // With transaction costs (0.10 per side = 0.20 total)
    Price transaction_cost = 20;
    auto opportunity2 = detector.detect(exchange1, exchange2, transaction_cost);
    
    if (opportunity2) {
        std::cout << "*** ARBITRAGE OPPORTUNITY FOUND (with costs) ***" << std::endl;
        std::cout << "  Buy at: " << opportunity2->buy_price / 100.0 << std::endl;
        std::cout << "  Sell at: " << opportunity2->sell_price / 100.0 << std::endl;
        std::cout << "  Quantity: " << opportunity2->quantity << std::endl;
        std::cout << "  Transaction Cost: " << transaction_cost / 100.0 << std::endl;
        std::cout << "  Net Profit: " 
                  << (opportunity2->sell_price - opportunity2->buy_price - transaction_cost) / 100.0
                  << " (" << opportunity2->profit_bps << " bps)" << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << "No arbitrage opportunity found (with costs of " 
                  << transaction_cost / 100.0 << ")" << std::endl;
        std::cout << std::endl;
    }
    
    // Simulate narrowing the spread on exchange 2
    std::cout << "Adjusting Exchange 2 prices (narrowing spread)..." << std::endl;
    exchange2.cancel_order(11);
    exchange2.cancel_order(13);
    exchange2.add_order(15, Side::Buy, 10050, 100);
    exchange2.add_order(16, Side::Sell, 10150, 100);
    
    std::cout << std::endl;
    print_book_state("Exchange 2 (Updated)", exchange2);
    
    auto opportunity3 = detector.detect(exchange1, exchange2, transaction_cost);
    
    if (opportunity3) {
        std::cout << "*** ARBITRAGE OPPORTUNITY STILL EXISTS ***" << std::endl;
        std::cout << "  Profit: " << opportunity3->profit_bps << " bps" << std::endl;
    } else {
        std::cout << "Arbitrage opportunity closed after price adjustment" << std::endl;
    }
    
    return 0;
}
