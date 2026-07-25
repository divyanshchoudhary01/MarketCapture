#include "marketcapture/moldudp64.hpp"
#include "marketcapture/itch.hpp"
#include <algorithm>

namespace marketcapture {
namespace {
std::uint16_t u16(std::span<const std::uint8_t> d, std::size_t p) {
    return static_cast<std::uint16_t>((d[p] << 8) | d[p+1]);
}
std::uint64_t u64(std::span<const std::uint8_t> d, std::size_t p) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8) | d[p+i];
    return value;
}
}

MoldPacket MoldUdp64Decoder::decode(std::span<const std::uint8_t> packet) const {
    if (packet.size() < 20) throw ParseError("truncated MoldUDP64 header");
    MoldPacket result;
    std::copy_n(reinterpret_cast<const char*>(packet.data()), 10, result.session.begin());
    result.sequence = u64(packet, 10);
    const auto count = u16(packet, 18);
    result.messages.reserve(count);
    std::size_t offset = 20;
    for (std::uint16_t i = 0; i < count; ++i) {
        if (offset + 2 > packet.size()) throw ParseError("truncated MoldUDP64 message length");
        const auto length = u16(packet, offset);
        offset += 2;
        if (offset + length > packet.size()) throw ParseError("truncated MoldUDP64 message");
        result.messages.push_back(packet.subspan(offset, length));
        offset += length;
    }
    if (offset != packet.size()) throw ParseError("trailing bytes in MoldUDP64 packet");
    return result;
}
} // namespace marketcapture
