#include <data_structures.hpp>
#include "../test_framework.hpp"
#include <atomic>
#include <thread>
#include <vector>

using namespace ds;
using namespace test;

TEST(mpsc_capacity_rounds_up_to_power_of_two) {
    MpscRingBuffer<int> q(5);
    ASSERT_EQ(q.capacity(), 8u);
}

TEST(mpsc_fifo_and_full) {
    MpscRingBuffer<int> q(4);
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(q.tryEmplace(i));
    ASSERT_FALSE(q.tryEmplace(99));  // full

    for (int i = 0; i < 4; ++i) {
        int got = -1;
        ASSERT_TRUE(q.tryConsume([&](int &v) { got = v; }));
        ASSERT_EQ(got, i);
    }
    ASSERT_FALSE(q.hasPending());
    ASSERT_FALSE(q.tryConsume([](int &) {}));

    // Slots are reusable after wrapping around.
    ASSERT_TRUE(q.tryEmplace(7));
    ASSERT_TRUE(q.hasPending());
    int got = -1;
    ASSERT_TRUE(q.tryConsume([&](int &v) { got = v; }));
    ASSERT_EQ(got, 7);
}

namespace {
struct Counted {
    static inline std::atomic<int> live{0};
    explicit Counted(int v)
        : value(v) {
        ++live;
    }
    ~Counted() { --live; }
    Counted(const Counted &) = delete;
    int value;
};
}  // namespace

TEST(mpsc_destroys_consumed_and_leftover_elements) {
    {
        MpscRingBuffer<Counted> q(8);
        for (int i = 0; i < 5; ++i)
            q.emplace(i);
        ASSERT_EQ(Counted::live.load(), 5);
        ASSERT_TRUE(q.tryConsume([](Counted &) {}));
        ASSERT_EQ(Counted::live.load(), 4);
    }
    ASSERT_EQ(Counted::live.load(), 0);
}

// Several producers push through a deliberately tiny buffer so they keep
// hitting the full condition and wrap around many times.  Every item must be
// delivered exactly once and each producer's items must stay in order.
TEST(mpsc_multi_producer_exactly_once_in_order) {
    constexpr int PRODUCERS = 4;
    constexpr int PER_PRODUCER = 20'000;
    MpscRingBuffer<std::pair<int, int>> q(16);

    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&q, p] {
            for (int i = 0; i < PER_PRODUCER; ++i)
                q.emplace(p, i);
        });
    }

    std::vector<int> next(PRODUCERS, 0);
    bool ordered = true;
    int received = 0;
    while (received < PRODUCERS * PER_PRODUCER) {
        if (q.tryConsume([&](std::pair<int, int> &item) {
                if (item.second != next[item.first])
                    ordered = false;
                next[item.first] = item.second + 1;
            })) {
            ++received;
        } else {
            std::this_thread::yield();
        }
    }
    for (auto &t : producers)
        t.join();

    ASSERT_TRUE(ordered);
    for (int p = 0; p < PRODUCERS; ++p)
        ASSERT_EQ(next[p], PER_PRODUCER);
    ASSERT_FALSE(q.hasPending());
}

int main() {
    return TestSuite::instance().run();
}
