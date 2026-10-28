#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
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

}
