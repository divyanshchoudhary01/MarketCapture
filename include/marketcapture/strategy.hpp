#pragma once

#include "marketcapture/order_book.hpp"
#include <vector>

namespace marketcapture {

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual void on_event(const Event&, const OrderBook&) = 0;
};

class StrategyDispatcher {
public:
    void add(Strategy& strategy) { strategies_.push_back(&strategy); }
    void dispatch(const Event& event, const OrderBook& book) {
        for (auto* strategy : strategies_) strategy->on_event(event, book);
    }
private:
    std::vector<Strategy*> strategies_;
};

} // namespace marketcapture
