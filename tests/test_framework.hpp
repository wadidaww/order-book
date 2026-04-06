#pragma once
#include <iostream>
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

    int run() {
        int passed = 0;
        int failed = 0;

        std::cout << "Running " << tests_.size() << " tests...\n\n";

        for (const auto &[name, test] : tests_) {
            try {
                test();
                std::cout << "✓ " << name << "\n";
                passed++;
            } catch (const std::exception &e) {
                std::cout << "✗ " << name << " - " << e.what() << "\n";
                failed++;
            }
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

}  // namespace test
