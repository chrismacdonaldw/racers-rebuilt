#include "app/menu/workshop/bench_drag.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "sim/input_frame.hpp"

namespace rr::app {
namespace {

constexpr BenchPiece kBrick{.area = sim::BenchArea::Ground, .index = 2};
constexpr std::uint8_t kLeftHeld = 1U << static_cast<unsigned>(sim::PointerButton::Left);

sim::MenuPointer Pressed() {
    return {.pressed = true, .held = kLeftHeld};
}

sim::MenuPointer Moved(float dx) {
    return {.dx = dx, .moved = true, .held = kLeftHeld};
}

sim::MenuPointer Released() {
    return {.released = true};
}

sim::MenuPointer RightPressed() {
    return {.pressed = true, .rightPressed = true, .held = kLeftHeld};
}

TEST_CASE("a press and release on a piece clicks it") {
    BenchDrag drag;

    CHECK(drag.Tick(Pressed(), kBrick, false).step == BenchStep::None);
    CHECK(drag.IsBusy());
    const BenchDragTick tick = drag.Tick(Released(), std::nullopt, false);

    CHECK(tick.step == BenchStep::Clicked);
    CHECK(tick.piece == kBrick);
    CHECK_FALSE(drag.IsBusy());
}

TEST_CASE("a press on nothing does nothing") {
    BenchDrag drag;

    CHECK(drag.Tick(Pressed(), std::nullopt, false).step == BenchStep::None);
    CHECK_FALSE(drag.IsBusy());
}

TEST_CASE("travelling with the button down lifts the pressed piece") {
    BenchDrag drag;
    static_cast<void>(drag.Tick(Pressed(), kBrick, false));

    CHECK(drag.Tick(Moved(3.0F), std::nullopt, false).step == BenchStep::None);
    const BenchDragTick lifted = drag.Tick(Moved(4.0F), std::nullopt, false);

    CHECK(lifted.step == BenchStep::Lifted);
    CHECK(lifted.piece == kBrick);
    CHECK(drag.IsHolding());
    CHECK(drag.Tick(Released(), std::nullopt, false).step == BenchStep::Dropped);
    CHECK_FALSE(drag.IsBusy());
}

TEST_CASE("right-click or a cancel puts a held piece back") {
    BenchDrag drag;
    drag.Hold();

    CHECK(drag.Tick(RightPressed(), std::nullopt, false).step == BenchStep::Cancelled);
    CHECK_FALSE(drag.IsHolding());

    drag.Hold();
    CHECK(drag.Tick(Moved(1.0F), std::nullopt, true).step == BenchStep::Cancelled);
}

TEST_CASE("right-click before the lift forgets the press") {
    BenchDrag drag;
    static_cast<void>(drag.Tick(Pressed(), kBrick, false));

    CHECK(drag.Tick(RightPressed(), std::nullopt, false).step == BenchStep::None);
    CHECK_FALSE(drag.IsBusy());
}

TEST_CASE("a piece held without a press drops on the next release") {
    BenchDrag drag;
    drag.Hold();

    CHECK(drag.Tick(Moved(20.0F), std::nullopt, false).step == BenchStep::None);
    CHECK(drag.Tick(Released(), std::nullopt, false).step == BenchStep::Dropped);
}

}
}
