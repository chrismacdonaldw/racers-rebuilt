#pragma once

#include <cstddef>
#include <span>

#include "assets/image_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

[[nodiscard]] bool IsPng(std::span<const std::byte> bytes);

[[nodiscard]] Result<ImageAsset> DecodePng(std::span<const std::byte> bytes);

}
