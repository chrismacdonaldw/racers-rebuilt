#pragma once

#include <array>
#include <cstddef>

namespace rr::assets {

inline constexpr std::size_t kCarDataSize = 0x202;

/// A racer record's car: a big-endian piece list, zero padded.
using CarData = std::array<std::byte, kCarDataSize>;

}
