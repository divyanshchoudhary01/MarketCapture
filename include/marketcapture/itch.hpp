#pragma once

#include "marketcapture/types.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace marketcapture {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ItchParser {
public:
    [[nodiscard]] Event parse(std::span<const std::uint8_t> message) const;
};

} // namespace marketcapture
