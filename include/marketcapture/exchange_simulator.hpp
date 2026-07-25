#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace marketcapture {

struct SimulatedPacket {
    std::uint64_t sequence{};
    std::vector<std::uint8_t> datagram;
};

class ExchangeSimulator {
public:
    explicit ExchangeSimulator(std::uint64_t seed = 1,
                               std::vector<std::string> symbols = {"AAPL", "MSFT", "NVDA"});
    [[nodiscard]] SimulatedPacket next();
    [[nodiscard]] std::vector<SimulatedPacket> generate(std::size_t count);

private:
    std::uint64_t random();
    std::vector<std::uint8_t> add_message(std::uint64_t id, const std::string& symbol);
    std::vector<std::uint8_t> delete_message(std::uint64_t id);
    std::vector<std::uint8_t> mold(std::uint64_t sequence,
                                   const std::vector<std::uint8_t>& message) const;
    std::uint64_t state_;
    std::uint64_t sequence_{1};
    std::uint64_t next_order_id_{1};
    std::vector<std::string> symbols_;
    std::vector<std::uint64_t> live_orders_;
};

} // namespace marketcapture
