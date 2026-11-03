#include "app/menu/menu_frame.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace rr::app {
namespace {

BoxSize SizeOf(const UiImage& image) {
    return {.width = static_cast<float>(image.widthPx),
            .height = static_cast<float>(image.heightPx)};
}

render::Color TintOr(const std::optional<assets::MenuColor>& color) {
    return color ? ToColor(*color) : render::Color{};
}

std::array<render::Rect, kFrameParts + 1> SlicesOf(const MenuFrameLook& look,
                                                   const render::Rect& frame) {
    std::array<BoxSize, kFrameParts> sizes{};
    std::ranges::transform(look.border, sizes.begin(), SizeOf);
    return FrameSlices(frame, sizes);
}

void PushVisible(const render::SpriteDraw& sprite, std::vector<render::SpriteDraw>& out) {
    if (sprite.bounds.width > 0.0F && sprite.bounds.height > 0.0F) {
        out.push_back(sprite);
    }
}

void DrawSlices(const MenuFrameLook& look, const std::array<render::Rect, kFrameParts + 1>& slices,
                std::vector<render::SpriteDraw>& out) {
    for (std::size_t part = 0; part < kFrameParts; ++part) {
        PushVisible({.texture = look.border.at(part).texture,
                     .bounds = slices.at(part),
                     .tint = look.borderTint},
                    out);
    }
}

}

Result<MenuFrameLook> LoadMenuFrame(MenuImageCache& images, const assets::FrameElement& frame) {
    MenuFrameLook look{.borderTint = TintOr(frame.color)};
    for (std::size_t part = 0; part < kFrameParts; ++part) {
        Result<UiImage> image = images.Upload(frame.images.at(part));
        if (!image) {
            return std::unexpected(std::move(image.error()));
        }
        look.border.at(part) = *image;
    }
    if (frame.hasFillColor != 0) {
        look.fill = TintOr(frame.fillColor);
    }
    return look;
}

render::Rect FrameInterior(const MenuFrameLook& look, const render::Rect& frame) {
    return SlicesOf(look, frame).back();
}

void DrawMenuFrame(const MenuFrameLook& look, const render::Rect& frame,
                   std::vector<render::SpriteDraw>& out) {
    const std::array<render::Rect, kFrameParts + 1> slices = SlicesOf(look, frame);
    if (look.fill) {
        PushVisible({.bounds = slices.back(), .tint = *look.fill}, out);
    }
    DrawSlices(look, slices, out);
}

void DrawMenuFrameBorder(const MenuFrameLook& look, const render::Rect& frame,
                         std::vector<render::SpriteDraw>& out) {
    DrawSlices(look, SlicesOf(look, frame), out);
}

}
