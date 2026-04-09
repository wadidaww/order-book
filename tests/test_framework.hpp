#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <vector>

// Simple test framework
namespace test {

class TestSuite {
  public:
    static TestSuite &instance() {
        static TestSuite suite;
        return suite;
    }

    void add_test(const std::string &name, std::function<void()> test) {
        tests_.push_back({name, test});
    }

    size_t run() {
        size_t passed = 0;
        size_t failed = 0;

        std::cout << "Running " << tests_.size() << " tests...\n\n";

        for (const auto &[name, test] : tests_) {
            try {
                test();
                std::cout << "\033[32m";  // green for success
                std::cout << "✓ " << name << "\n";
                passed++;
            } catch (const std::exception &e) {
                std::cerr << "\033[31m";  // red for failures
                std::cout << "✗ " << name << " - " << e.what() << "\n";
                failed++;
            }
            // reset
            std::cout << "\033[0m";
        }

        std::cout << "\n" << passed << " passed, " << failed << " failed\n";
        return failed;
    }

  private:
    struct Test {
        std::string name;
        std::function<void()> func;
    };

    std::vector<Test> tests_;
};

class TestRegistrar {
  public:
    TestRegistrar(const std::string &name, std::function<void()> test) {
        TestSuite::instance().add_test(name, test);
    }
};

#define TEST(name)                                                                                 \
    void test_##name();                                                                            \
    static test::TestRegistrar registrar_##name(#name, test_##name);                               \
    void test_##name()

#define ASSERT(condition)                                                                          \
    if (!(condition)) {                                                                            \
        throw std::runtime_error("Assertion failed: " #condition);                                 \
    }

#define ASSERT_EQ(a, b)                                                                            \
    if ((a) != (b)) {                                                                              \
        throw std::runtime_error("Assertion failed: " #a " == " #b);                               \
    }

#define ASSERT_TRUE(condition) ASSERT(condition)
#define ASSERT_FALSE(condition) ASSERT(!(condition))

// Helper: throw a descriptive error when a threshold is violated
inline void check_throughput(const double actual, const double minimum, const char *label) {
    if (actual < minimum) {
        std::ostringstream ss;
        ss << label << " throughput too low: " << static_cast<long long>(actual) << " ops/sec"
           << " (minimum required: " << static_cast<long long>(minimum) << " ops/sec)";
        throw std::runtime_error(ss.str());
    }
}

// Helper: throw a descriptive error when a threshold is violated
inline void check_latency(const double actual_ns, const double max_ns, const char *label) {
    if (actual_ns > max_ns) {
        std::ostringstream ss;
        ss << label << " latency too high: " << static_cast<long long>(actual_ns) << " ns"
           << " (maximum allowed: " << static_cast<long long>(max_ns) << " ns)";
        throw std::runtime_error(ss.str());
    }
}

}  // namespace test
