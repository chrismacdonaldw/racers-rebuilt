#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::render {

enum class FxEffectId : std::uint16_t {};

struct FxRange {
    float min = 0.0F;
    float max = 0.0F;
};

/// Sizes are world units across, speeds units per second, and `drag` the share of speed lost per
/// second. `inherit` is the share of the emitter's velocity a particle keeps.
struct FxEffect {
    std::string_view emitter;
    float ratePerSecond = 0.0F;
    int burst = 0;
    FxRange lifeMs;
    FxRange startSize;
    FxRange endSize;
    FxRange spinPerSecond;
    float inherit = 0.0F;
    Vector3 spread;
    Vector3 acceleration;
    float drag = 0.0F;
    Color color;
    float shade = 1.0F;
    float fadeInShare = 0.1F;
    float fadeOutShare = 0.5F;
    BlendMode blend = BlendMode::Alpha;
};

struct FxEmission {
    bool hasBurst = false;
    float carry = 0.0F;
    std::optional<Vector3> lastPosition;
};

}
