#pragma once

#include "marketcapture/order_book.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace marketcapture {

struct RoutedOrder {
    std::string symbol;
    ActiveOrder order;
};

class ShardedBookRouter {
public:
    explicit ShardedBookRouter(std::size_t shard_count);

    void apply(const Event& event);
    [[nodiscard]] std::size_t shard_for(const std::string& symbol) const noexcept;
    [[nodiscard]] const OrderBook* find(const std::string& symbol) const noexcept;
    [[nodiscard]] std::size_t shard_count() const noexcept { return shards_.size(); }
    [[nodiscard]] std::size_t symbol_count() const noexcept;
    [[nodiscard]] std::size_t order_count() const noexcept { return order_to_shard_.size(); }
    [[nodiscard]] std::vector<RoutedOrder> active_orders() const;
    void clear();

private:
    struct Shard {
        std::unordered_map<std::string, OrderBook> books;
    };

    OrderBook& book_for(const std::string& symbol);
    OrderBook& book_for_order(std::uint64_t order_id);
    std::vector<Shard> shards_;
    std::unordered_map<std::uint64_t, std::size_t> order_to_shard_;
};

} // namespace marketcapture
