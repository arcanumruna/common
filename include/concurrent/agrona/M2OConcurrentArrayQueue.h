#pragma once

#include <atomic>
#include <vector>
#include <functional>

/**
 * Cache line size used for padding.
 */
constexpr std::size_t CACHE_LINE_SIZE = 64;

/**
 * Round up to the next power of two.
 */
constexpr std::size_t next_power_of_two(std::size_t x)
{
    if (x == 0) return 1;
    --x;
    for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1)
        x |= x >> i;
    return x + 1;
}

/**
 * Compute buffer offset.
 */
template<typename T>
inline std::size_t sequence_to_buffer_offset(std::size_t sequence, std::size_t mask)
{
    return sequence & mask;
}

template<typename E>
class M2OConcurrentArrayQueue
{
public:
    explicit M2OConcurrentArrayQueue(const std::size_t requested_capacity)
        : tail_(0),
          shared_head_cache_(0),
          head_(0),
          capacity_(next_power_of_two(requested_capacity)),
          buffer_(capacity_)
    {
    }

    bool offer(const E& element)
    {
        std::size_t current_head = shared_head_cache_.load(std::memory_order_relaxed);
        std::size_t buffer_limit = current_head + capacity_;
        std::size_t current_tail;

        do
        {
            current_tail = tail_.load(std::memory_order_relaxed);
            if (current_tail >= buffer_limit)
            {
                current_head = head_.load(std::memory_order_acquire);
                buffer_limit = current_head + capacity_;
                if (current_tail >= buffer_limit)
                {
                    return false;
                }
                shared_head_cache_.store(current_head, std::memory_order_release);
            }
        } while (!tail_.compare_exchange_weak(current_tail, current_tail + 1, std::memory_order_acq_rel));

        buffer_[sequence_to_buffer_offset<E>(current_tail, capacity_ - 1)].store(new E(element), std::memory_order_release);
        return true;
    }

    E* poll()
    {
        std::size_t current_head = head_.load(std::memory_order_relaxed);
        std::size_t offset = sequence_to_buffer_offset<E>(current_head, capacity_ - 1);
        std::atomic<E*>& slot = buffer_[offset];

        E* element = slot.load(std::memory_order_acquire);
        if (element != nullptr)
        {
            slot.store(nullptr, std::memory_order_relaxed);
            head_.store(current_head + 1, std::memory_order_release);
        }
        return element;
    }

    int drain(std::function<void(E*)> element_consumer, int limit)
    {
        std::size_t next_sequence = head_.load(std::memory_order_relaxed);
        std::size_t mask = capacity_ - 1;
        std::size_t limit_sequence = next_sequence + limit;
        int count = 0;

        while (next_sequence < limit_sequence)
        {
            std::size_t offset = sequence_to_buffer_offset<E>(next_sequence, mask);
            std::atomic<E*>& slot = buffer_[offset];
            E* element = slot.load(std::memory_order_acquire);
            if (element == nullptr)
            {
                break;
            }

            slot.store(nullptr, std::memory_order_release);
            head_.store(++next_sequence, std::memory_order_release);
            element_consumer(element);
            count++;
        }

        return count;
    }

    bool is_empty() const
    {
        return head_.load(std::memory_order_acquire) >= tail_.load(std::memory_order_acquire);
    }

    int size() const
    {
        std::size_t before, after = head_.load(std::memory_order_relaxed);
        std::size_t current_tail;

        do
        {
            before = after;
            current_tail = tail_.load(std::memory_order_acquire);
            after = head_.load(std::memory_order_acquire);
        } while (before != after);

        std::size_t size = current_tail - after;
        if (size > capacity_) size = capacity_;
        return static_cast<int>(size);
    }

    std::size_t capacity() const { return capacity_; }

private:
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> tail_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> shared_head_cache_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> head_;

    const std::size_t capacity_;
    std::vector<std::atomic<E*>> buffer_;
};
