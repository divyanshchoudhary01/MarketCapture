#pragma once

#include "marketcapture/itch.hpp"
#include "marketcapture/metrics.hpp"
#include "marketcapture/moldudp64.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace marketcapture {

// Converts complete MoldUDP64 datagrams into normalized events while tracking
// packet sequence continuity. One instance is intended for one feed session.
class CapturePipeline {
public:
    using EventHandler = std::function<void(std::uint64_t sequence, const Event&)>;

    explicit CapturePipeline(EventHandler handler);
    std::size_t process(std::span<const std::uint8_t> datagram);
    void reset() noexcept;

    [[nodiscard]] const Metrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] std::optional<std::uint64_t> next_sequence() const noexcept {
        return next_sequence_;
    }

private:
    MoldUdp64Decoder mold_;
    ItchParser itch_;
    EventHandler handler_;
    Metrics metrics_;
    std::optional<std::uint64_t> next_sequence_;
};

} // namespace marketcapture
