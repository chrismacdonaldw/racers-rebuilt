#include "app/hud/minimap.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "app/hud/hud_shapes.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr float kMapSize = 128.0F;
constexpr float kInset = render::kUiHeight * 0.03125F;
constexpr float kMapRight = render::kUiWidth - kInset;
constexpr float kMarkerSize = 16.0F;
constexpr float kDirectionScale = 0.12F;
constexpr float kTopV = 0.999F;
constexpr float kRightU = 0.999F;
constexpr float kImageAlpha = 200.0F / 255.0F;
constexpr render::Color kBlack{.r = 0.0F, .g = 0.0F, .b = 0.0F};
constexpr render::Color kGreen{.r = 0.0F, .g = 1.0F, .b = 0.0F};

struct ArrowShape {
    float tip;
    float back;
    float side;
    render::Color tint;
};

constexpr std::array kArrow{ArrowShape{.tip = 9.5F, .back = 6.5F, .side = 5.5F, .tint = kBlack},
                            ArrowShape{.tip = 8.0F, .back = 5.0F, .side = 4.0F, .tint = kGreen}};

float Truncated(float value) {
    return std::trunc(value);
}

render::SpriteCorner Corner(float x, float y, float u, float v, render::Color tint) {
    return {.x = x, .y = y, .u = u, .v = v, .tint = tint};
}

void DrawArrow(float x, float y, float forwardX, float forwardY, const render::Rect& clip,
               std::vector<render::SpriteDraw>& out) {
    for (const ArrowShape& shape : kArrow) {
        const float backX = -forwardX * shape.back;
        const float backY = -forwardY * shape.back;
        const float sideX = forwardY * shape.side;
        const float sideY = -forwardX * shape.side;
        AppendClipped(
            {Corner(x + backX + sideX, y + backY + sideY, 0.0F, 0.0F, shape.tint),
             Corner(x + (forwardX * shape.tip), y + (forwardY * shape.tip), 0.0F, 0.0F, shape.tint),
             Corner(x + backX - sideX, y + backY - sideY, 0.0F, 0.0F, shape.tint)},
            clip, render::kWhiteTexture, out);
    }
}

void DrawMarker(float left, float top, const MapMarker& marker, const UiImage& sheet,
                const HudPlacement& placement, std::vector<render::SpriteDraw>& out) {
    const float width = static_cast<float>(std::max(sheet.widthPx, 1U));
    const float height = static_cast<float>(std::max(sheet.heightPx, 1U));
    const float column = marker.isChampion ? 1.0F : 0.0F;
    const float row = placement.isSplit ? 1.0F : 0.0F;
    const render::Rect viewport = ViewportOf(placement);
    const float clippedTop = std::max(top, viewport.y);
    const float clippedBottom = std::min(top + kMarkerSize, viewport.y + viewport.height);
    if (clippedBottom <= clippedTop) {
        return;
    }
    out.push_back({.texture = sheet.texture,
                   .bounds = {.x = left,
                              .y = clippedTop,
                              .width = kMarkerSize,
                              .height = clippedBottom - clippedTop},
                   .uv = {.x = column * kMarkerSize / width,
                          .y = ((row * kMarkerSize) + clippedTop - top) / height,
                          .width = kMarkerSize / width,
                          .height = (clippedBottom - clippedTop) / height}});
}

void DrawImageQuad(const std::array<render::SpriteCorner, 4>& corners, render::TextureId image,
                   const render::Rect& clip, std::vector<render::SpriteDraw>& out) {
    AppendClipped({corners[0], corners[1], corners[3]}, clip, image, out);
    AppendClipped({corners[3], corners[1], corners[2]}, clip, image, out);
}

float TopV(const TrackMap& map) {
    return map.isMirrored ? 0.0F : kTopV;
}

float BottomV(const TrackMap& map) {
    return map.isMirrored ? kTopV : 0.0F;
}

}

