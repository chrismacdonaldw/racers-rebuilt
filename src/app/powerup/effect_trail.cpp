#include "app/powerup/effect_trail.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kSegmentsPerLife = 4.0F;
constexpr float kTailShare = 0.1F;
constexpr Vector3 kGameUp{.z = 1.0F};
constexpr Vector3 kGameSide{.x = 1.0F};

using Ring = std::array<Vector3, 4>;

Ring RingAt(const Vector3& centre, const Vector3& side, float halfWidth) {
    const Vector3 across = side * halfWidth;
    const Vector3 up = kGameUp * halfWidth;
    return {ToEngine(centre + up - across), ToEngine(centre - up - across),
            ToEngine(centre - up + across), ToEngine(centre + up + across)};
}

}

void EffectTrail::Follow(const Vector3& head, const TrailStyle& style, float elapsedMs) {
    for (Point& point : points_) {
        point.ageMs += elapsedMs;
    }
    sinceSampleMs_ += elapsedMs;
    const float sampleMs = style.lifeMs / kSegmentsPerLife;
    if (points_.size() < 2 || sinceSampleMs_ >= sampleMs) {
        points_.push_back({.position = head});
        sinceSampleMs_ = 0.0F;
    }
    points_.back() = {.position = head};
    std::erase_if(points_, [&](const Point& point) { return point.ageMs > style.lifeMs; });
}

void EffectTrail::Clear() {
    points_.clear();
    sinceSampleMs_ = 0.0F;
}

void EffectTrail::Append(const QuadLook& look, const TrailStyle& style,
                         std::vector<render::QuadDraw>& out) const {
    const render::Color tint = Modulate(look.tint, style.colour);
    const auto halfWidth = [&](const Point& point) {
        const float share = std::clamp(point.ageMs / style.lifeMs, 0.0F, 1.0F);
        return style.width * 0.5F * (1.0F - ((1.0F - kTailShare) * share));
    };
    for (std::size_t i = 1; i < points_.size(); ++i) {
        const Point& tail = points_[i - 1];
        const Point& head = points_[i];
        Vector3 along = head.position - tail.position;
        along.z = 0.0F;
        const Vector3 side = Normalized(Cross(along, kGameUp), kGameSide);
        const Ring from = RingAt(tail.position, side, halfWidth(tail));
        const Ring to = RingAt(head.position, side, halfWidth(head));
        for (std::size_t edge = 0; edge < from.size(); ++edge) {
            const std::size_t next = (edge + 1) % from.size();
            out.push_back({.texture = look.texture,
                           .corners = {from[edge], to[edge], from[next], to[next]},
                           .tint = tint,
                           .blend = look.blend});
        }
    }
}

}
