#include "marketcapture/mmap_store.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace marketcapture {
namespace {
constexpr std::size_t control_size = 4096;
constexpr std::array<std::uint8_t, 8> file_magic{'M','C','M','A','P','Z','1','\0'};
constexpr std::uint32_t block_magic = 0x4d43424c;
constexpr std::size_t block_header_size = 24;

void put(std::uint8_t* output, std::uint64_t value, std::size_t bytes) {
    for (std::size_t i = 0; i < bytes; ++i)
        output[i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint64_t get(const std::uint8_t* input, std::size_t bytes) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i)
        value |= std::uint64_t(input[i]) << (i * 8);
    return value;
}
std::uint64_t checksum(std::span<const std::uint8_t> data) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : data) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

class ZstdRuntime {
public:
    using Bound = std::size_t (*)(std::size_t);
    using Compress = std::size_t (*)(void*, std::size_t, const void*, std::size_t, int);
    using Decompress = std::size_t (*)(void*, std::size_t, const void*, std::size_t);
    using IsError = unsigned (*)(std::size_t);

    ZstdRuntime() {
#ifdef _WIN32
        library_ = LoadLibraryA("libzstd.dll");
        if (!library_) library_ = LoadLibraryA("zstd.dll");
        const auto symbol = [&](const char* name) {
            return reinterpret_cast<void*>(GetProcAddress(library_, name));
        };
#else
        library_ = dlopen("libzstd.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!library_) library_ = dlopen("libzstd.so", RTLD_NOW | RTLD_LOCAL);
        const auto symbol = [&](const char* name) { return dlsym(library_, name); };
#endif
        if (!library_) throw std::runtime_error("Zstandard runtime library is unavailable");
        bound = reinterpret_cast<Bound>(symbol("ZSTD_compressBound"));
        compress = reinterpret_cast<Compress>(symbol("ZSTD_compress"));
        decompress = reinterpret_cast<Decompress>(symbol("ZSTD_decompress"));
        is_error = reinterpret_cast<IsError>(symbol("ZSTD_isError"));
        if (!bound || !compress || !decompress || !is_error)
            throw std::runtime_error("Zstandard runtime ABI is incomplete");
    }
    ~ZstdRuntime() {
#ifdef _WIN32
        if (library_) FreeLibrary(library_);
#else
        if (library_) dlclose(library_);
#endif
    }
    Bound bound{};
    Compress compress{};
    Decompress decompress{};
    IsError is_error{};
private:
#ifdef _WIN32
    HMODULE library_{};
#else
    void* library_{};
#endif
};
}

class MappedZstdStore::Impl {
public:
    Impl(const std::filesystem::path& path, std::size_t requested_capacity)
        : capacity(std::max(requested_capacity, control_size + block_header_size + 1)) {
#ifdef _WIN32
        file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open mmap store");
        LARGE_INTEGER size; size.QuadPart = static_cast<LONGLONG>(capacity);
        if (!SetFilePointerEx(file, size, nullptr, FILE_BEGIN) || !SetEndOfFile(file))
            throw std::runtime_error("cannot size mmap store");
        mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE,
            static_cast<DWORD>(capacity >> 32), static_cast<DWORD>(capacity), nullptr);
        if (!mapping) throw std::runtime_error("cannot create mmap store mapping");
        data = static_cast<std::uint8_t*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity));
        if (!data) throw std::runtime_error("cannot map store");
#else
        file = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
        if (file < 0 || ftruncate(file, static_cast<off_t>(capacity)) != 0)
            throw std::runtime_error("cannot size mmap store");
        data = static_cast<std::uint8_t*>(
            mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0));
        if (data == MAP_FAILED) throw std::runtime_error("cannot map store");
