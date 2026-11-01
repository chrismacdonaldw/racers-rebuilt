#include "app/powerup/warp_start.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr float kStartMs = 500.0F;
constexpr float kShrinkMs = 250.0F;
constexpr float kFlashMs = 200.0F;
constexpr float kFovBoostDegrees = 30.0F;
constexpr float kFlashBlue = 100.0F / 255.0F;
constexpr float kFlashSpan = 3.0F;

bool IsWarpIn(const sim::EffectSnapshot& effect, sim::WarpPhase phase) {
    return effect.kind == sim::PowerupKind::Warp &&
           effect.phase == static_cast<std::uint8_t>(phase);
}

}

const sim::EffectSnapshot* FindWarp(std::span<const sim::EffectSnapshot> effects, std::size_t racer,
                                    sim::WarpPhase phase) {
    const auto warp = std::ranges::find_if(effects, [&](const sim::EffectSnapshot& effect) {
        return effect.use.racer == racer && IsWarpIn(effect, phase);
    });
    return warp == effects.end() ? nullptr : &*warp;
}

float WarpPortalScale(float leftMs) {
    return std::sin(std::numbers::pi_v<float> * leftMs / kStartMs);
}

float WarpCarScale(float leftMs) {
    return leftMs < kShrinkMs ? WarpPortalScale(leftMs) : 1.0F;
}

render::Camera WarpStartCamera(render::Camera camera, float leftMs) {
    const float flown = (kStartMs - leftMs) / kStartMs;
    camera.verticalFovDegrees +=
        kFovBoostDegrees * (1.0F - std::cos(std::numbers::pi_v<float> * flown)) / 2.0F;
    return camera;
}

std::optional<render::SpriteDraw> WarpFlash(float leftMs) {
    if (leftMs >= kFlashMs) {
        return std::nullopt;
    }
    return render::SpriteDraw{
        .bounds = {.x = -render::kUiWidth,
                   .y = -render::kUiHeight,
                   .width = kFlashSpan * render::kUiWidth,
                   .height = kFlashSpan * render::kUiHeight},
        .tint = {.r = 0.0F, .g = 0.0F, .b = kFlashBlue, .a = 1.0F - (leftMs / kFlashMs)}};
}

}
