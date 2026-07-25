#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace marketcapture {

struct LatencyPercentiles {
    std::uint64_t minimum_ns{};
    std::uint64_t p50_ns{};
    std::uint64_t p99_ns{};
    std::uint64_t p999_ns{};
    std::uint64_t maximum_ns{};
    double mean_ns{};
};

[[nodiscard]] inline LatencyPercentiles summarize_latency(
    std::span<const std::uint64_t> samples) {
    if (samples.empty()) throw std::invalid_argument("latency samples are empty");
    std::vector<std::uint64_t> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&](std::uint64_t numerator, std::uint64_t denominator) {
        const auto index = ((sorted.size() - 1) * numerator + denominator - 1) / denominator;
        return sorted[index];
    };
    const long double total = std::accumulate(
        sorted.begin(), sorted.end(), static_cast<long double>(0));
    return {sorted.front(), percentile(50, 100), percentile(99, 100),
        percentile(999, 1000), sorted.back(),
        static_cast<double>(total / sorted.size())};
}

} // namespace marketcapture
