#pragma once

#include "cache.hpp"
#include <memory>
#include <vector>
#include <cstddef>
#include <new>

namespace orderbook {

// Slab-style memory pool for fixed-size object allocation.
//
// Allocates objects in large contiguous blocks (slabs) of `BlockSize` elements.
// When a slab is exhausted a new one is allocated via ::operator new.
//
// Benefits over per-object heap allocation:
//   • No allocation overhead on the hot order-add path
//   • Improved cache locality: consecutive orders land in adjacent memory
//   • No per-block deallocation during normal operation (only on clear/destroy)
//
// Limitations:
//   • deallocate() only calls the destructor; the slot is not reused until
//     clear() resets the pool.  The pool is therefore suitable for workloads
//     that bulk-reset (e.g., end of trading session) rather than fine-grained
//     churn.
//   • Non-copyable and non-movable for safety.
template <typename T, size_t BlockSize = 4096> class MemoryPool {
  public:
    MemoryPool()
        : currentBlock_(nullptr)
        , currentSlot_(0) {
        allocateBlock();
    }

    ~MemoryPool() {
        for (auto *block : blocks_) {
            ::operator delete(block);
        }
    }

    // Non-copyable, non-movable — the pool owns raw memory blocks and raw
    // pointers into them; copying or moving would invalidate those pointers.
    MemoryPool(const MemoryPool &) = delete;
    MemoryPool &operator=(const MemoryPool &) = delete;
    MemoryPool(MemoryPool &&) = delete;
    MemoryPool &operator=(MemoryPool &&) = delete;

    // Construct a T in-place in the pool and return a pointer to it.
    // Allocates a new block if the current one is exhausted.  O(1) amortised.
    template <typename... Args> T *allocate(Args &&...args) {
        if (currentSlot_ >= BlockSize) {
            allocateBlock();
        }

        void *ptr = currentBlock_ + currentSlot_;
        ++currentSlot_;

        return new (ptr) T(std::forward<Args>(args)...);
    }

    // Destroy the object at ptr (calls ~T()) but does not return the slot to
    // the pool.  Slots are only reclaimed by clear().
    void deallocate(T *ptr) noexcept {
        if (ptr) {
            ptr->~T();
        }
    }

    // Reset the allocation cursor to the start of the first block, effectively
    // reclaiming all slots.  Does NOT call destructors — callers must ensure
    // all live objects have been destroyed before calling clear().
    void clear() {
        currentSlot_ = 0;
        if (!blocks_.empty()) {
            currentBlock_ = static_cast<T *>(blocks_[0]);
        }
    }

    [[nodiscard]] size_t capacity() const noexcept { return blocks_.size() * BlockSize; }

    [[nodiscard]] size_t size() const noexcept {
        return (blocks_.size() - 1) * BlockSize + currentSlot_;
    }

  private:
    void allocateBlock() {
        void *newBlock = ::operator new(BlockSize * sizeof(T));
        blocks_.push_back(newBlock);
        currentBlock_ = static_cast<T *>(newBlock);
        currentSlot_ = 0;
    }

    // blocks_ is accessed only when growing the pool (cold path).  Placing it
    // on its own cache line prevents it from polluting the cache line that
    // holds the hot allocation cursor (currentBlock_ + currentSlot_).
    alignas(detail::destructive_interference_size) std::vector<void *> blocks_;
    T *currentBlock_;
    size_t currentSlot_;
};

}  // namespace orderbook
