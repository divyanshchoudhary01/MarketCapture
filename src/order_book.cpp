#include "marketcapture/order_book.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace marketcapture {
void OrderBook::apply(const Event& event) {
    std::visit([this](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AddOrder>) {
            if (value.shares == 0 || value.symbol.empty()) throw std::invalid_argument("invalid add order");
            if (!symbol_.empty() && symbol_ != value.symbol) throw std::invalid_argument("symbol mismatch");
            if (orders_.contains(value.order_id)) throw std::invalid_argument("duplicate order id");
            symbol_ = value.symbol;
            auto& levels = value.side == Side::buy ? bids_ : asks_;
            auto& level = levels[value.price];
            level.price = value.price;
            level.shares += value.shares;
            ++level.orders;
            orders_.emplace(value.order_id, Order{value.side, value.price, value.shares});
        } else if constexpr (std::is_same_v<T, AddOrderMpid>) {
            apply(value.order);
        } else if constexpr (std::is_same_v<T, ExecuteOrder> || std::is_same_v<T, CancelOrder>) {
            reduce(value.order_id, value.shares, false);
        } else if constexpr (std::is_same_v<T, ExecuteOrderPrice>) {
            reduce(value.execution.order_id, value.execution.shares, false);
        } else if constexpr (std::is_same_v<T, DeleteOrder>) {
            reduce(value.order_id, 0, true);
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
            auto it = orders_.find(value.original_order_id);
            if (it == orders_.end()) throw std::invalid_argument("unknown order id");
            if (value.shares == 0 || orders_.contains(value.new_order_id))
                throw std::invalid_argument("invalid replacement order");
            const auto side = it->second.side;
            reduce(value.original_order_id, 0, true);
            apply(AddOrder{value.timestamp, value.new_order_id, side, value.shares, symbol_, value.price});
        }
    }, event);
}

void OrderBook::reduce(std::uint64_t id, std::uint32_t shares, bool erase_all) {
    auto order_it = orders_.find(id);
    if (order_it == orders_.end()) throw std::invalid_argument("unknown order id");
    auto& order = order_it->second;
    if (!erase_all && (shares == 0 || shares > order.shares)) throw std::invalid_argument("invalid reduction");
    const auto amount = erase_all ? order.shares : shares;
    auto& levels = order.side == Side::buy ? bids_ : asks_;
    auto level_it = levels.find(order.price);
    level_it->second.shares -= amount;
    order.shares -= amount;
    if (order.shares == 0) {
        --level_it->second.orders;
        orders_.erase(order_it);
    }
    if (level_it->second.orders == 0) levels.erase(level_it);
}

std::optional<Level> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.rbegin()->second;
}
std::optional<Level> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->second;
}
std::vector<Level> OrderBook::bids(std::size_t depth) const {
    std::vector<Level> result;
    result.reserve(std::min(depth, bids_.size()));
    for (auto it = bids_.rbegin(); it != bids_.rend() && result.size() < depth; ++it) result.push_back(it->second);
    return result;
}
std::vector<Level> OrderBook::asks(std::size_t depth) const {
    std::vector<Level> result;
    result.reserve(std::min(depth, asks_.size()));
    for (auto it = asks_.begin(); it != asks_.end() && result.size() < depth; ++it) result.push_back(it->second);
    return result;
}
std::vector<ActiveOrder> OrderBook::active_orders() const {
    std::vector<ActiveOrder> result;
    result.reserve(orders_.size());
    for (const auto& [id, order] : orders_)
        result.push_back({id, order.side, order.price, order.shares});
    std::sort(result.begin(), result.end(),
        [](const ActiveOrder& left, const ActiveOrder& right) {
            return left.order_id < right.order_id;
        });
    return result;
}
void OrderBook::clear() {
    symbol_.clear(); orders_.clear(); bids_.clear(); asks_.clear();
}
} // namespace marketcapture
