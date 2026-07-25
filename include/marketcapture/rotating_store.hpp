#pragma once

#include "marketcapture/mmap_store.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace marketcapture {

struct RetentionPolicy {
    std::size_t segment_capacity_bytes{256 * 1024 * 1024};
    std::size_t max_raw_bytes_per_segment{192 * 1024 * 1024};
    std::size_t max_segments{16};
    std::uint64_t max_total_bytes{4ull * 1024 * 1024 * 1024};
};

// A bounded, append-only segment set. Rotation occurs at block boundaries and
// retention removes only closed oldest segments, never the active segment.
class RotatingMappedStore {
public:
    RotatingMappedStore(std::filesystem::path directory, RetentionPolicy policy = {});
    ~RotatingMappedStore();
    RotatingMappedStore(const RotatingMappedStore&) = delete;
    RotatingMappedStore& operator=(const RotatingMappedStore&) = delete;

    void append(std::span<const std::uint8_t> block, int compression_level = 1);
    void flush();
    [[nodiscard]] std::vector<std::filesystem::path> segments() const;
    [[nodiscard]] std::uint64_t rotations() const noexcept { return rotations_; }

private:
    void open_next_segment();
    void enforce_retention();

    std::filesystem::path directory_;
    RetentionPolicy policy_;
    std::unique_ptr<MappedZstdStore> active_;
    std::filesystem::path active_path_;
    std::uint64_t next_id_{};
    std::uint64_t active_raw_bytes_{};
    std::uint64_t rotations_{};
};

} // namespace marketcapture
