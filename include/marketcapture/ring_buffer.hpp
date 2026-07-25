#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace marketcapture {

// Bounded, wait-free single-producer/single-consumer queue.
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2);
public:
    [[nodiscard]] bool try_push(T value) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) return false;
        storage_[head] = std::move(value);
        head_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return std::nullopt;
        auto value = std::move(storage_[tail]);
        tail_.store(increment(tail), std::memory_order_release);
        return value;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity - 1; }

private:
    static constexpr std::size_t increment(std::size_t value) noexcept {
        return (value + 1) % Capacity;
    }
    std::array<T, Capacity> storage_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

} // namespace marketcapture
