#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/color.hpp"

namespace rr::assets {

struct LdrawPrint {
    std::string_view torso;
    std::string_view file;
};

[[nodiscard]] std::span<const LdrawPrint> LdrawPrints();

/// Matches `torsoName` in any case.
[[nodiscard]] std::optional<LdrawPrint> LdrawPrintFor(std::string_view torsoName);

/// Empty for codes the prints don't use, and for the main colour 16.
[[nodiscard]] std::optional<Rgba8> LdrawColourOf(std::string_view code);

/// Draws `part`'s front over `torso`'s panel. Unknown colour codes take the torso's side colour.
[[nodiscard]] ImageAsset DrawTorsoPrint(const ImageAsset& torso, const GeometryAsset& part,
                                        std::uint32_t sizePx);

}
