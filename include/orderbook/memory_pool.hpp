#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <new>

namespace orderbook {

// High-performance memory pool for order allocation
// Reduces allocation overhead and improves cache locality
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    MemoryPool() : current_block_(nullptr), current_slot_(0) {
        allocate_block();
    }
    
    ~MemoryPool() {
        for (auto* block : blocks_) {
            ::operator delete(block);
        }
    }
    
    // Non-copyable, non-movable for safety
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;
    
    template<typename... Args>
    T* allocate(Args&&... args) {
        if (current_slot_ >= BlockSize) {
            allocate_block();
        }
        
        void* ptr = current_block_ + current_slot_;
        ++current_slot_;
        
        return new (ptr) T(std::forward<Args>(args)...);
    }
    
    void deallocate(T* ptr) noexcept {
        if (ptr) {
            ptr->~T();
            // Note: Memory is not returned to the pool, only reused on clear
        }
    }
    
    void clear() {
        current_slot_ = 0;
        if (!blocks_.empty()) {
            current_block_ = static_cast<T*>(blocks_[0]);
        }
    }
    
    [[nodiscard]] size_t capacity() const noexcept {
        return blocks_.size() * BlockSize;
    }
    
    [[nodiscard]] size_t size() const noexcept {
        return (blocks_.size() - 1) * BlockSize + current_slot_;
    }

private:
    void allocate_block() {
        void* new_block = ::operator new(BlockSize * sizeof(T));
        blocks_.push_back(new_block);
        current_block_ = static_cast<T*>(new_block);
        current_slot_ = 0;
    }
    
    std::vector<void*> blocks_;
    T* current_block_;
    size_t current_slot_;
};

} // namespace orderbook
