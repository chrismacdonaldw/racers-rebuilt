#pragma once

#include <span>
#include <vector>

#include "app/ui_text/ui_image.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Appends nothing while `racer` holds no power-up. `images` is `LEGOIMGS.IDB` in list order.
void LayoutPowerupPanel(const sim::RacerSnapshot& racer, std::span<const UiImage> images,
                        std::vector<render::SpriteDraw>& out);

}
