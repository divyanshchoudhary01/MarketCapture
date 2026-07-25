#include "marketcapture/itch.hpp"
#include <algorithm>
#include <string>

namespace marketcapture {
namespace {
void require(std::span<const std::uint8_t> data, std::size_t size) {
    if (data.size() != size) throw ParseError("invalid ITCH message length");
}
std::uint32_t u32(std::span<const std::uint8_t> d, std::size_t p) {
    return (std::uint32_t(d[p]) << 24) | (std::uint32_t(d[p+1]) << 16) |
           (std::uint32_t(d[p+2]) << 8) | d[p+3];
}
std::uint64_t u48(std::span<const std::uint8_t> d, std::size_t p) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 6; ++i) value = (value << 8) | d[p+i];
    return value;
}
std::uint64_t u64(std::span<const std::uint8_t> d, std::size_t p) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8) | d[p+i];
    return value;
}
std::string stock(std::span<const std::uint8_t> d, std::size_t p) {
    std::string result(reinterpret_cast<const char*>(d.data() + p), 8);
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}
std::string text(std::span<const std::uint8_t> d, std::size_t p, std::size_t size) {
    std::string result(reinterpret_cast<const char*>(d.data() + p), size);
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}
bool yes(std::uint8_t value) {
    if (value != 'Y' && value != 'N') throw ParseError("invalid ITCH yes/no field");
    return value == 'Y';
}
Side side(std::uint8_t value) {
    if (value != 'B' && value != 'S') throw ParseError("invalid order side");
    return static_cast<Side>(value);
}
}

Event ItchParser::parse(std::span<const std::uint8_t> m) const {
    if (m.empty()) throw ParseError("empty ITCH message");
    switch (m[0]) {
    case 'S':
        require(m, 12);
        return SystemEvent{u48(m, 5), static_cast<char>(m[11])};
    case 'R':
        require(m, 39);
        return StockDirectory{u48(m, 5), stock(m, 11), static_cast<char>(m[19]),
            static_cast<char>(m[20]), u32(m, 21), yes(m[25]), static_cast<char>(m[26]),
            text(m, 27, 2), static_cast<char>(m[29]), static_cast<char>(m[30]),
            static_cast<char>(m[31]), static_cast<char>(m[32]), static_cast<char>(m[33]),
            u32(m, 34), yes(m[38])};
    case 'H':
        require(m, 25);
        return TradingAction{u48(m, 5), stock(m, 11), static_cast<char>(m[19]), text(m, 21, 4)};
    case 'Y':
        require(m, 20);
        return RegShoRestriction{u48(m, 5), stock(m, 11), static_cast<char>(m[19])};
    case 'L':
        require(m, 26);
        return MarketParticipantPosition{u48(m, 5), text(m, 11, 4), stock(m, 15),
            yes(m[23]), static_cast<char>(m[24]), static_cast<char>(m[25])};
    case 'V':
        require(m, 35);
        return MwcbDeclineLevels{u48(m, 5), u64(m, 11), u64(m, 19), u64(m, 27)};
    case 'W':
        require(m, 12);
        return MwcbStatus{u48(m, 5), static_cast<char>(m[11])};
    case 'K':
        require(m, 28);
        return IpoQuotingPeriod{u48(m, 5), stock(m, 11), u32(m, 19),
            static_cast<char>(m[23]), u32(m, 24)};
    case 'J':
        require(m, 35);
        return LuldAuctionCollar{u48(m, 5), stock(m, 11), u32(m, 19),
            u32(m, 27), u32(m, 23), u32(m, 31)};
    case 'h':
        require(m, 21);
        return OperationalHalt{u48(m, 5), stock(m, 11),
            static_cast<char>(m[19]), static_cast<char>(m[20])};
    case 'A':
        require(m, 36);
        return AddOrder{u48(m, 5), u64(m, 11), side(m[19]),
                        u32(m, 20), stock(m, 24), u32(m, 32)};
    case 'F':
        require(m, 40);
        return AddOrderMpid{AddOrder{u48(m, 5), u64(m, 11), side(m[19]),
            u32(m, 20), stock(m, 24), u32(m, 32)}, text(m, 36, 4)};
    case 'E':
        require(m, 31);
        return ExecuteOrder{u48(m, 5), u64(m, 11), u32(m, 19), u64(m, 23)};
    case 'C':
        require(m, 36);
        if (m[31] != 'Y' && m[31] != 'N') throw ParseError("invalid printable field");
        return ExecuteOrderPrice{ExecuteOrder{u48(m, 5), u64(m, 11),
            u32(m, 19), u64(m, 23)}, m[31] == 'Y', u32(m, 32)};
    case 'X':
        require(m, 23);
        return CancelOrder{u48(m, 5), u64(m, 11), u32(m, 19)};
    case 'D':
        require(m, 19);
        return DeleteOrder{u48(m, 5), u64(m, 11)};
    case 'U':
        require(m, 35);
        return ReplaceOrder{u48(m, 5), u64(m, 11), u64(m, 19), u32(m, 27), u32(m, 31)};
    case 'P':
        require(m, 44);
        return Trade{u48(m, 5), u64(m, 11), side(m[19]), u32(m, 20),
            stock(m, 24), u32(m, 32), u64(m, 36)};
    case 'Q':
        require(m, 40);
        return CrossTrade{u48(m, 5), u64(m, 11), stock(m, 19), u32(m, 27),
            u64(m, 31), static_cast<char>(m[39])};
    case 'B':
        require(m, 19);
        return BrokenTrade{u48(m, 5), u64(m, 11)};
    case 'I':
        require(m, 50);
        return Noii{u48(m, 5), u64(m, 11), u64(m, 19), static_cast<char>(m[27]),
            stock(m, 28), u32(m, 36), u32(m, 40), u32(m, 44),
            static_cast<char>(m[48]), static_cast<char>(m[49])};
    case 'N':
        require(m, 20);
        return RetailPriceImprovement{u48(m, 5), stock(m, 11), static_cast<char>(m[19])};
    default:
        throw ParseError("unsupported ITCH message type");
    }
}
} // namespace marketcapture
