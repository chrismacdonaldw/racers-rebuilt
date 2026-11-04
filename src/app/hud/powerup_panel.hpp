#pragma once

#include <span>
#include <vector>

#include "app/ui_text/ui_image.hpp"
#include "render/render_view.hpp"
#include "render/ui_layout.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Appends nothing while `racer` holds no power-up. `images` is `LEGOIMGS.IDB` in list order, and
/// the item shown is the original icon of what `slots` fires.
void LayoutPowerupPanel(const sim::RacerSnapshot& racer, const sim::PowerupSlots& slots,
                        std::span<const UiImage> images, std::vector<render::SpriteDraw>& out,
                        float bottom = render::kUiHeight);

}
