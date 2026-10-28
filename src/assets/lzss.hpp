#pragma once

#include <cstddef>
#include <span>

#include "core/result.hpp"

namespace rr::assets {

/// Fails unless the packed stream ends with its terminator exactly when `unpacked` is full.
[[nodiscard]] Result<void> DecodeLzss(std::span<const std::byte> packed,
                                      std::span<std::byte> unpacked);

}
