#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "render/render_view.hpp"

namespace rr::app {

enum class FocusMove : std::uint8_t { Up, Down, Left, Right };

/// Picks the nearest rect whose centre lies that way, or wraps round to the farthest one the other
/// way. Stays on `current` when every other rect lines up with it.
[[nodiscard]] std::size_t NextFocus(std::span<const render::Rect> rects, std::size_t current,
                                    FocusMove move);

}
