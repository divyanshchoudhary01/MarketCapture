#pragma once

#include "marketcapture/feed_arbitrator.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace marketcapture {

struct ThreadedEngineConfig {
    std::size_t book_shards{8};
    std::filesystem::path record_path;
    std::filesystem::path pcap_path;
    std::optional<std::uint64_t> initial_sequence;
    std::size_t max_reorder_messages{65536};
    std::size_t book_arena_bytes{128 * 1024 * 1024};
    std::size_t max_orders{1'000'000};
    std::size_t max_levels{500'000};
    std::size_t max_symbols{16'384};
};

struct ThreadedEngineStats {
    std::uint64_t packets_submitted{};
    std::uint64_t packets_rejected{};
    std::uint64_t messages_parsed{};
    std::uint64_t book_updates{};
    std::uint64_t recorded_events{};
    std::uint64_t metrics_events{};
    std::uint64_t pcap_packets{};
    std::uint64_t parse_errors{};
    std::uint64_t duplicate_messages{};
    std::uint64_t recovery_requests{};
    std::size_t active_symbols{};
    std::size_t active_orders{};
};

// One submitter (NIC/RX thread) feeds a preallocated packet pool. Arbitration
// and parsing run on one worker and fan out through independent SPSC queues.
class ThreadedCaptureEngine {
public:
    using RecoveryHandler = std::function<void(const RecoveryRequest&)>;

    explicit ThreadedCaptureEngine(ThreadedEngineConfig config,
                                   RecoveryHandler recovery_handler = {});
    ~ThreadedCaptureEngine();
    ThreadedCaptureEngine(const ThreadedCaptureEngine&) = delete;
    ThreadedCaptureEngine& operator=(const ThreadedCaptureEngine&) = delete;

    // Non-blocking: false means the fixed ingress pool is full or the engine
    // is stopping. Exactly one producer may call submit().
    [[nodiscard]] bool submit(FeedChannel channel,
                              std::span<const std::uint8_t> datagram) noexcept;
    [[nodiscard]] bool wait_until_idle(std::chrono::milliseconds timeout);
    void stop();
    [[nodiscard]] ThreadedEngineStats stats() const noexcept;
    void rethrow_worker_error() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketcapture
