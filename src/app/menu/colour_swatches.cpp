#include "app/menu/colour_swatches.hpp"

#include <algorithm>
#include <limits>

namespace rr::app {
namespace {

constexpr float kGapShare = 0.25F;
constexpr std::size_t kMostColours = std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1;

}

ColourSwatches::ColourSwatches(std::size_t colours, const render::Rect& area)
    : count_(std::min(colours, kMostColours)), area_(area) {
    if (count_ == 0) {
        return;
    }
    const auto count = static_cast<float>(count_);
    side_ = std::min(area.height, area.width / (count + (kGapShare * (count - 1.0F))));
    step_ = count_ > 1 ? (area.width - side_) / (count - 1.0F) : 0.0F;
}

render::Rect ColourSwatches::SwatchOf(std::size_t colour) const {
    return {.x = area_.x + (static_cast<float>(colour) * step_),
            .y = area_.y + ((area_.height - side_) / 2.0F),
            .width = side_,
            .height = side_};
}

std::optional<std::uint8_t> ColourSwatches::ColourAt(float x, float y) const {
    for (std::size_t colour = 0; colour < count_; ++colour) {
        const render::Rect swatch = SwatchOf(colour);
        if (x >= swatch.x && y >= swatch.y && x < swatch.x + swatch.width &&
            y < swatch.y + swatch.height) {
            return static_cast<std::uint8_t>(colour);
        }
    }
    return std::nullopt;
}

}
