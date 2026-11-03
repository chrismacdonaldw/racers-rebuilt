#include "app/menu/icon_strip.hpp"

#include <algorithm>
#include <cmath>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"

namespace rr::app {

void IconStrip::Snap(std::size_t item, std::size_t items) {
    items_ = std::max<std::size_t>(items, 1);
    end_ = static_cast<int>(item % items_);
    start_ = static_cast<float>(end_);
    left_ = Milliseconds{0};
}

bool IconStrip::Slide(int direction, std::size_t items) {
    if (IsSliding()) {
        return false;
    }
    items_ = std::max<std::size_t>(items, 1);
    start_ = static_cast<float>(end_);
    end_ += direction;
    left_ = slide_;
    Tick(Milliseconds{0});
    return true;
}

void IconStrip::Tick(Milliseconds elapsed) {
    left_ = std::max(left_ - elapsed, Milliseconds{0});
    if (!IsSliding()) {
        end_ = static_cast<int>(Wrap(end_));
        start_ = static_cast<float>(end_);
    }
}

std::array<std::optional<StripPage>, 2> IconStrip::Pages() const {
    auto position = static_cast<float>(end_);
    if (IsSliding()) {
        const float left = static_cast<float>(left_.count()) / static_cast<float>(slide_.count());
        position += (start_ - position) * left;
    }
    const float base = std::floor(position);
    const float within = position - base;
    const int first = static_cast<int>(base);
    std::array<std::optional<StripPage>, 2> pages{StripPage{.item = Wrap(first), .shift = -within}};
    if (within > 0.0F) {
        pages.back() = StripPage{.item = Wrap(first + 1), .shift = 1.0F - within};
    }
    return pages;
}

std::size_t IconStrip::Wrap(int item) const {
    const auto count = static_cast<int>(items_);
    return static_cast<std::size_t>(((item % count) + count) % count);
}

void DrawStripIcon(const UiImage& icon, const render::Rect& box, float shift,
                   std::vector<render::SpriteDraw>& out) {
    const render::Rect sized = SizedAt({}, icon);
    const render::Rect bounds{.x = box.x + (shift * box.width) + ((box.width - sized.width) / 2),
                              .y = box.y + ((box.height - sized.height) / 2),
                              .width = sized.width,
                              .height = sized.height};
    const float left = std::max(bounds.x, box.x);
    const float right = std::min(bounds.x + bounds.width, box.x + box.width);
    if (right <= left) {
        return;
    }
    const render::Rect shown{
        .x = left, .y = bounds.y, .width = right - left, .height = bounds.height};
    out.push_back({.texture = icon.texture, .bounds = shown, .uv = PieceUv(bounds, shown)});
}

void DrawStrip(const IconStrip& strip, std::span<const UiImage> icons,
               std::span<const std::size_t> items, const render::Rect& box,
               std::vector<render::SpriteDraw>& out) {
    for (const std::optional<StripPage>& page : strip.Pages()) {
        if (page && page->item < items.size() && items[page->item] < icons.size()) {
            DrawStripIcon(icons[items[page->item]], box, page->shift, out);
        }
    }
}

}
