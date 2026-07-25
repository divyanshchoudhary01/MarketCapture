#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace marketcapture {

constexpr std::uint16_t fpga_record_valid = 0x0001;
constexpr std::size_t fpga_payload_capacity = 96;

struct alignas(64) FpgaDmaRecord {
    std::uint64_t sequence{};
    std::uint64_t hardware_timestamp_ns{};
    std::uint16_t length{};
    std::uint16_t flags{};
    std::uint32_t checksum{};
    std::array<std::uint8_t, fpga_payload_capacity> payload{};
};
static_assert(sizeof(FpgaDmaRecord) == 128);

class FpgaRingConsumer {
public:
    using Handler = std::function<void(std::uint64_t sequence,
        std::uint64_t hardware_timestamp_ns, std::span<const std::uint8_t> payload)>;
    explicit FpgaRingConsumer(std::span<FpgaDmaRecord> ring) : ring_(ring) {}
    [[nodiscard]] std::size_t poll(const Handler& handler,
                                   std::size_t maximum_records);
private:
    std::span<FpgaDmaRecord> ring_;
    std::size_t cursor_{};
};

class FpgaRingSimulator {
public:
    explicit FpgaRingSimulator(std::size_t slots);
    void publish(std::uint64_t sequence, std::uint64_t timestamp_ns,
                 std::span<const std::uint8_t> payload);
    [[nodiscard]] FpgaRingConsumer consumer() { return FpgaRingConsumer(records_); }
private:
    std::vector<FpgaDmaRecord> records_;
    std::size_t producer_{};
};

} // namespace marketcapture
