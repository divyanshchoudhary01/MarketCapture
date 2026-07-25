#pragma once

#include "marketcapture/book_router.hpp"
#include <cstdint>
#include <filesystem>

namespace marketcapture {

struct CheckpointInfo {
    std::uint64_t generation{};
    std::uint64_t sequence{};
    std::size_t orders{};
};

class CheckpointStore {
public:
    explicit CheckpointStore(std::filesystem::path base_path)
        : base_path_(std::move(base_path)) {}

    [[nodiscard]] CheckpointInfo save(const ShardedBookRouter& router,
                                      std::uint64_t sequence) const;
    [[nodiscard]] CheckpointInfo load(ShardedBookRouter& router) const;

private:
    std::filesystem::path base_path_;
};

} // namespace marketcapture
