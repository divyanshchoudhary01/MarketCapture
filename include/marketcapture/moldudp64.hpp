#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace marketcapture {

struct MoldPacket {
    std::array<char, 10> session{};
    std::uint64_t sequence{};
    std::vector<std::span<const std::uint8_t>> messages;
};

class MoldUdp64Decoder {
public:
    [[nodiscard]] MoldPacket decode(std::span<const std::uint8_t> packet) const;
};

} // namespace marketcapture
