#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <type_traits>

namespace KhDetector {

/**
 * @brief Lock-free single-producer/single-consumer ring buffer
 * 
 * This ring buffer is designed for real-time audio applications where one thread
 * produces data and another consumes it without blocking. It uses atomic operations
 * for thread safety without locks.
 * 
 * @tparam T The type of elements stored in the buffer
 * @tparam Size The size of the buffer (must be power of 2 for optimal performance)
 */
template<typename T, size_t Size>
class RingBuffer
{
    static_assert(Size > 0, "RingBuffer size must be greater than 0");
    static_assert((Size & (Size - 1)) == 0, "RingBuffer size must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for lock-free operation");

public:
    /**
     * @brief Construct a new Ring Buffer object
     */
    RingBuffer() : readIndex_(0), writeIndex_(0) {}

    /**
     * @brief Destroy the Ring Buffer object
     */
    ~RingBuffer() = default;

    // Non-copyable and non-movable for simplicity
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    /**
     * @brief Push a single element to the buffer (producer side)
     * 
     * @param item The item to push
     * @return true if the item was successfully pushed, false if buffer is full
     */
    bool push(const T& item)
    {
        const size_t currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const size_t nextWrite = increment(currentWrite);
        
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            // Buffer is full
            return false;
        }
        
        buffer_[currentWrite] = item;
        writeIndex_.store(nextWrite, std::memory_order_release);
        return true;
    }

    /**
     * @brief Push a single element to the buffer (producer side) - move version
     * 
     * @param item The item to push
     * @return true if the item was successfully pushed, false if buffer is full
     */
    bool push(T&& item)
    {
        const size_t currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const size_t nextWrite = increment(currentWrite);
        
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            // Buffer is full
            return false;
        }
        
        buffer_[currentWrite] = std::move(item);
        writeIndex_.store(nextWrite, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop a single element from the buffer (consumer side)
     * 
     * @param item Reference to store the popped item
     * @return true if an item was successfully popped, false if buffer is empty
     */
    bool pop(T& item)
    {
        const size_t currentRead = readIndex_.load(std::memory_order_relaxed);
        
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            // Buffer is empty
            return false;
        }
        
        item = buffer_[currentRead];
        readIndex_.store(increment(currentRead), std::memory_order_release);
        return true;
    }

    /**
     * @brief Peek at the next element without removing it (consumer side)
     * 
     * @param item Reference to store the peeked item
     * @return true if an item was available to peek, false if buffer is empty
     */
    bool peek(T& item) const
    {
        const size_t currentRead = readIndex_.load(std::memory_order_relaxed);
        
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            // Buffer is empty
            return false;
        }
        
        item = buffer_[currentRead];
        return true;
    }

    /**
     * @brief Check if the buffer is empty
     * 
     * @return true if empty, false otherwise
     */
    bool empty() const
    {
        return readIndex_.load(std::memory_order_acquire) == 
               writeIndex_.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if the buffer is full
     * 
     * @return true if full, false otherwise
     */
    bool full() const
    {
        const size_t currentWrite = writeIndex_.load(std::memory_order_acquire);
        const size_t nextWrite = increment(currentWrite);
        return nextWrite == readIndex_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the current number of elements in the buffer
     * 
     * Note: This is an approximation due to concurrent access and should be used
     * for informational purposes only, not for algorithmic decisions.
     * 
     * @return Approximate number of elements
     */
    size_t size() const
    {
        const size_t write = writeIndex_.load(std::memory_order_acquire);
        const size_t read = readIndex_.load(std::memory_order_acquire);
        return (write - read) & (Size - 1);
    }

    /**
     * @brief Get the capacity of the buffer
     * 
     * @return The capacity (Size - 1, since one slot is reserved)
     */
    constexpr size_t capacity() const
    {
        return Size - 1; // One slot is reserved to distinguish full from empty
    }

    /**
     * @brief Clear the buffer (not thread-safe, use only when no other threads are accessing)
     */
    void clear()
    {
        readIndex_.store(0, std::memory_order_relaxed);
        writeIndex_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Bulk push operation - push multiple elements at once
     * 
     * @param items Pointer to array of items to push
     * @param count Number of items to push
     * @return Number of items actually pushed
     */
    size_t push_bulk(const T* items, size_t count)
    {
        if (!items || count == 0) return 0;
        
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!push(items[i])) {
                break;
            }
            ++pushed;
        }
        return pushed;
    }

    /**
     * @brief Bulk pop operation - pop multiple elements at once
     * 
     * @param items Pointer to array to store popped items
     * @param count Maximum number of items to pop
     * @return Number of items actually popped
     */
    size_t pop_bulk(T* items, size_t count)
    {
        if (!items || count == 0) return 0;
        
        size_t popped = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!pop(items[i])) {
                break;
            }
            ++popped;
        }
        return popped;
    }

private:
    /**
     * @brief Increment index with wraparound
     * 
     * @param index Current index
     * @return Next index
     */
    constexpr size_t increment(size_t index) const
    {
        return (index + 1) & (Size - 1);
    }

    // Buffer storage
    std::array<T, Size> buffer_;
    
    // Atomic indices for lock-free operation
    // Aligned to cache line boundaries to avoid false sharing
    alignas(64) std::atomic<size_t> readIndex_;
    alignas(64) std::atomic<size_t> writeIndex_;
};

} // namespace KhDetector 