#include "app/menu/palette_drag.hpp"

#include <cmath>

namespace rr::app {

PaletteHold HoldAfterTick(PaletteHold hold, bool holdsPiece, bool isMoving) {
    return hold == PaletteHold::None && holdsPiece && !isMoving ? PaletteHold::Keys : hold;
}

void PaletteDrag::Hold(std::size_t entry) {
    entry_ = entry;
    wheel_ = 0.0F;
}

DragTick PaletteDrag::Tick(const sim::MenuPointer& pointer, std::optional<std::size_t> tile,
                           bool cancels) {
    if (!entry_) {
        if (pointer.pressed && !pointer.rightPressed && tile) {
            entry_ = tile;
            wheel_ = 0.0F;
            return {.step = DragStep::Picked};
        }
        return {};
    }
    if (cancels || pointer.rightPressed) {
        entry_.reset();
        return {.step = DragStep::Cancelled};
    }
    if (pointer.released) {
        entry_.reset();
        return {.step = DragStep::Dropped};
    }
    wheel_ += pointer.wheel;
    const float turns = std::trunc(wheel_);
    wheel_ -= turns;
    return {.turns = static_cast<int>(turns)};
}

}
