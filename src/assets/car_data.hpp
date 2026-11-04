#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::assets {

inline constexpr std::size_t kCarDataSize = 0x202;

/// A racer record's car: a big-endian piece list, zero padded.
using CarData = std::array<std::byte, kCarDataSize>;

/// In list order, cut to the entries the data has room for.
[[nodiscard]] std::vector<std::uint16_t> PieceTypesOf(const CarData& car);

}
