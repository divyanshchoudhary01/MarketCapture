#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace marketcapture {

[[nodiscard]] std::optional<std::span<const std::uint8_t>>
extract_ethernet_ipv4_udp_payload(std::span<const std::uint8_t> frame) noexcept;

class DpdkPacketSource {
public:
    using Handler = std::function<void(std::span<const std::uint8_t>)>;
    DpdkPacketSource(std::uint16_t port_id, std::uint16_t queue_id,
                     std::uint16_t burst_size = 32);
    ~DpdkPacketSource();
    DpdkPacketSource(const DpdkPacketSource&) = delete;
    DpdkPacketSource& operator=(const DpdkPacketSource&) = delete;

    [[nodiscard]] std::size_t poll(const Handler& handler);
    [[nodiscard]] static constexpr bool compiled() noexcept {
#ifdef MARKETCAPTURE_HAS_DPDK
        return true;
#else
        return false;
#endif
    }
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketcapture
