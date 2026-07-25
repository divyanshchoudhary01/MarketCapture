#pragma once

#include "marketcapture/types.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace marketcapture {

struct Level {
    std::uint32_t price{};
    std::uint64_t shares{};
    std::size_t orders{};
};

struct ActiveOrder {
    std::uint64_t order_id{};
    Side side{};
    std::uint32_t price{};
    std::uint32_t shares{};
};

class OrderBook {
public:
    void apply(const Event& event);
    [[nodiscard]] std::optional<Level> best_bid() const;
    [[nodiscard]] std::optional<Level> best_ask() const;
    [[nodiscard]] std::vector<Level> bids(std::size_t depth) const;
    [[nodiscard]] std::vector<Level> asks(std::size_t depth) const;
    [[nodiscard]] std::size_t order_count() const noexcept { return orders_.size(); }
    [[nodiscard]] bool contains_order(std::uint64_t order_id) const noexcept {
        return orders_.contains(order_id);
    }
    [[nodiscard]] std::vector<ActiveOrder> active_orders() const;
    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }
    void clear();

private:
    struct Order { Side side; std::uint32_t price; std::uint32_t shares; };
    using Levels = std::map<std::uint32_t, Level>;
    void reduce(std::uint64_t id, std::uint32_t shares, bool erase_all);
    std::string symbol_;
    std::unordered_map<std::uint64_t, Order> orders_;
    Levels bids_;
    Levels asks_;
};

} // namespace marketcapture
