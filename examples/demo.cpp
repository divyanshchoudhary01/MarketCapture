#include "marketcapture/order_book.hpp"
#include "marketcapture/snapshot.hpp"
#include <iomanip>
#include <iostream>

int main() {
    using namespace marketcapture;
    OrderBook book;
    book.apply(AddOrder{1, 1001, Side::buy, 200, "AAPL", 1892500});
    book.apply(AddOrder{2, 1002, Side::sell, 150, "AAPL", 1892600});
    const auto snapshot = SnapshotManager{5}.capture(book, 2);
    std::cout << snapshot.bids.front().shares << " @ $"
              << std::fixed << std::setprecision(4) << snapshot.bids.front().price / 10000.0
              << " | " << snapshot.asks.front().shares << " @ $"
              << snapshot.asks.front().price / 10000.0 << '\n';
}
