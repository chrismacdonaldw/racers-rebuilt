#pragma once

#include <cstddef>
#include <span>

#include "assets/image_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Accepts the game's own compressed layout and uncompressed Windows BMP.
[[nodiscard]] Result<ImageAsset> DecodeBmp(std::span<const std::byte> bytes);

}
