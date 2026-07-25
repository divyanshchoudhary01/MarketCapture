#include "marketcapture/fpga_ring.hpp"
#include <atomic>
#include <cstring>
#include <stdexcept>

namespace marketcapture {
namespace {
std::uint32_t checksum(std::span<const std::uint8_t> payload) {
    std::uint32_t hash = 2166136261u;
    for (const auto byte : payload) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}
}

std::size_t FpgaRingConsumer::poll(const Handler& handler,
                                   std::size_t maximum_records) {
    std::size_t consumed = 0;
    while (consumed < maximum_records && !ring_.empty()) {
        auto& record = ring_[cursor_];
        std::atomic_ref<std::uint16_t> flags(record.flags);
        if ((flags.load(std::memory_order_acquire) & fpga_record_valid) == 0) break;
        if (record.length == 0 || record.length > record.payload.size())
            throw std::runtime_error("invalid FPGA DMA record length");
        const auto payload = std::span<const std::uint8_t>(
            record.payload.data(), record.length);
        if (checksum(payload) != record.checksum)
            throw std::runtime_error("invalid FPGA DMA record checksum");
        handler(record.sequence, record.hardware_timestamp_ns, payload);
        flags.store(0, std::memory_order_release);
        cursor_ = (cursor_ + 1) % ring_.size();
        ++consumed;
    }
    return consumed;
}

FpgaRingSimulator::FpgaRingSimulator(std::size_t slots) : records_(slots) {
    if (slots == 0) throw std::invalid_argument("FPGA ring needs at least one slot");
}

void FpgaRingSimulator::publish(std::uint64_t sequence, std::uint64_t timestamp_ns,
                                std::span<const std::uint8_t> payload) {
    if (payload.empty() || payload.size() > fpga_payload_capacity)
        throw std::invalid_argument("invalid FPGA simulated payload");
    auto& record = records_[producer_];
    std::atomic_ref<std::uint16_t> flags(record.flags);
    if (flags.load(std::memory_order_acquire) & fpga_record_valid)
        throw std::runtime_error("FPGA simulated ring is full");
    record.sequence = sequence;
    record.hardware_timestamp_ns = timestamp_ns;
    record.length = static_cast<std::uint16_t>(payload.size());
    std::memcpy(record.payload.data(), payload.data(), payload.size());
    record.checksum = checksum(payload);
    flags.store(fpga_record_valid, std::memory_order_release);
    producer_ = (producer_ + 1) % records_.size();
}

} // namespace marketcapture
