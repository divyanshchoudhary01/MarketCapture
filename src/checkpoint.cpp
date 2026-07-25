#include "marketcapture/checkpoint.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace marketcapture {
namespace {
constexpr std::array<std::uint8_t, 8> magic{'M','C','C','H','K','P','T','2'};

void put(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t bytes) {
    for (std::size_t i = 0; i < bytes; ++i)
        out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
std::uint64_t get(const std::vector<std::uint8_t>& in, std::size_t& position, std::size_t bytes) {
    if (position + bytes > in.size()) throw std::runtime_error("truncated checkpoint");
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i)
        value |= std::uint64_t(in[position++]) << (i * 8);
    return value;
}
std::uint64_t checksum(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}
struct Decoded {
    CheckpointInfo info;
    std::vector<RoutedOrder> orders;
};
void sync_file(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle)) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        throw std::runtime_error("checkpoint durable flush failed");
    }
    CloseHandle(handle);
#else
    const auto descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0 || ::fsync(descriptor) != 0) {
        if (descriptor >= 0) ::close(descriptor);
        throw std::runtime_error("checkpoint durable flush failed");
    }
    ::close(descriptor);
#endif
}
void commit_file(const std::filesystem::path& temporary,
                 const std::filesystem::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("checkpoint atomic replacement failed");
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) throw std::runtime_error("checkpoint commit failed: " + error.message());
    auto parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    const auto directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0 || ::fsync(directory) != 0) {
        if (directory >= 0) ::close(directory);
        throw std::runtime_error("checkpoint directory flush failed");
    }
    ::close(directory);
#endif
}
std::optional<Decoded> read_slot(const std::filesystem::path& path) {
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::nullopt;
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
        if (data.size() < 40 || !std::equal(magic.begin(), magic.end(), data.begin()))
            return std::nullopt;
        std::size_t position = 8;
        Decoded result;
        result.info.generation = get(data, position, 8);
        result.info.sequence = get(data, position, 8);
        const auto count = get(data, position, 8);
        const auto expected_checksum = get(data, position, 8);
        if (checksum(data.data() + position, data.size() - position) != expected_checksum)
            return std::nullopt;
        result.orders.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto length = get(data, position, 2);
            if (length == 0 || position + length > data.size()) return std::nullopt;
            std::string symbol(reinterpret_cast<const char*>(data.data() + position),
                               static_cast<std::size_t>(length));
            position += static_cast<std::size_t>(length);
            ActiveOrder order;
            order.order_id = get(data, position, 8);
            const auto side = get(data, position, 1);
            if (side != 'B' && side != 'S') return std::nullopt;
            order.side = static_cast<Side>(side);
            order.shares = static_cast<std::uint32_t>(get(data, position, 4));
            order.price = static_cast<std::uint32_t>(get(data, position, 4));
            if (order.shares == 0) return std::nullopt;
            result.orders.push_back({std::move(symbol), order});
        }
        if (position != data.size()) return std::nullopt;
        result.info.orders = result.orders.size();
        return result;
    } catch (...) {
        return std::nullopt;
    }
}
}

CheckpointInfo CheckpointStore::save(const ShardedBookRouter& router,
                                     std::uint64_t sequence) const {
    const auto first = read_slot(base_path_.string() + ".a");
    const auto second = read_slot(base_path_.string() + ".b");
    const auto generation = std::max(first ? first->info.generation : 0,
                                     second ? second->info.generation : 0) + 1;
    const auto orders = router.active_orders();
    std::vector<std::uint8_t> payload;
    for (const auto& routed : orders) {
        if (routed.symbol.empty() || routed.symbol.size() > 65535)
            throw std::invalid_argument("invalid checkpoint symbol");
        put(payload, routed.symbol.size(), 2);
        payload.insert(payload.end(), routed.symbol.begin(), routed.symbol.end());
        put(payload, routed.order.order_id, 8);
        put(payload, static_cast<std::uint8_t>(routed.order.side), 1);
        put(payload, routed.order.shares, 4);
        put(payload, routed.order.price, 4);
    }
    std::vector<std::uint8_t> data(magic.begin(), magic.end());
    put(data, generation, 8); put(data, sequence, 8); put(data, orders.size(), 8);
    put(data, checksum(payload.data(), payload.size()), 8);
    data.insert(data.end(), payload.begin(), payload.end());

    const auto slot = std::filesystem::path(base_path_.string() +
        (generation % 2 == 0 ? ".a" : ".b"));
    const auto temporary = std::filesystem::path(slot.string() + ".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create checkpoint");
        output.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
        output.flush();
        if (!output) throw std::runtime_error("checkpoint write failed");
    }
    sync_file(temporary);
    commit_file(temporary, slot);
    return {generation, sequence, orders.size()};
}

CheckpointInfo CheckpointStore::load(ShardedBookRouter& router) const {
    const auto first = read_slot(base_path_.string() + ".a");
    const auto second = read_slot(base_path_.string() + ".b");
    if (!first && !second) throw std::runtime_error("no valid checkpoint generation");
    const auto& selected = !second || (first && first->info.generation > second->info.generation)
        ? *first : *second;
    ShardedBookRouter restored(router.shard_count());
    for (const auto& routed : selected.orders) {
        restored.apply(AddOrder{0, routed.order.order_id, routed.order.side,
            routed.order.shares, routed.symbol, routed.order.price});
    }
    router = std::move(restored);
    return selected.info;
}

} // namespace marketcapture
