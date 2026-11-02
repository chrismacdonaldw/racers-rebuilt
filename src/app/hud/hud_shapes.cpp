#include "app/hud/hud_shapes.hpp"

#include <array>
#include <cstddef>

namespace rr::app {
namespace {

constexpr std::size_t kMaxClippedCorners = 8;

struct Polygon {
    std::array<render::SpriteCorner, kMaxClippedCorners> corners{};
    std::size_t count = 0;

    void Add(const render::SpriteCorner& corner) { corners.at(count++) = corner; }
};

render::SpriteCorner Cut(const render::SpriteCorner& inside, const render::SpriteCorner& outside,
                         float amount) {
    return {.x = inside.x + ((outside.x - inside.x) * amount),
            .y = inside.y + ((outside.y - inside.y) * amount),
            .u = inside.u + ((outside.u - inside.u) * amount),
            .v = inside.v + ((outside.v - inside.v) * amount),
            .tint = outside.tint};
}

template <typename InsideBy>
Polygon ClipEdge(const Polygon& polygon, InsideBy insideBy) {
    Polygon kept;
    for (std::size_t i = 0; i < polygon.count; ++i) {
        const render::SpriteCorner& from = polygon.corners[i];
        const render::SpriteCorner& to = polygon.corners[(i + 1) % polygon.count];
        const float fromDistance = insideBy(from);
        const float toDistance = insideBy(to);
        if (fromDistance >= 0.0F) {
            kept.Add(from);
        }
        if ((fromDistance >= 0.0F) != (toDistance >= 0.0F)) {
            const float amount = fromDistance / (fromDistance - toDistance);
            kept.Add(fromDistance >= 0.0F ? Cut(from, to, amount) : Cut(to, from, 1.0F - amount));
        }
    }
    return kept;
}

}

void AppendTriangle(const HudTriangle& triangle, render::TextureId texture,
                    std::vector<render::SpriteDraw>& out) {
    out.push_back({.texture = texture,
                   .corners = std::array{triangle[0], triangle[1], triangle[2], triangle[2]}});
}

void AppendClipped(const HudTriangle& triangle, const render::Rect& clip, render::TextureId texture,
                   std::vector<render::SpriteDraw>& out) {
    Polygon polygon{.corners = {triangle[0], triangle[1], triangle[2]}, .count = triangle.size()};
    const float right = clip.x + clip.width;
    const float bottom = clip.y + clip.height;
    polygon = ClipEdge(polygon, [&](const render::SpriteCorner& c) { return c.y - clip.y; });
    polygon = ClipEdge(polygon, [&](const render::SpriteCorner& c) { return c.x - clip.x; });
    polygon = ClipEdge(polygon, [&](const render::SpriteCorner& c) { return right - c.x; });
    polygon = ClipEdge(polygon, [&](const render::SpriteCorner& c) { return bottom - c.y; });
    for (std::size_t i = 2; i < polygon.count; ++i) {
        AppendTriangle({polygon.corners[0], polygon.corners[i - 1], polygon.corners[i]}, texture,
                       out);
    }
}

}
