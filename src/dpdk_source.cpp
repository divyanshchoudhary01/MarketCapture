#include "marketcapture/dpdk_source.hpp"
#include <stdexcept>
#include <vector>
#ifdef MARKETCAPTURE_HAS_DPDK
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#endif

namespace marketcapture {

std::optional<std::span<const std::uint8_t>>
extract_ethernet_ipv4_udp_payload(std::span<const std::uint8_t> frame) noexcept {
    constexpr std::size_t ethernet = 14;
    if (frame.size() < ethernet + 20 + 8) return std::nullopt;
    const auto ether_type = (std::uint16_t(frame[12]) << 8) | frame[13];
    if (ether_type != 0x0800) return std::nullopt;
    const auto ihl = std::size_t(frame[ethernet] & 0x0f) * 4;
    if (ihl < 20 || frame.size() < ethernet + ihl + 8 ||
        frame[ethernet + 9] != 17) return std::nullopt;
    const auto udp = ethernet + ihl;
    const auto udp_length = (std::uint16_t(frame[udp + 4]) << 8) | frame[udp + 5];
    if (udp_length < 8 || udp + udp_length > frame.size()) return std::nullopt;
    return frame.subspan(udp + 8, udp_length - 8);
}

class DpdkPacketSource::Impl {
public:
    Impl(std::uint16_t port, std::uint16_t queue, std::uint16_t burst)
        : port_id(port), queue_id(queue), burst_size(burst) {
        if (burst == 0) throw std::invalid_argument("DPDK burst size must be positive");
#ifndef MARKETCAPTURE_HAS_DPDK
        throw std::runtime_error("MarketCapture was built without DPDK");
#endif
    }
    std::uint16_t port_id;
    std::uint16_t queue_id;
    std::uint16_t burst_size;
};

DpdkPacketSource::DpdkPacketSource(std::uint16_t port_id, std::uint16_t queue_id,
                                   std::uint16_t burst_size)
    : impl_(std::make_unique<Impl>(port_id, queue_id, burst_size)) {}
DpdkPacketSource::~DpdkPacketSource() = default;

std::size_t DpdkPacketSource::poll(const Handler& handler) {
#ifdef MARKETCAPTURE_HAS_DPDK
    std::vector<rte_mbuf*> packets(impl_->burst_size);
    const auto received = rte_eth_rx_burst(
        impl_->port_id, impl_->queue_id, packets.data(), impl_->burst_size);
    std::size_t delivered = 0;
    for (std::uint16_t index = 0; index < received; ++index) {
        auto* packet = packets[index];
        const auto* bytes = rte_pktmbuf_mtod(packet, const std::uint8_t*);
        const auto length = rte_pktmbuf_pkt_len(packet);
        if (rte_pktmbuf_is_contiguous(packet)) {
            if (const auto payload = extract_ethernet_ipv4_udp_payload(
                    std::span<const std::uint8_t>(bytes, length))) {
                handler(*payload);
                ++delivered;
            }
        }
        rte_pktmbuf_free(packet);
    }
    return delivered;
#else
    (void)handler;
    throw std::runtime_error("MarketCapture was built without DPDK");
#endif
}

} // namespace marketcapture
