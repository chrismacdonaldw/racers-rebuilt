#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace rr::assets {

[[nodiscard]] inline std::uint16_t LoadU16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(bytes[offset]) |
                                      (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8U));
}

[[nodiscard]] inline std::uint32_t LoadU32(std::span<const std::byte> bytes, std::size_t offset) {
    return LoadU16(bytes, offset) | (std::uint32_t{LoadU16(bytes, offset + 2)} << 16U);
}

}
