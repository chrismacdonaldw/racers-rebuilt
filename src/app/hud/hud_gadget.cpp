#include "app/hud/hud_gadget.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "app/hud/hud_shapes.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kDialImage = 0;
constexpr std::size_t kCapImage = 1;
constexpr std::size_t kMarkerImage = 10;
constexpr std::uint8_t kModeCount = 4;
constexpr float kNewSpeedShare = 0.2F;
constexpr float kKeptSpeedShare = 0.8F;
constexpr float kRadiansPerSpeed = 18.0F;
constexpr float kMaxSweep = 3.4F;
constexpr float kZeroAngle = 2.25F;
constexpr float kThirdTurn = 2.0F * std::numbers::pi_v<float> / 3.0F;
constexpr float kNeedleLength = 45.0F;
constexpr float kNeedleBase = 7.0F;
constexpr float kDialMargin = 2.0F;
constexpr float kHubShare = 52.0F / 128.0F;
constexpr render::Color kNeedleGrey{
    .r = 150.0F / 255.0F, .g = 150.0F / 255.0F, .b = 150.0F / 255.0F};
constexpr render::Color kHubGrey{.r = 200.0F / 255.0F, .g = 200.0F / 255.0F, .b = 200.0F / 255.0F};
constexpr render::Color kWhite;

render::SpriteCorner Spoke(float x, float y, float angle, float length, render::Color tint) {
    return {.x = x + (std::cos(angle) * length), .y = y + (std::sin(angle) * length), .tint = tint};
}

float Width(const UiImage& image, float scale) {
    return std::trunc(static_cast<float>(image.widthPx) * scale);
}

float Height(const UiImage& image, float scale) {
    return std::trunc(static_cast<float>(image.heightPx) * scale);
}

}

void HudGadget::Cycle() {
    mode_ = static_cast<HudGadgetMode>((static_cast<std::uint8_t>(mode_) + 1) % kModeCount);
}

void HudGadget::Tick(float forwardSpeed) {
    speed_ = (forwardSpeed * kNewSpeedShare) + (speed_ * kKeptSpeedShare);
}

void HudGadget::Draw(const MapView& view, std::span<const UiImage> images,
                     const HudPlacement& placement, std::vector<render::SpriteDraw>& out) const {
    if (images.size() <= kMarkerImage) {
        return;
    }
    switch (mode_) {
        case HudGadgetMode::FixedMap:
            if (map_) {
                DrawFixedMap(*map_, view, images[kMarkerImage], placement, out);
            }
            break;
        case HudGadgetMode::Speedometer:
            DrawSpeedometer(speed_, images[kDialImage], images[kCapImage], placement, out);
            break;
        case HudGadgetMode::RotatingMap:
            if (map_) {
                DrawRotatingMap(*map_, view, images[kMarkerImage], placement, out);
            }
            break;
        case HudGadgetMode::None:
            break;
    }
}

void DrawSpeedometer(float speed, const UiImage& dial, const UiImage& cap,
                     const HudPlacement& placement, std::vector<render::SpriteDraw>& out) {
    const float scale = placement.mapScale;
    const float dialWidth = Width(dial, scale);
    const float dialHeight = Height(dial, scale);
    out.push_back({.texture = dial.texture,
                   .bounds = {.x = render::kUiWidth - dialWidth - kDialMargin,
                              .y = placement.bottom - dialHeight - kDialMargin,
                              .width = dialWidth,
                              .height = dialHeight}});
    const float hubX = render::kUiWidth - std::floor(dialWidth * kHubShare) - kDialMargin;
    const float hubY = placement.bottom - std::floor(dialHeight * kHubShare) - kDialMargin;
    const float angle = std::clamp(speed * kRadiansPerSpeed, 0.0F, kMaxSweep) + kZeroAngle;
    const float needle = kNeedleLength * scale;
    const float base = kNeedleBase * scale;
    const render::SpriteCorner hub{.x = hubX, .y = hubY, .tint = kHubGrey};
    AppendTriangle({Spoke(hubX, hubY, angle + (2.0F * kThirdTurn), base, kNeedleGrey),
                    Spoke(hubX, hubY, angle, needle, kNeedleGrey), hub},
                   render::kWhiteTexture, out);
    AppendTriangle({Spoke(hubX, hubY, angle + kThirdTurn, base, kWhite), hub,
                    Spoke(hubX, hubY, angle, needle, kWhite)},
                   render::kWhiteTexture, out);
    const float capWidth = Width(cap, scale);
    const float capHeight = Height(cap, scale);
    out.push_back({.texture = cap.texture,
                   .bounds = {.x = std::trunc(hubX) - std::floor(capWidth / 2.0F),
                              .y = std::trunc(hubY) - std::floor(capHeight / 2.0F),
                              .width = capWidth,
                              .height = capHeight}});
}

}
