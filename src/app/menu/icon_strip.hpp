#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "app/ui_text/ui_image.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct StripPage {
    std::size_t item = 0;
    float shift = 0.0F;
};

/// A wrapping row of one-icon pages that slides a page per step, as the part set pickers do.
class IconStrip {
public:
    explicit IconStrip(Milliseconds slide) : slide_(slide) {}

    void Snap(std::size_t item, std::size_t items);

    /// False, ignoring the step, while a slide runs.
    bool Slide(int direction, std::size_t items);

    void Tick(Milliseconds elapsed);

    [[nodiscard]] bool IsSliding() const { return left_ > Milliseconds{0}; }

    /// `shift` is in page widths from the strip's box.
    [[nodiscard]] std::array<std::optional<StripPage>, 2> Pages() const;

private:
    [[nodiscard]] std::size_t Wrap(int item) const;

    Milliseconds slide_;
    std::size_t items_ = 1;
    float start_ = 0.0F;
    int end_ = 0;
    Milliseconds left_{0};
};

void DrawStripIcon(const UiImage& icon, const render::Rect& box, float shift,
                   std::vector<render::SpriteDraw>& out);

/// Strip item n shows `icons[items[n]]`.
void DrawStrip(const IconStrip& strip, std::span<const UiImage> icons,
               std::span<const std::size_t> items, const render::Rect& box,
               std::vector<render::SpriteDraw>& out);

}