void DrawFixedMap(const TrackMap& map, const MapView& view, const UiImage& markers,
                  const HudPlacement& placement, std::vector<render::SpriteDraw>& out) {
    const float mapBottom = placement.bottom - kInset;
    const render::Rect viewport = ViewportOf(placement);
    const float referenceY = map.isMirrored ? -map.maxY : map.minY;
    const float scale =
        kMapSize * placement.mapScale / std::max(map.maxX - map.minX, map.maxY - map.minY);
    if (map.image) {
        const float left = kMapRight + ((map.minX - map.maxX) * scale);
        const float top = mapBottom + ((map.minY - map.maxY) * scale);
        const render::Color tint{.a = kImageAlpha};
        DrawImageQuad({Corner(left, top, 0.0F, TopV(map), tint),
                       Corner(kMapRight, top, kRightU, TopV(map), tint),
                       Corner(kMapRight, mapBottom, kRightU, BottomV(map), tint),
                       Corner(left, mapBottom, 0.0F, BottomV(map), tint)},
                      *map.image, viewport, out);
    }
    const float originX = kMapRight - (kMarkerSize / 2.0F);
    const float originY = mapBottom - (kMarkerSize / 2.0F);
    for (const MapMarker& marker : view.markers) {
        DrawMarker(originX + Truncated((marker.position.x - map.maxX) * scale),
                   originY + Truncated((referenceY - marker.position.y) * scale), marker, markers,
                   placement, out);
    }
    const float length = std::hypot(view.playerForward.x, view.playerForward.y);
    DrawArrow(kMapRight + Truncated((view.player.x - map.maxX) * scale),
              mapBottom + Truncated((referenceY - view.player.y) * scale),
              view.playerForward.x / length, -view.playerForward.y / length, viewport, out);
}

void DrawRotatingMap(const TrackMap& map, const MapView& view, const UiImage& markers,
                     const HudPlacement& placement, std::vector<render::SpriteDraw>& out) {
    const float size = kMapSize * placement.mapScale;
    const float mapBottom = placement.bottom - kInset;
    const render::Rect square{
        .x = kMapRight - size, .y = mapBottom - size, .width = size, .height = size};
    const float centreX = Truncated(kMapRight - (size / 2.0F));
    const float centreY = Truncated(mapBottom - (size / 2.0F));
    const float length = std::hypot(view.viewDirection.x, view.viewDirection.y);
    const float upX = view.viewDirection.x * kDirectionScale * placement.mapScale / length;
    const float upY = view.viewDirection.y * kDirectionScale * placement.mapScale / length;
    const auto place = [&](float dx, float dy) {
        return std::array{centreX + ((dx * upY) - (dy * upX)), centreY - ((dx * upX) + (dy * upY))};
    };
    if (map.image) {
        const float nearY = (map.isMirrored ? -map.minY : map.maxY) - view.player.y;
        const float farY = (map.isMirrored ? -map.maxY : map.minY) - view.player.y;
        const float leftX = map.minX - view.player.x;
        const float rightX = map.maxX - view.player.x;
        const auto corner = [&](float dx, float dy, float u, float v) {
            const auto [x, y] = place(dx, dy);
            return Corner(x, y, u, v, {.a = kImageAlpha});
        };
        DrawImageQuad(
            {corner(leftX, nearY, 0.0F, TopV(map)), corner(rightX, nearY, kRightU, TopV(map)),
             corner(rightX, farY, kRightU, BottomV(map)), corner(leftX, farY, 0.0F, BottomV(map))},
            *map.image, square, out);
    }
    for (const MapMarker& marker : view.markers) {
        const float dx = marker.position.x - view.player.x;
        const float dy = marker.position.y - view.player.y;
        DrawMarker(centreX - (kMarkerSize / 2.0F) + Truncated((dx * upY) - (dy * upX)),
                   centreY - (kMarkerSize / 2.0F) - Truncated((dy * upY) + (dx * upX)), marker,
                   markers, placement, out);
    }
    DrawArrow(centreX, centreY, 0.0F, -1.0F, square, out);
    const float inner = size - 2.0F;
    for (const render::Rect& edge :
         {render::Rect{.x = square.x, .y = square.y, .width = size, .height = 1.0F},
          render::Rect{.x = square.x, .y = mapBottom - 1.0F, .width = size, .height = 1.0F},
          render::Rect{.x = square.x, .y = square.y + 1.0F, .width = 1.0F, .height = inner},
          render::Rect{
              .x = kMapRight - 1.0F, .y = square.y + 1.0F, .width = 1.0F, .height = inner}}) {
        out.push_back({.bounds = edge});
    }
}

}
