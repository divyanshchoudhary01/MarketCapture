#pragma once

#include "marketcapture/types.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace marketcapture {

enum class HotEventType : std::uint8_t {
    informational, add, add_mpid, execute, execute_price, cancel,
    delete_order, replace
};

struct HotEvent {
    HotEventType type{HotEventType::informational};
    std::uint64_t timestamp{};
    std::uint64_t order_id{};
    std::uint64_t secondary_id{};
    std::uint32_t shares{};
    std::uint32_t price{};
    Side side{Side::buy};
    bool printable{};
    std::array<char, 8> symbol{};
    std::array<char, 4> attribution{};

    [[nodiscard]] bool order_tick() const noexcept {
        return type != HotEventType::informational;
    }
};

class HotItchParser {
public:
    [[nodiscard]] HotEvent parse(std::span<const std::uint8_t> message) const;
};

[[nodiscard]] Event materialize_event(const HotEvent& event);

} // namespace marketcapture
