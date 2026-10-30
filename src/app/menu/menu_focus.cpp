#include "app/menu/menu_focus.hpp"

#include <cmath>
#include <limits>
#include <optional>

namespace rr::app {
namespace {

constexpr float kSidewaysWeight = 2.0F;

struct Offset {
    float along = 0.0F;
    float sideways = 0.0F;
};

Offset OffsetBetween(const render::Rect& from, const render::Rect& to, FocusMove move) {
    const float dx = (to.x + (to.width / 2.0F)) - (from.x + (from.width / 2.0F));
    const float dy = (to.y + (to.height / 2.0F)) - (from.y + (from.height / 2.0F));
    switch (move) {
        case FocusMove::Up:
            return {.along = -dy, .sideways = std::abs(dx)};
        case FocusMove::Down:
            return {.along = dy, .sideways = std::abs(dx)};
        case FocusMove::Left:
            return {.along = -dx, .sideways = std::abs(dy)};
        case FocusMove::Right:
            return {.along = dx, .sideways = std::abs(dy)};
    }
    return {};
}

struct Best {
    std::optional<std::size_t> index;
    float score = std::numeric_limits<float>::max();

    void Offer(std::size_t candidate, float candidateScore) {
        if (candidateScore < score) {
            index = candidate;
            score = candidateScore;
        }
    }
};

}

std::size_t NextFocus(std::span<const render::Rect> rects, std::size_t current, FocusMove move) {
    Best ahead;
    Best wrapped;
    for (std::size_t i = 0; i < rects.size(); ++i) {
        if (i == current) {
            continue;
        }
        const Offset offset = OffsetBetween(rects[current], rects[i], move);
        if (offset.along > 0.0F) {
            ahead.Offer(i, offset.along + (kSidewaysWeight * offset.sideways));
        } else if (offset.along < 0.0F) {
            wrapped.Offer(i, offset.along + (kSidewaysWeight * offset.sideways));
        }
    }
    return ahead.index.value_or(wrapped.index.value_or(current));
}

}
