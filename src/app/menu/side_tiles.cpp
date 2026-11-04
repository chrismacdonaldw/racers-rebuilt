#include "app/menu/side_tiles.hpp"

#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr int kTilesPerSide = 2;

[[nodiscard]] bool SpansHeight(const render::Rect& bounds) {
    return bounds.y <= 0.0F && bounds.y + bounds.height >= render::kUiHeight;
}

void AppendTiles(const MenuPicture& picture, float step, std::vector<render::SpriteDraw>& out) {
    for (int tile = 1; tile <= kTilesPerSide; ++tile) {
        const bool isMirrored = tile % 2 == 1;
        render::Rect bounds = picture.bounds;
        bounds.x += step * static_cast<float>(tile);
        out.push_back({.texture = picture.texture,
                       .bounds = bounds,
                       .uv = isMirrored ? render::Rect{.x = 1.0F, .width = -1.0F, .height = 1.0F}
                                        : render::Rect{.width = 1.0F, .height = 1.0F},
                       .tint = picture.tint,
                       .anchor = render::SpriteAnchor::Beside});
    }
}

}

void AppendSideTiles(const MenuPicture& picture, std::vector<render::SpriteDraw>& out) {
    const render::Rect& bounds = picture.bounds;
    if (bounds.width <= 0.0F || !SpansHeight(bounds)) {
        return;
    }
    if (bounds.x <= 0.0F) {
        AppendTiles(picture, -bounds.width, out);
    }
    if (bounds.x + bounds.width >= render::kUiWidth) {
        AppendTiles(picture, bounds.width, out);
    }
}

}
