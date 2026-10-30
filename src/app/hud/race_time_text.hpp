#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include "core/duration.hpp"

namespace rr::app {

inline constexpr std::size_t kRaceTimeLength = 8;

/// Writes `m:ss:cc`, or `mm:ss:cc` from ten minutes, wrapping every hour as the original does.
[[nodiscard]] std::u16string_view WriteRaceTime(Milliseconds time,
                                                std::span<char16_t, kRaceTimeLength> out);

}
