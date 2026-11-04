#pragma once

#include <cstddef>
#include <optional>

#include "app/menu/palette_drag.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

struct GrabTick {
    std::optional<std::size_t> clicked;
    std::optional<std::size_t> lifted;
    DragTick drag;
};

/// A press on a placed piece selects it on release, or lifts it once the pointer travels.
class PieceGrab {
public:
    /// `piece` is the placed piece under the pointer. Once lifted, `drag` follows the piece.
    GrabTick Tick(const sim::MenuPointer& pointer, std::optional<std::size_t> piece, bool cancels);

    [[nodiscard]] bool IsBusy() const { return pressed_ || drag_.Entry(); }

    void Forget();

private:
    std::optional<std::size_t> pressed_;
    float travelled_ = 0.0F;
    PaletteDrag drag_;
};

}
