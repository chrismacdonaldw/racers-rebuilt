#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "render/render_view.hpp"

namespace rr::render {

inline constexpr std::size_t kSmokeVariants = 4;
inline constexpr std::uint32_t kSmokeCellPx = 128;

/// Billowing puffs in white with density in alpha, `kSmokeVariants` cells in a 2x2 grid.
[[nodiscard]] assets::ImageAsset MakeSmokeAtlas();

[[nodiscard]] Rect SmokeCell(std::size_t variant);

/// A white spark, hot at its head on the top edge and fading down its tail.
[[nodiscard]] assets::ImageAsset MakeSparkTexture();

/// A soft white round light.
[[nodiscard]] assets::ImageAsset MakeGlowTexture();

/// A soft white ring near the image's edge.
[[nodiscard]] assets::ImageAsset MakeRingTexture();

/// A dark glossy bubble with a rainbow sheen at its rim and a white highlight.
[[nodiscard]] assets::ImageAsset MakeBubbleTexture();

/// A white four-pointed twinkle.
[[nodiscard]] assets::ImageAsset MakeStarTexture();

/// A 1 by 2 brick with two studs, one unit long, centred on its middle and facing up +y.
[[nodiscard]] assets::GeometryAsset MakeBrickGeometry();

}
