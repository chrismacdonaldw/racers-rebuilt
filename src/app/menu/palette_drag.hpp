#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class DragStep : std::uint8_t { None, Picked, Dropped, Cancelled };

enum class PaletteHold : std::uint8_t { None, Pointer, Keys };

/// An empty hand that finds a piece in it, as after an undo, holds it by keys unless it is moving.
[[nodiscard]] PaletteHold HoldAfterTick(PaletteHold hold, bool holdsPiece, bool isMoving);

struct DragTick {
    DragStep step = DragStep::None;
    int turns = 0;

    friend bool operator==(const DragTick&, const DragTick&) = default;
};

/// A left press on a tile picks its piece up until the button lets go. Each wheel step turns it.
class PaletteDrag {
public:
    /// `tile` is the palette entry under the pointer. `cancels` drops the piece without placing.
    DragTick Tick(const sim::MenuPointer& pointer, std::optional<std::size_t> tile, bool cancels);

    [[nodiscard]] std::optional<std::size_t> Entry() const { return entry_; }

private:
    std::optional<std::size_t> entry_;
    float wheel_ = 0.0F;
};

}
