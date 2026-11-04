#pragma once

#include <cstdint>
#include <optional>

#include "app/menu/workshop/bench_pick.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class BenchStep : std::uint8_t { None, Clicked, Lifted, Dropped, Cancelled };

struct BenchDragTick {
    BenchStep step = BenchStep::None;
    std::optional<BenchPiece> piece;
};

/// A left press on a piece selects it on release, or lifts it once the pointer travels. A held
/// piece drops on the next release, and right-click or `cancels` puts it back.
class BenchDrag {
public:
    /// `under` is the piece under the pointer, read only before anything is held.
    BenchDragTick Tick(const sim::MenuPointer& pointer, std::optional<BenchPiece> under,
                       bool cancels);

    /// Holds a piece that didn't come from a press, such as one from the drawer.
    void Hold();

    [[nodiscard]] bool IsHolding() const { return isHolding_; }
    [[nodiscard]] bool IsBusy() const { return isHolding_ || pressed_.has_value(); }

    void Forget();

private:
    std::optional<BenchPiece> pressed_;
    float travelled_ = 0.0F;
    bool isHolding_ = false;
};

}
