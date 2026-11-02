#include "app/hud/hud_placement.hpp"

namespace rr::app {
namespace {

constexpr float kSplitTextScale = 0.9F;
constexpr float kSplitBannerScale = 0.8F;
constexpr float kSplitMapScale = 0.8F;

}

HudPlacement PlacementFor(std::size_t player, std::size_t players) {
    if (players <= 1) {
        return {};
    }
    const float height = render::kUiHeight / static_cast<float>(players);
    const float top = height * static_cast<float>(player);
    return {.top = top,
            .bottom = top + height,
            .textScale = kSplitTextScale,
            .bannerScale = kSplitBannerScale,
            .mapScale = kSplitMapScale,
            .isSplit = true};
}

render::Rect ViewportOf(const HudPlacement& placement) {
    return {
        .y = placement.top, .width = render::kUiWidth, .height = placement.bottom - placement.top};
}

}
