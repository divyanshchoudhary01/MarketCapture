#include "marketcapture/book_router.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace marketcapture {

ShardedBookRouter::ShardedBookRouter(std::size_t shard_count) : shards_(shard_count) {
    if (shard_count == 0) throw std::invalid_argument("shard count must be positive");
}

std::size_t ShardedBookRouter::shard_for(const std::string& symbol) const noexcept {
    return std::hash<std::string>{}(symbol) % shards_.size();
}

OrderBook& ShardedBookRouter::book_for(const std::string& symbol) {
    return shards_[shard_for(symbol)].books[symbol];
}

OrderBook& ShardedBookRouter::book_for_order(std::uint64_t order_id) {
    const auto route = order_routes_.find(order_id);
    if (route == order_routes_.end() || !route->second.book)
        throw std::invalid_argument("unknown routed order id");
    return *route->second.book;
}

void ShardedBookRouter::apply(const Event& event) {
    std::visit([this](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AddOrder>) {
            if (order_routes_.contains(value.order_id))
                throw std::invalid_argument("duplicate global order id");
            const auto shard = shard_for(value.symbol);
            auto& book = shards_[shard].books[value.symbol];
            book.apply(value);
            order_routes_.emplace(value.order_id, OrderRoute{shard, &book});
        } else if constexpr (std::is_same_v<T, AddOrderMpid>) {
            apply(value.order);
        } else if constexpr (std::is_same_v<T, ExecuteOrder> ||
                             std::is_same_v<T, CancelOrder> ||
                             std::is_same_v<T, DeleteOrder>) {
            auto& book = book_for_order(value.order_id);
            book.apply(value);
            if (!book.contains_order(value.order_id)) order_routes_.erase(value.order_id);
        } else if constexpr (std::is_same_v<T, ExecuteOrderPrice>) {
            const auto id = value.execution.order_id;
            auto& book = book_for_order(id);
            book.apply(value);
            if (!book.contains_order(id)) order_routes_.erase(id);
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
            if (order_routes_.contains(value.new_order_id))
                throw std::invalid_argument("duplicate replacement order id");
            auto& book = book_for_order(value.original_order_id);
            const auto route = order_routes_.at(value.original_order_id);
            book.apply(value);
            order_routes_.erase(value.original_order_id);
            order_routes_.emplace(value.new_order_id, route);
        }
    }, event);
}

const OrderBook* ShardedBookRouter::find(const std::string& symbol) const noexcept {
    const auto& books = shards_[shard_for(symbol)].books;
    const auto found = books.find(symbol);
    return found == books.end() ? nullptr : &found->second;
}

std::size_t ShardedBookRouter::symbol_count() const noexcept {
    std::size_t count = 0;
    for (const auto& shard : shards_) count += shard.books.size();
    return count;
}

std::vector<RoutedOrder> ShardedBookRouter::active_orders() const {
    std::vector<RoutedOrder> result;
    result.reserve(order_routes_.size());
    for (const auto& shard : shards_) {
        for (const auto& [symbol, book] : shard.books) {
            for (const auto& order : book.active_orders()) result.push_back({symbol, order});
        }
    }
    std::sort(result.begin(), result.end(),
        [](const RoutedOrder& left, const RoutedOrder& right) {
            return left.order.order_id < right.order.order_id;
        });
    return result;
}

void ShardedBookRouter::clear() {
    for (auto& shard : shards_) shard.books.clear();
    order_routes_.clear();
}

} // namespace marketcapture
