#include "app/racer/car_look.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "app/powerup/warp_start.hpp"
#include "sim/powerup/warp_effect.hpp"

namespace rr::app {
namespace {

constexpr float kChannelMax = 255.0F;

float ScaleOf(std::int32_t shift) {
    return std::ldexp(1.0F, -shift);
}

float OffsetOf(std::int32_t offset) {
    return static_cast<float>(offset) / kChannelMax;
}

render::VertexColorTransform CurseFlash() {
    return ToVertexColors({.shifts = {1, 0, 1, 0}, .offsets = {0, 64, 0, 0}});
}

}

render::VertexColorTransform ToVertexColors(const assets::ColorTransformResource& transform) {
    const auto& [redShift, greenShift, blueShift, alphaShift] = transform.shifts;
    const auto& [red, green, blue, alpha] = transform.offsets;
    return {
        .scale = {.r = ScaleOf(redShift),
                  .g = ScaleOf(greenShift),
                  .b = ScaleOf(blueShift),
                  .a = ScaleOf(alphaShift)},
        .offset = {
            .r = OffsetOf(red), .g = OffsetOf(green), .b = OffsetOf(blue), .a = OffsetOf(alpha)}};
}

void LookCars(const sim::RenderSnapshot& snapshot,
              std::span<const assets::ColorTransformResource> tints, std::span<CarLook> looks) {
    std::ranges::fill(looks, CarLook{});
    for (std::size_t i = 0; i < looks.size() && i < snapshot.racers.size(); ++i) {
        const std::optional<std::uint8_t> tint = snapshot.racers[i].tint;
        if (snapshot.racers[i].isCursed) {
            looks[i].colors = CurseFlash();
        } else if (tint && *tint < tints.size()) {
            looks[i].colors = ToVertexColors(tints[*tint]);
        }
    }
    for (const sim::EffectSnapshot& effect : snapshot.effects) {
        if (effect.kind == sim::PowerupKind::Warp && effect.use.racer < looks.size() &&
            effect.phase == static_cast<std::uint8_t>(sim::WarpPhase::Starting)) {
            looks[effect.use.racer].scale = WarpCarScale(effect.phaseLeftMs);
        }
    }
}

}
