#include "marketcapture/moldudp64.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        (void)marketcapture::MoldUdp64Decoder{}.decode(
            std::span<const std::uint8_t>(data, size));
    } catch (...) {
    }
    return 0;
}
