#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <new>

namespace orderbook {

// High-performance memory pool for order allocation
// Reduces allocation overhead and improves cache locality
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

    // Non-copyable, non-movable for safety
    MemoryPool(const MemoryPool &) = delete;
    MemoryPool &operator=(const MemoryPool &) = delete;
    MemoryPool(MemoryPool &&) = delete;
    MemoryPool &operator=(MemoryPool &&) = delete;

    template <typename... Args> T *allocate(Args &&...args) {
        if (currentSlot_ >= BlockSize) {
            allocateBlock();
        }

        void *ptr = currentBlock_ + currentSlot_;
        ++currentSlot_;

        return new (ptr) T(std::forward<Args>(args)...);
    }

    void deallocate(T *ptr) noexcept {
        if (ptr) {
            ptr->~T();
            // Note: Memory is not returned to the pool, only reused on clear
        }
    }

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

    std::vector<void *> blocks_;
    T *currentBlock_;
    size_t currentSlot_;
};

}  // namespace orderbook
