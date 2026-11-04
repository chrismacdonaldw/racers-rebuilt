#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "render/render_view.hpp"

namespace rr::app {

class ColourSwatches {
public:
    ColourSwatches(std::size_t colours, const render::Rect& area);

    [[nodiscard]] std::size_t Count() const { return count_; }
    [[nodiscard]] render::Rect SwatchOf(std::size_t colour) const;
    [[nodiscard]] std::optional<std::uint8_t> ColourAt(float x, float y) const;

    void Choose(std::uint8_t colour) { chosen_ = colour; }
    void Forget() { chosen_.reset(); }
    [[nodiscard]] std::optional<std::uint8_t> Chosen() const { return chosen_; }

private:
    std::size_t count_;
    render::Rect area_;
    float side_ = 0.0F;
    float step_ = 0.0F;
    std::optional<std::uint8_t> chosen_;
};

}
