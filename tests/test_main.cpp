#include <iostream>

// Forward declarations
void run_order_book_tests();
void run_analytics_tests();

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "Order Book Test Suite" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;
    
    try {
        run_order_book_tests();
        run_analytics_tests();
        
        std::cout << "====================================" << std::endl;
        std::cout << "ALL TESTS PASSED" << std::endl;
        std::cout << "====================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
