#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <string_view>
#include <vector>

namespace rr::test {

[[nodiscard]] inline std::vector<std::byte> Bytes(std::initializer_list<std::uint8_t> values) {
    return values | std::views::transform([](std::uint8_t value) { return std::byte{value}; }) |
           std::ranges::to<std::vector>();
}

[[nodiscard]] inline std::vector<std::byte> Join(
    std::initializer_list<std::vector<std::byte>> parts) {
    return parts | std::views::join | std::ranges::to<std::vector>();
}

[[nodiscard]] inline std::vector<std::byte> ListHeader(std::uint8_t count) {
    return Bytes({0x07, 0x0B, count, 0x08, 0x05});
}

[[nodiscard]] inline std::vector<std::byte> Ints(std::initializer_list<std::uint8_t> values) {
    std::vector<std::byte> bytes;
    for (const std::uint8_t value : values) {
        bytes.insert(bytes.end(), {std::byte{0x0B}, std::byte{value}});
    }
    return bytes;
}

[[nodiscard]] inline std::vector<std::byte> Floats(std::initializer_list<std::int16_t> values) {
    std::vector<std::byte> bytes;
    for (const std::int16_t value : values) {
        const auto bits = static_cast<std::uint16_t>(value);
        bytes.insert(bytes.end(), {std::byte{0x11}, static_cast<std::byte>(bits & 0xFFU),
                                   static_cast<std::byte>(bits >> 8U)});
    }
    return bytes;
}

[[nodiscard]] inline std::vector<std::byte> Text(std::string_view text) {
    std::vector<std::byte> bytes{std::byte{0x02}};
    for (const char letter : text) {
        bytes.push_back(static_cast<std::byte>(letter));
    }
    bytes.push_back(std::byte{0x00});
    return bytes;
}

}
