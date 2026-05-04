#pragma once

#include <unordered_map>
#include <vector>
#include <cstddef>

namespace ds {

// A dynamic circular queue (ring buffer) whose elements are non-owning
// pointers of type T*.
//
// Complexity:
//   push     - O(1) amortized  (doubles capacity on growth)
//   front    - O(1) amortized  (lazy tombstone skipping)
//   pop_front - O(1) amortized
//   remove   - O(1) average   (hash map lookup + tombstone marking)
//
// Elements removed via remove() are marked as tombstones (nullptr) and
// are transparently skipped when front()/pop_front() advances the head.
// The buffer is compacted instead of grown when there is sufficient
// dead space, keeping memory use proportional to the live element count.
template <typename T>
class DynamicCircularQueue {
  public:
    explicit DynamicCircularQueue(size_t initialCapacity = 16)
        : buf_(initialCapacity, nullptr)
        , head_(0)
        , tail_(0)
        , active_(0)
        , capacity_(initialCapacity) {}

    // Non-copyable; movable.
    DynamicCircularQueue(const DynamicCircularQueue &) = delete;
    DynamicCircularQueue &operator=(const DynamicCircularQueue &) = delete;

    DynamicCircularQueue(DynamicCircularQueue &&) = default;
    DynamicCircularQueue &operator=(DynamicCircularQueue &&) = default;

    // Add item to the back. O(1) amortized.
    void push(T *item) {
        if (active_ == capacity_) {
            grow();
        } else if (usedSlots() == capacity_) {
            // Buffer is full of tombstones; compact without growing.
            compact();
        }
        buf_[tail_] = item;
        pos_[item] = tail_;
        tail_ = (tail_ + 1) % capacity_;
        ++active_;
    }

    // Return the front live element, or nullptr if empty. O(1) amortized.
    [[nodiscard]] T *front() {
        skipTombstones();
        return (active_ > 0) ? buf_[head_] : nullptr;
    }

    // Remove and discard the front live element. O(1) amortized.
    void popFront() {
        skipTombstones();
        if (active_ > 0) {
            pos_.erase(buf_[head_]);
            buf_[head_] = nullptr;
            head_ = (head_ + 1) % capacity_;
            --active_;
        }
    }

    // Mark an arbitrary item as removed in O(1) average.
    // Returns true if the item was present, false otherwise.
    bool remove(T *item) {
        auto it = pos_.find(item);
        if (it == pos_.end()) {
            return false;
        }
        buf_[it->second] = nullptr;  // tombstone
        pos_.erase(it);
        --active_;
        return true;
    }

    [[nodiscard]] bool empty() const noexcept { return active_ == 0; }
    [[nodiscard]] size_t size() const noexcept { return active_; }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

  private:
    // Number of slots currently occupied (live + tombstone).
    // When head_ == tail_ the ring buffer is either full or empty; we
    // disambiguate using active_ (the live-element count).
    [[nodiscard]] size_t usedSlots() const noexcept {
        if (head_ == tail_) {
            return active_ > 0 ? capacity_ : 0;
        }
        return (tail_ > head_) ? (tail_ - head_) : (capacity_ - head_ + tail_);
    }

    // Advance head_ past tombstones at the front.
    void skipTombstones() noexcept {
        while (active_ > 0 && buf_[head_] == nullptr) {
            head_ = (head_ + 1) % capacity_;
        }
    }

    // Re-pack live elements contiguously starting at index 0.
    // Called when the buffer is full of tombstones but active_ < capacity_.
    void compact() {
        std::vector<T *> newBuf(capacity_, nullptr);
        size_t newTail = 0;
        const size_t slots = usedSlots();
        size_t cur = head_;
        for (size_t i = 0; i < slots; ++i, cur = (cur + 1) % capacity_) {
            if (buf_[cur] != nullptr) {
                newBuf[newTail] = buf_[cur];
                pos_[buf_[cur]] = newTail;
                ++newTail;
            }
        }
        buf_ = std::move(newBuf);
        head_ = 0;
        tail_ = newTail;
    }

    // Double capacity, preserving element order and updating pos_ entries.
    void grow() {
        const size_t newCap = capacity_ * 2;
        std::vector<T *> newBuf(newCap, nullptr);
        size_t newTail = 0;
        const size_t slots = usedSlots();
        size_t cur = head_;
        for (size_t i = 0; i < slots; ++i, cur = (cur + 1) % capacity_) {
            if (buf_[cur] != nullptr) {
                newBuf[newTail] = buf_[cur];
                pos_[buf_[cur]] = newTail;
                ++newTail;
            }
        }
        buf_ = std::move(newBuf);
        capacity_ = newCap;
        head_ = 0;
        tail_ = newTail;
    }

    std::vector<T *> buf_;
    size_t head_;
    size_t tail_;
    size_t active_;    // count of live (non-tombstone) elements
    size_t capacity_;
    std::unordered_map<T *, size_t> pos_;  // item -> slot index in buf_
};

}  // namespace ds
