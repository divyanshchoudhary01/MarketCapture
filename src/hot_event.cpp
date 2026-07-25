#include "marketcapture/hot_event.hpp"
#include "marketcapture/itch.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace marketcapture {
namespace {
std::uint64_t be(std::span<const std::uint8_t> bytes, std::size_t offset,
                 std::size_t width) {
    if (offset + width > bytes.size()) throw ParseError("truncated ITCH field");
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < width; ++index)
        value = (value << 8) | bytes[offset + index];
    return value;
}
void exact(std::span<const std::uint8_t> bytes, std::size_t size) {
    if (bytes.size() != size) throw ParseError("invalid ITCH message length");
}
template <std::size_t N>
void text(std::array<char, N>& output, std::span<const std::uint8_t> input,
          std::size_t offset) {
    for (std::size_t index = 0; index < N; ++index)
        output[index] = static_cast<char>(input[offset + index]);
}
template <std::size_t N>
std::string trimmed(const std::array<char, N>& input) {
    auto end = input.end();
    while (end != input.begin() && *(end - 1) == ' ') --end;
    return {input.begin(), end};
}
std::uint64_t timestamp(std::span<const std::uint8_t> message) {
    return be(message, 5, 6);
}
}

HotEvent HotItchParser::parse(std::span<const std::uint8_t> m) const {
    if (m.empty()) throw ParseError("empty ITCH message");
    HotEvent event;
    switch (m[0]) {
    case 'A':
    case 'F':
        exact(m, m[0] == 'A' ? 36 : 40);
        event.type = m[0] == 'A' ? HotEventType::add : HotEventType::add_mpid;
        event.timestamp = timestamp(m);
        event.order_id = be(m, 11, 8);
        if (m[19] != 'B' && m[19] != 'S') throw ParseError("invalid order side");
        event.side = static_cast<Side>(m[19]);
        event.shares = static_cast<std::uint32_t>(be(m, 20, 4));
        text(event.symbol, m, 24);
        event.price = static_cast<std::uint32_t>(be(m, 32, 4));
        if (m[0] == 'F') text(event.attribution, m, 36);
        return event;
    case 'E':
        exact(m, 31); event.type = HotEventType::execute;
        event.timestamp = timestamp(m); event.order_id = be(m, 11, 8);
        event.shares = static_cast<std::uint32_t>(be(m, 19, 4));
        event.secondary_id = be(m, 23, 8); return event;
    case 'C':
        exact(m, 36); event.type = HotEventType::execute_price;
        event.timestamp = timestamp(m); event.order_id = be(m, 11, 8);
        event.shares = static_cast<std::uint32_t>(be(m, 19, 4));
        event.secondary_id = be(m, 23, 8);
        if (m[31] != 'Y' && m[31] != 'N') throw ParseError("invalid printable field");
        event.printable = m[31] == 'Y';
        event.price = static_cast<std::uint32_t>(be(m, 32, 4)); return event;
    case 'X':
        exact(m, 23); event.type = HotEventType::cancel;
        event.timestamp = timestamp(m); event.order_id = be(m, 11, 8);
        event.shares = static_cast<std::uint32_t>(be(m, 19, 4)); return event;
    case 'D':
        exact(m, 19); event.type = HotEventType::delete_order;
        event.timestamp = timestamp(m); event.order_id = be(m, 11, 8); return event;
    case 'U':
        exact(m, 35); event.type = HotEventType::replace;
        event.timestamp = timestamp(m); event.order_id = be(m, 11, 8);
        event.secondary_id = be(m, 19, 8);
        event.shares = static_cast<std::uint32_t>(be(m, 27, 4));
        event.price = static_cast<std::uint32_t>(be(m, 31, 4)); return event;
    default:
        // Validate every non-book ITCH family without materializing strings.
        switch (m[0]) {
        case 'S': exact(m, 12); break;
        case 'R': exact(m, 39); break;
        case 'H': exact(m, 25); break;
        case 'Y': exact(m, 20); break;
        case 'L': exact(m, 26); break;
        case 'V': exact(m, 35); break;
        case 'W': exact(m, 12); break;
        case 'K': exact(m, 28); break;
        case 'J': exact(m, 35); break;
        case 'h': exact(m, 21); break;
        case 'P': exact(m, 44); break;
        case 'Q': exact(m, 40); break;
        case 'B': exact(m, 19); break;
        case 'I': exact(m, 50); break;
        case 'N': exact(m, 20); break;
        default: throw ParseError("unsupported ITCH message type");
        }
        event.type = HotEventType::informational;
        if (m.size() >= 11) event.timestamp = timestamp(m);
        return event;
    }
}

Event materialize_event(const HotEvent& e) {
    switch (e.type) {
    case HotEventType::add:
        return AddOrder{e.timestamp, e.order_id, e.side, e.shares,
                        trimmed(e.symbol), e.price};
    case HotEventType::add_mpid:
        return AddOrderMpid{{e.timestamp, e.order_id, e.side, e.shares,
                             trimmed(e.symbol), e.price}, trimmed(e.attribution)};
    case HotEventType::execute:
        return ExecuteOrder{e.timestamp, e.order_id, e.shares, e.secondary_id};
    case HotEventType::execute_price:
        return ExecuteOrderPrice{{e.timestamp, e.order_id, e.shares, e.secondary_id},
                                 e.printable, e.price};
    case HotEventType::cancel:
        return CancelOrder{e.timestamp, e.order_id, e.shares};
    case HotEventType::delete_order:
        return DeleteOrder{e.timestamp, e.order_id};
    case HotEventType::replace:
        return ReplaceOrder{e.timestamp, e.order_id, e.secondary_id, e.shares, e.price};
    default:
        return SystemEvent{e.timestamp, 0};
    }
}

} // namespace marketcapture
