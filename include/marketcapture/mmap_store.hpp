#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace marketcapture {

struct MappedStoreStats {
    std::uint64_t blocks{};
    std::uint64_t raw_bytes{};
    std::uint64_t compressed_bytes{};
};

class MappedZstdStore {
public:
    using Handler = std::function<void(std::span<const std::uint8_t>)>;

    MappedZstdStore(const std::filesystem::path& path, std::size_t capacity_bytes);
    ~MappedZstdStore();
    MappedZstdStore(const MappedZstdStore&) = delete;
    MappedZstdStore& operator=(const MappedZstdStore&) = delete;

    void append(std::span<const std::uint8_t> block, int compression_level = 1);
    [[nodiscard]] std::size_t replay(const Handler& handler) const;
    [[nodiscard]] MappedStoreStats stats() const;
    void flush();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketcapture
