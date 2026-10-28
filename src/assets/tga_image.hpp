#pragma once

#include <cstddef>
#include <span>

#include "assets/image_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Accepts uncompressed 24- and 32-bit true-color TGA, the only kind the game ships.
[[nodiscard]] Result<ImageAsset> DecodeTga(std::span<const std::byte> bytes);

}
