#pragma once

#include "marketcapture/order_book.hpp"
#include <cstdint>
#include <vector>

namespace marketcapture {

struct BookSnapshot {
    std::uint64_t sequence{};
    std::vector<Level> bids;
    std::vector<Level> asks;
};

class SnapshotManager {
public:
    explicit SnapshotManager(std::size_t depth = 10) : depth_(depth) {}
    [[nodiscard]] BookSnapshot capture(const OrderBook& book, std::uint64_t sequence) const {
        return {sequence, book.bids(depth_), book.asks(depth_)};
    }
private:
    std::size_t depth_;
};

} // namespace marketcapture
