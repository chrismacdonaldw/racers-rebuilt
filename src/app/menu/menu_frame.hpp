#pragma once

#include <array>
#include <optional>
#include <vector>

#include "app/menu/dialog_layout.hpp"
#include "app/menu/menu_page.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct MenuFrameLook {
    std::array<UiImage, kFrameParts> border;
    render::Color borderTint;
    std::optional<render::Color> fill;
};

[[nodiscard]] Result<MenuFrameLook> LoadMenuFrame(MenuImageCache& images,
                                                  const assets::FrameElement& frame);

[[nodiscard]] render::Rect FrameInterior(const MenuFrameLook& look, const render::Rect& frame);

void DrawMenuFrame(const MenuFrameLook& look, const render::Rect& frame,
                   std::vector<render::SpriteDraw>& out);

/// Leaves the fill out, so a scene drawn inside shows through.
void DrawMenuFrameBorder(const MenuFrameLook& look, const render::Rect& frame,
                         std::vector<render::SpriteDraw>& out);

}
