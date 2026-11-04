#include "app/menu/piece_grab.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

constexpr sim::MenuPointer kPress{.isOver = true, .pressed = true};
constexpr sim::MenuPointer kRelease{.isOver = true, .released = true};

sim::MenuPointer Moved(float dx) {
    return {.dx = dx, .isOver = true, .moved = true};
}

TEST_CASE("a press and release on a placed piece clicks it") {
    PieceGrab grab;
    CHECK_FALSE(grab.Tick(kPress, 3, false).clicked.has_value());
    CHECK(grab.IsBusy());
    CHECK(grab.Tick(kRelease, std::nullopt, false).clicked == 3);
    CHECK_FALSE(grab.IsBusy());

    CHECK(grab.Tick({.isOver = true, .pressed = true, .released = true}, 2, false).clicked == 2);
    CHECK_FALSE(grab.Tick(kPress, std::nullopt, false).clicked.has_value());
    CHECK_FALSE(grab.IsBusy());
}

TEST_CASE("a pressed piece lifts once the pointer travels and drops on release") {
    PieceGrab grab;
    static_cast<void>(grab.Tick(kPress, 4, false));
    CHECK_FALSE(grab.Tick(Moved(4.0F), std::nullopt, false).lifted.has_value());
    CHECK(grab.Tick(Moved(-3.0F), std::nullopt, false).lifted == 4);
    CHECK(grab.IsBusy());
    CHECK(grab.Tick({.wheel = 1.0F, .isOver = true}, std::nullopt, false).drag.turns == 1);
    CHECK(grab.Tick(kRelease, std::nullopt, false).drag.step == DragStep::Dropped);
    CHECK_FALSE(grab.IsBusy());
}

TEST_CASE("a right press or a cancel lets go of a pressed or lifted piece") {
    PieceGrab grab;
    static_cast<void>(grab.Tick(kPress, 1, false));
    CHECK(grab.Tick({.isOver = true, .rightPressed = true}, std::nullopt, false).clicked ==
          std::nullopt);
    CHECK_FALSE(grab.IsBusy());

    static_cast<void>(grab.Tick(kPress, 1, false));
    static_cast<void>(grab.Tick(Moved(10.0F), std::nullopt, false));
    CHECK(grab.Tick({}, std::nullopt, true).drag.step == DragStep::Cancelled);
    CHECK_FALSE(grab.IsBusy());
}

}
}
