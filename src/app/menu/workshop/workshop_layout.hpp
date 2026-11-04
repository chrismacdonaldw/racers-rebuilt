#pragma once

#include <array>
#include <cstddef>

#include "app/menu/piece_palette.hpp"
#include "render/render_view.hpp"

namespace rr::app {

inline constexpr std::size_t kWorkshopTools = 5;
inline constexpr std::size_t kTabColumns = 4;
inline constexpr std::size_t kTabRows = 4;

/// In UI units. The drawer runs down the window's left edge and the scene fills the rest.
struct WorkshopLayout {
    render::Rect scene;
    render::Rect drawer;
    render::Rect tabs;
    PaletteGrid tiles;
    render::Rect counts;
    render::Rect done;
    std::array<render::Rect, kWorkshopTools> tools{};
    render::Rect colours;
    render::Rect hint;
};

/// `window` is the window in UI units, as `Renderer::UiWindow` gives it. The tab rows grow with
/// `tabs`, up to `kTabRows`.
[[nodiscard]] WorkshopLayout LayOutWorkshop(const render::Rect& window, std::size_t tabs);

/// Tabs fill `kTabColumns` across, then the next row.
[[nodiscard]] render::Rect TabOf(const WorkshopLayout& layout, std::size_t tab);

[[nodiscard]] bool Contains(const render::Rect& area, float x, float y);

}
