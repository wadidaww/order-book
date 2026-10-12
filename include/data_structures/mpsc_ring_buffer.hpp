#pragma once

#include "../orderbook/cache.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ds {

// Spin-wait hint for busy loops (PAUSE on x86, YIELD on AArch64).
inline void cpuRelax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    asm volatile("yield" ::: "memory");
#endif
}

// Bounded multi-producer / single-consumer ring buffer (Vyukov-style).
//
// Each slot carries a sequence number that tells producers and the consumer
// whose turn it is, so there is no mutex anywhere:
//   • producers claim a slot with one CAS on the shared tail, construct the
//     element in place and publish it by bumping the slot's sequence;
//   • the single consumer reads the head without any atomic RMW, consumes
//     the element in place and hands the slot back to producers.
// Elements are constructed directly in the slot, so the steady state performs
// zero heap allocations.
//
// Capacity is rounded up to a power of two.  When the buffer is full,
// tryEmplace() returns false and emplace() spins until a slot frees up
// (natural back-pressure on producers).
template <typename T> class MpscRingBuffer {
  public:
    explicit MpscRingBuffer(size_t capacity)
        : mask_(roundUpPow2(capacity) - 1)
        , slots_(new Slot[mask_ + 1]) {
        for (size_t i = 0; i <= mask_; ++i)
            slots_[i].seq.store(i, std::memory_order_relaxed);
    }

    ~MpscRingBuffer() {
        // Destroy anything that was published but never consumed.
        while (tryConsume([](T &) {})) {
        }
    }

    MpscRingBuffer(const MpscRingBuffer &) = delete;
    MpscRingBuffer &operator=(const MpscRingBuffer &) = delete;

    [[nodiscard]] size_t capacity() const noexcept { return mask_ + 1; }

    // Construct an element at the tail.  Thread-safe for any number of
    // producers.  Returns false (without constructing) if the buffer is full.
    template <typename... Args> bool tryEmplace(Args &&...args) {
        size_t pos = tail_.load(std::memory_order_relaxed);
        for (;;) {
            Slot &slot = slots_[pos & mask_];
            size_t seq = slot.seq.load(std::memory_order_acquire);
            auto diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);
            if (diff == 0) {
                if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    ::new (slot.storage) T(std::forward<Args>(args)...);
                    // seq_cst (not just release) so that a consumer going to
                    // sleep and a producer checking for a sleeper cannot both
                    // miss each other (see ConcurrentOrderBook::enqueue).
                    slot.seq.store(pos + 1, std::memory_order_seq_cst);
                    return true;
                }
            } else if (diff < 0) {
                return false;  // full: the consumer has not freed this slot yet
            } else {
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    // Like tryEmplace() but waits for space instead of failing.
    template <typename... Args> void emplace(Args &&...args) {
        for (unsigned spins = 0; !tryEmplace(std::forward<Args>(args)...); ++spins) {
            if (spins < 64)
                cpuRelax();
            else
                std::this_thread::yield();
        }
    }

    // Consumer only.  If an element is available, invoke fn(T&) on it in
    // place, destroy it, release the slot and return true.
    template <typename Fn> bool tryConsume(Fn &&fn) {
        Slot &slot = slots_[head_ & mask_];
        if (slot.seq.load(std::memory_order_seq_cst) != head_ + 1)
            return false;
        T *item = std::launder(reinterpret_cast<T *>(slot.storage));
        fn(*item);
        item->~T();
        slot.seq.store(head_ + mask_ + 1, std::memory_order_release);
        ++head_;
        return true;
    }

    // Consumer only.  True if the next slot has been published.
    [[nodiscard]] bool hasPending() const noexcept {
        return slots_[head_ & mask_].seq.load(std::memory_order_seq_cst) == head_ + 1;
    }

  private:
    struct alignas(orderbook::detail::destructive_interference_size) Slot {
        std::atomic<size_t> seq;
        alignas(T) unsigned char storage[sizeof(T)];
    };

    static size_t roundUpPow2(size_t n) {
        if (n < 2)
            throw std::invalid_argument("MpscRingBuffer capacity must be at least 2");
        size_t p = 1;
        while (p < n)
            p <<= 1;
        return p;
    }

    const size_t mask_;
    std::unique_ptr<Slot[]> slots_;

    // Producers contend on tail_; head_ is touched by the consumer only.
    // Separate cache lines prevent false sharing between the two.
    alignas(orderbook::detail::destructive_interference_size) std::atomic<size_t> tail_{0};
    alignas(orderbook::detail::destructive_interference_size) size_t head_{0};
};

}  // namespace ds
