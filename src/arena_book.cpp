#include "marketcapture/arena_book.hpp"
#include <algorithm>
#include <limits>
#include <memory_resource>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace marketcapture {
namespace {
using Symbol = std::array<char, 8>;
struct SymbolHash {
    std::size_t operator()(const Symbol& symbol) const noexcept {
        std::size_t value = 1469598103934665603ull;
        for (const auto byte : symbol) {
            value ^= static_cast<unsigned char>(byte);
            value *= 1099511628211ull;
        }
        return value;
    }
};
struct Order {
    Symbol symbol{};
    Side side{};
    std::uint32_t price{};
    std::uint32_t shares{};
};
struct LevelKey {
    Symbol symbol{};
    Side side{};
    std::uint32_t price{};
    bool operator==(const LevelKey&) const = default;
};
struct LevelHash {
    std::size_t operator()(const LevelKey& key) const noexcept {
        auto value = SymbolHash{}(key.symbol);
        value ^= static_cast<std::size_t>(key.price) * 0x9e3779b97f4a7c15ull;
        value ^= static_cast<std::size_t>(key.side);
        return value;
    }
};
}

class ArenaBookRouter::Impl {
public:
    Impl(std::size_t bytes, std::size_t max_orders, std::size_t max_levels,
         std::size_t max_symbols)
        : storage(std::make_unique<std::byte[]>(bytes)),
          arena(storage.get(), bytes, std::pmr::null_memory_resource()),
          pool(&arena), orders(&pool), levels(&pool), symbols(&pool),
          order_limit(max_orders), level_limit(max_levels), symbol_limit(max_symbols) {
        if (bytes == 0 || max_orders == 0 || max_levels == 0 || max_symbols == 0)
            throw std::invalid_argument("invalid arena book capacity");
        orders.reserve(max_orders);
        levels.reserve(max_levels);
        symbols.reserve(max_symbols);
    }

    void reduce(std::uint64_t id, std::uint32_t shares, bool erase_all) {
        auto found = orders.find(id);
        if (found == orders.end()) throw std::invalid_argument("unknown arena order");
        auto& order = found->second;
        if (!erase_all && (shares == 0 || shares > order.shares))
            throw std::invalid_argument("invalid arena reduction");
        const auto amount = erase_all ? order.shares : shares;
        const LevelKey key{order.symbol, order.side, order.price};
        auto level = levels.find(key);
        if (level == levels.end() || level->second.shares < amount)
            throw std::logic_error("arena level invariant violated");
        level->second.shares -= amount;
        order.shares -= amount;
        if (order.shares == 0) {
            --level->second.orders;
            orders.erase(found);
        }
        if (level->second.orders == 0) levels.erase(level);
    }

    std::unique_ptr<std::byte[]> storage;
    std::pmr::monotonic_buffer_resource arena;
    std::pmr::unsynchronized_pool_resource pool;
    std::pmr::unordered_map<std::uint64_t, Order> orders;
    std::pmr::unordered_map<LevelKey, ArenaLevel, LevelHash> levels;
    std::pmr::unordered_set<Symbol, SymbolHash> symbols;
    std::size_t order_limit, level_limit, symbol_limit;
};

ArenaBookRouter::ArenaBookRouter(std::size_t bytes, std::size_t max_orders,
                                 std::size_t max_levels, std::size_t max_symbols)
    : impl_(std::make_unique<Impl>(bytes, max_orders, max_levels, max_symbols)) {}
ArenaBookRouter::~ArenaBookRouter() = default;

void ArenaBookRouter::apply(const HotEvent& event) {
    if (!event.order_tick()) return;
    if (event.type == HotEventType::add || event.type == HotEventType::add_mpid) {
        if (event.shares == 0 || impl_->orders.contains(event.order_id))
            throw std::invalid_argument("invalid arena add");
        if (!impl_->symbols.contains(event.symbol) &&
            impl_->symbols.size() >= impl_->symbol_limit)
            throw std::length_error("arena symbol capacity exhausted");
        if (impl_->orders.size() >= impl_->order_limit)
            throw std::length_error("arena order capacity exhausted");
        const LevelKey key{event.symbol, event.side, event.price};
        auto level = impl_->levels.find(key);
        if (level == impl_->levels.end()) {
            if (impl_->levels.size() >= impl_->level_limit)
                throw std::length_error("arena level capacity exhausted");
            level = impl_->levels.emplace(key, ArenaLevel{event.price, 0, 0}).first;
        }
        impl_->symbols.insert(event.symbol);
        level->second.shares += event.shares;
        ++level->second.orders;
        impl_->orders.emplace(event.order_id,
            Order{event.symbol, event.side, event.price, event.shares});
    } else if (event.type == HotEventType::execute ||
               event.type == HotEventType::execute_price ||
               event.type == HotEventType::cancel) {
        impl_->reduce(event.order_id, event.shares, false);
    } else if (event.type == HotEventType::delete_order) {
        impl_->reduce(event.order_id, 0, true);
    } else if (event.type == HotEventType::replace) {
        auto found = impl_->orders.find(event.order_id);
        if (found == impl_->orders.end() || impl_->orders.contains(event.secondary_id))
            throw std::invalid_argument("invalid arena replace");
        const auto previous = found->second;
        impl_->reduce(event.order_id, 0, true);
        HotEvent replacement = event;
        replacement.type = HotEventType::add;
        replacement.order_id = event.secondary_id;
        replacement.symbol = previous.symbol;
        replacement.side = previous.side;
        apply(replacement);
    }
}

std::size_t ArenaBookRouter::order_count() const noexcept { return impl_->orders.size(); }
std::size_t ArenaBookRouter::symbol_count() const noexcept { return impl_->symbols.size(); }

std::optional<ArenaLevel> ArenaBookRouter::best_bid(const Symbol& symbol) const noexcept {
    std::optional<ArenaLevel> best;
    for (const auto& [key, level] : impl_->levels)
        if (key.symbol == symbol && key.side == Side::buy &&
            (!best || level.price > best->price)) best = level;
    return best;
}
std::optional<ArenaLevel> ArenaBookRouter::best_ask(const Symbol& symbol) const noexcept {
    std::optional<ArenaLevel> best;
    for (const auto& [key, level] : impl_->levels)
        if (key.symbol == symbol && key.side == Side::sell &&
            (!best || level.price < best->price)) best = level;
    return best;
}

} // namespace marketcapture
