#pragma once

#include "marketcapture/hot_event.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace marketcapture {

struct ArenaLevel {
    std::uint32_t price{};
    std::uint64_t shares{};
    std::uint32_t orders{};
};

class ArenaBookRouter {
public:
    ArenaBookRouter(std::size_t arena_bytes, std::size_t max_orders,
                    std::size_t max_levels, std::size_t max_symbols);
    ~ArenaBookRouter();
    ArenaBookRouter(const ArenaBookRouter&) = delete;
    ArenaBookRouter& operator=(const ArenaBookRouter&) = delete;

    void apply(const HotEvent& event);
    [[nodiscard]] std::size_t order_count() const noexcept;
    [[nodiscard]] std::size_t symbol_count() const noexcept;
    [[nodiscard]] std::optional<ArenaLevel> best_bid(
        const std::array<char, 8>& symbol) const noexcept;
    [[nodiscard]] std::optional<ArenaLevel> best_ask(
        const std::array<char, 8>& symbol) const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketcapture
