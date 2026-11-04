#include "app/menu/palette_drag.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

sim::MenuPointer Press() {
    return {.isOver = true, .pressed = true};
}

sim::MenuPointer Wheel(float steps) {
    return {.wheel = steps, .isOver = true};
}

TEST_CASE("a left press on a tile picks it up and a release drops it") {
    PaletteDrag drag;
    CHECK(drag.Tick(Press(), std::nullopt, false).step == DragStep::None);
    CHECK_FALSE(drag.Entry().has_value());

    CHECK(drag.Tick(Press(), 3, false).step == DragStep::Picked);
    CHECK(drag.Entry() == 3);
    CHECK(drag.Tick({.x = 300.0F, .isOver = true, .moved = true}, std::nullopt, false) ==
          DragTick{});
    CHECK(drag.Entry() == 3);

    CHECK(drag.Tick({.isOver = true, .released = true}, std::nullopt, false).step ==
          DragStep::Dropped);
    CHECK_FALSE(drag.Entry().has_value());
}

TEST_CASE("wheel steps turn the held piece once per whole step") {
    PaletteDrag drag;
    static_cast<void>(drag.Tick(Press(), 0, false));

    CHECK(drag.Tick(Wheel(0.5F), std::nullopt, false).turns == 0);
    CHECK(drag.Tick(Wheel(0.75F), std::nullopt, false).turns == 1);
    CHECK(drag.Tick(Wheel(-3.0F), std::nullopt, false).turns == -2);
}

TEST_CASE("a dropped piece leaves the hand empty and an undone one is held by keys") {
    CHECK(HoldAfterTick(PaletteHold::None, true, true) == PaletteHold::None);
    CHECK(HoldAfterTick(PaletteHold::None, true, false) == PaletteHold::Keys);
    CHECK(HoldAfterTick(PaletteHold::None, false, false) == PaletteHold::None);
    CHECK(HoldAfterTick(PaletteHold::Pointer, true, false) == PaletteHold::Pointer);
}

TEST_CASE("a right press or a cancel drops the piece without placing it") {
    PaletteDrag drag;
    static_cast<void>(drag.Tick(Press(), 1, false));
    CHECK(drag.Tick({.isOver = true, .rightPressed = true}, std::nullopt, false).step ==
          DragStep::Cancelled);

    static_cast<void>(drag.Tick(Press(), 1, false));
    CHECK(drag.Tick({}, std::nullopt, true).step == DragStep::Cancelled);
    CHECK_FALSE(drag.Entry().has_value());
    CHECK(drag.Tick({.isOver = true, .pressed = true, .rightPressed = true}, 1, false).step ==
          DragStep::None);
}

}
}
