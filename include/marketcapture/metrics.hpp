#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace marketcapture {

class Metrics {
public:
    void packet_received() noexcept { packets_.fetch_add(1, std::memory_order_relaxed); }
    void message_processed() noexcept { messages_.fetch_add(1, std::memory_order_relaxed); }
    void parse_error() noexcept { errors_.fetch_add(1, std::memory_order_relaxed); }
    void gap(std::uint64_t count = 1) noexcept { gaps_.fetch_add(count, std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t packets() const noexcept { return packets_.load(); }
    [[nodiscard]] std::uint64_t messages() const noexcept { return messages_.load(); }
    [[nodiscard]] std::uint64_t errors() const noexcept { return errors_.load(); }
    [[nodiscard]] std::uint64_t gaps() const noexcept { return gaps_.load(); }
private:
    std::atomic<std::uint64_t> packets_{0}, messages_{0}, errors_{0}, gaps_{0};
};

class LatencyTimer {
public:
    LatencyTimer() : start_(std::chrono::steady_clock::now()) {}
    [[nodiscard]] std::uint64_t nanoseconds() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start_).count();
    }
private:
    std::chrono::steady_clock::time_point start_;
};

} // namespace marketcapture
