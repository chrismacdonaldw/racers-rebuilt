#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::render {

enum class FxEffectId : std::uint16_t {};

inline constexpr std::size_t kMaxFxLayers = 8;

/// Rings lie flat on the ground; bricks tumble as solid models, not quads.
enum class FxShape : std::uint8_t { Puff, Streak, Glow, Ring, Brick };

struct FxRange {
    float min = 0.0F;
    float max = 0.0F;
};

/// Layers naming the same emitter play together; sizes are world units, speeds units per second.
struct FxEffect {
    std::string_view emitter;
    FxShape shape = FxShape::Puff;
    float ratePerSecond = 0.0F;
    int burst = 0;
    FxRange lifeMs;
    FxRange startSize;
    FxRange endSize;
    FxRange spinPerSecond;
    float inherit = 0.0F;
    Vector3 launch;
    Vector3 spread;
    Vector3 acceleration;
    float drag = 0.0F;
    float streakSeconds = 0.0F;
    float bounce = 0.0F;
    float floorDrop = 0.0F;
    Color color;
    std::optional<Color> endColor;
    std::span<const Color> palette;
    Vector3 offset;
    float shade = 1.0F;
    float fadeInShare = 0.1F;
    float fadeOutShare = 0.5F;
    BlendMode blend = BlendMode::Alpha;
};

struct FxEmission {
    bool hasBurst = false;
    std::array<float, kMaxFxLayers> carry{};
    std::optional<Vector3> lastPosition;
};

struct FxAssets {
    TextureId smoke = kWhiteTexture;
    TextureId spark = kWhiteTexture;
    TextureId glow = kWhiteTexture;
    TextureId ring = kWhiteTexture;
    MeshId brick{};
};

}
