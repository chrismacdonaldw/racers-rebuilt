#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/image_asset.hpp"
#include "render/render_view.hpp"

namespace rr::render {

inline constexpr std::size_t kSmokeVariants = 4;
inline constexpr std::uint32_t kSmokeCellPx = 128;

/// Billowing puffs in white with density in alpha, `kSmokeVariants` cells in a 2x2 grid.
[[nodiscard]] assets::ImageAsset MakeSmokeAtlas();

[[nodiscard]] Rect SmokeCell(std::size_t variant);

}