#endif
        if (!std::equal(file_magic.begin(), file_magic.end(), data)) {
            std::memset(data, 0, control_size);
            std::copy(file_magic.begin(), file_magic.end(), data);
            put(data + 8, 1, 4);
            put(data + 16, capacity, 8);
            put(data + 24, control_size, 8);
            sync(0, control_size);
        } else {
            if (get(data + 8, 4) != 1 || get(data + 16, 8) != capacity)
                throw std::runtime_error("mmap store header or capacity mismatch");
            validate();
        }
    }
    ~Impl() {
        try { flush_all(); } catch (...) {}
#ifdef _WIN32
        if (data) UnmapViewOfFile(data);
        if (mapping) CloseHandle(mapping);
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
#else
        if (data && data != MAP_FAILED) munmap(data, capacity);
        if (file >= 0) ::close(file);
#endif
    }

    void sync(std::size_t offset, std::size_t length) {
#ifdef _WIN32
        if (!FlushViewOfFile(data + offset, length) || !FlushFileBuffers(file))
            throw std::runtime_error("mmap durable flush failed");
#else
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        const auto begin = offset & ~(page - 1);
        const auto end = (offset + length + page - 1) & ~(page - 1);
        if (msync(data + begin, end - begin, MS_SYNC) != 0 || fsync(file) != 0)
            throw std::runtime_error("mmap durable flush failed");
#endif
    }
    void flush_all() { sync(0, static_cast<std::size_t>(committed())); }
    std::uint64_t committed() const { return get(data + 24, 8); }
    std::uint64_t blocks() const { return get(data + 32, 8); }

    void validate() const {
        const auto end = committed();
        if (end < control_size || end > capacity)
            throw std::runtime_error("invalid mmap committed offset");
        std::size_t position = control_size;
        std::uint64_t count = 0;
        while (position < end) {
            if (position + block_header_size > end ||
                get(data + position, 4) != block_magic)
                throw std::runtime_error("invalid mmap block header");
            const auto compressed = get(data + position + 8, 4);
            if (compressed == 0 || position + block_header_size + compressed > end)
                throw std::runtime_error("invalid mmap block size");
            position += block_header_size + static_cast<std::size_t>(compressed);
            ++count;
        }
        if (position != end || count != blocks())
            throw std::runtime_error("mmap block count mismatch");
    }

    std::size_t capacity;
    std::uint8_t* data{};
    ZstdRuntime zstd;
#ifdef _WIN32
    HANDLE file{INVALID_HANDLE_VALUE};
    HANDLE mapping{};
#else
    int file{-1};
#endif
};

MappedZstdStore::MappedZstdStore(const std::filesystem::path& path,
                                 std::size_t capacity_bytes)
    : impl_(std::make_unique<Impl>(path, capacity_bytes)) {}
MappedZstdStore::~MappedZstdStore() = default;

void MappedZstdStore::append(std::span<const std::uint8_t> block, int compression_level) {
    if (block.empty()) throw std::invalid_argument("cannot append empty mmap block");
    std::vector<std::uint8_t> compressed(impl_->zstd.bound(block.size()));
    const auto compressed_size = impl_->zstd.compress(compressed.data(), compressed.size(),
        block.data(), block.size(), compression_level);
    if (impl_->zstd.is_error(compressed_size))
        throw std::runtime_error("Zstd compression failed");
    const auto position = static_cast<std::size_t>(impl_->committed());
    const auto next = position + block_header_size + compressed_size;
    if (next > impl_->capacity) throw std::runtime_error("mmap store capacity exhausted");
    auto* header = impl_->data + position;
    put(header, block_magic, 4); put(header + 4, block.size(), 4);
    put(header + 8, compressed_size, 4); put(header + 12, 0, 4);
    put(header + 16, checksum(block), 8);
    std::memcpy(header + block_header_size, compressed.data(), compressed_size);
    impl_->sync(position, block_header_size + compressed_size);
    put(impl_->data + 24, next, 8);
    put(impl_->data + 32, impl_->blocks() + 1, 8);
    impl_->sync(0, control_size);
}

std::size_t MappedZstdStore::replay(const Handler& handler) const {
    std::size_t position = control_size;
    std::size_t count = 0;
    const auto end = static_cast<std::size_t>(impl_->committed());
    while (position < end) {
        const auto raw_size = static_cast<std::size_t>(get(impl_->data + position + 4, 4));
        const auto compressed_size = static_cast<std::size_t>(get(impl_->data + position + 8, 4));
        const auto expected = get(impl_->data + position + 16, 8);
        std::vector<std::uint8_t> raw(raw_size);
        const auto result = impl_->zstd.decompress(raw.data(), raw.size(),
            impl_->data + position + block_header_size, compressed_size);
        if (impl_->zstd.is_error(result) || result != raw_size || checksum(raw) != expected)
            throw std::runtime_error("corrupt compressed mmap block");
        handler(raw);
        position += block_header_size + compressed_size;
        ++count;
    }
    return count;
}

MappedStoreStats MappedZstdStore::stats() const {
    MappedStoreStats result;
    result.blocks = impl_->blocks();
    std::size_t position = control_size;
    while (position < impl_->committed()) {
        result.raw_bytes += get(impl_->data + position + 4, 4);
        const auto compressed = get(impl_->data + position + 8, 4);
        result.compressed_bytes += compressed;
        position += block_header_size + static_cast<std::size_t>(compressed);
    }
    return result;
}

void MappedZstdStore::flush() { impl_->flush_all(); }

} // namespace marketcapture
