#include "marketcapture/rotating_store.hpp"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace marketcapture {
namespace {
constexpr const char* prefix = "segment-";
constexpr const char* suffix = ".mcz";

bool is_segment(const std::filesystem::path& path) {
    const auto name = path.filename().string();
    return name.starts_with(prefix) && name.ends_with(suffix);
}
}

RotatingMappedStore::RotatingMappedStore(std::filesystem::path directory,
                                         RetentionPolicy policy)
    : directory_(std::move(directory)), policy_(policy) {
    if (policy_.segment_capacity_bytes < 8192 ||
        policy_.max_raw_bytes_per_segment == 0 ||
        policy_.max_segments == 0 ||
        policy_.max_total_bytes < policy_.segment_capacity_bytes)
        throw std::invalid_argument("invalid rotating-store retention policy");
    std::filesystem::create_directories(directory_);
    for (const auto& path : segments()) {
        const auto name = path.stem().string();
        try { next_id_ = std::max(next_id_, std::stoull(name.substr(8)) + 1); }
        catch (...) { /* Ignore unrelated names matching the broad pattern. */ }
    }
    open_next_segment();
    enforce_retention();
}

RotatingMappedStore::~RotatingMappedStore() = default;

std::vector<std::filesystem::path> RotatingMappedStore::segments() const {
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(directory_)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory_))
        if (entry.is_regular_file() && is_segment(entry.path()))
            result.push_back(entry.path());
    std::sort(result.begin(), result.end());
    return result;
}

void RotatingMappedStore::open_next_segment() {
    char name[64]{};
    std::snprintf(name, sizeof(name), "segment-%020llu.mcz",
                  static_cast<unsigned long long>(next_id_++));
    active_path_ = directory_ / name;
    active_ = std::make_unique<MappedZstdStore>(
        active_path_, policy_.segment_capacity_bytes);
    active_raw_bytes_ = 0;
}

void RotatingMappedStore::append(std::span<const std::uint8_t> block,
                                 int compression_level) {
    if (block.empty()) throw std::invalid_argument("cannot append empty block");
    if (block.size() > policy_.max_raw_bytes_per_segment)
        throw std::length_error("block exceeds configured segment raw-byte limit");
    if (active_raw_bytes_ != 0 &&
        active_raw_bytes_ + block.size() > policy_.max_raw_bytes_per_segment) {
        active_->flush();
        active_.reset();
        ++rotations_;
        open_next_segment();
        enforce_retention();
    }
    active_->append(block, compression_level);
    active_raw_bytes_ += block.size();
}

void RotatingMappedStore::flush() {
    if (active_) active_->flush();
}

void RotatingMappedStore::enforce_retention() {
    auto paths = segments();
    std::uint64_t total = 0;
    for (const auto& path : paths) total += std::filesystem::file_size(path);
    std::size_t index = 0;
    while ((paths.size() - index > policy_.max_segments ||
            total > policy_.max_total_bytes) &&
           index < paths.size() && paths[index] != active_path_) {
        const auto bytes = std::filesystem::file_size(paths[index]);
        std::filesystem::remove(paths[index]);
        total -= bytes;
        ++index;
    }
}

} // namespace marketcapture
