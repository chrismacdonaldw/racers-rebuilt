#include "app/hazard/water_scroll.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <variant>

#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kPeriod{10'000};
constexpr float kSineSteps = 1024.0F;
constexpr float kSpeedScale = 65.536F;
constexpr float kOffsetScale = 1.0F / 65'536.0F;

std::int16_t SpeedOf(float amplitude, float sine) {
    const float speed = amplitude * sine * kSpeedScale;
    if (!std::isfinite(speed)) {
        return 0;
    }
    constexpr auto kLowest = static_cast<float>(std::numeric_limits<std::int16_t>::min());
    constexpr auto kHighest = static_cast<float>(std::numeric_limits<std::int16_t>::max());
    return static_cast<std::int16_t>(std::clamp(speed, kLowest, kHighest));
}

std::uint16_t Advance(std::uint16_t offset, std::int16_t speed, Milliseconds elapsed) {
    return static_cast<std::uint16_t>(offset + (speed * static_cast<int>(elapsed.count())));
}

}

WaterScroll::WaterScroll(std::span<const assets::Hazard> hazards,
                         std::span<const NamedDraws> named) {
    for (const assets::Hazard& hazard : hazards) {
        const auto* scroll = std::get_if<assets::WaterScroll>(&hazard.payload);
        if (scroll == nullptr) {
            continue;
        }
        Surface& surface = surfaces_.emplace_back(
            Surface{.uAmplitude = scroll->uAmplitude, .vAmplitude = scroll->vAmplitude});
        const std::string entity = assets::ToUpperAscii(scroll->entity);
        for (const NamedDraws& draws : named) {
            if (assets::ToUpperAscii(draws.entity) == entity) {
                surface.spans.push_back(draws.span);
            }
        }
    }
}

void WaterScroll::Tick(Milliseconds elapsed) {
    phase_ = (phase_ + elapsed) % kPeriod;
    const float step = std::floor(kSineSteps * static_cast<float>(phase_.count()) /
                                  static_cast<float>(kPeriod.count()));
    const float sine = std::sin(2.0F * std::numbers::pi_v<float> * step / kSineSteps);
    for (Surface& surface : surfaces_) {
        surface.u = Advance(surface.u, surface.uSpeed, elapsed);
        surface.v = Advance(surface.v, surface.vSpeed, elapsed);
        surface.uSpeed = SpeedOf(surface.uAmplitude, sine);
        surface.vSpeed = SpeedOf(surface.vAmplitude, sine);
    }
}

void WaterScroll::Apply(std::span<render::MeshDraw> draws) const {
    for (const Surface& surface : surfaces_) {
        const render::UvOffset offset{.u = static_cast<float>(surface.u) * kOffsetScale,
                                      .v = static_cast<float>(surface.v) * kOffsetScale};
        for (const DrawSpan& span : surface.spans) {
            for (std::size_t draw = span.first; draw < span.first + span.count; ++draw) {
                draws[draw].material.uvOffset = offset;
            }
        }
    }
}

}
