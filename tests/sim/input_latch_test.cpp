#include "sim/input_latch.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

TEST_CASE("a tap between two ticks reaches the next frame") {
    InputLatch latch;

    latch.Press(Action::UsePowerup);
    latch.Release(Action::UsePowerup);
    const InputFrame frame = latch.TakeFrame();

    CHECK(frame.WasPressed(Action::UsePowerup));
    CHECK(frame.WasReleased(Action::UsePowerup));
    CHECK_FALSE(frame.IsHeld(Action::UsePowerup));
}

TEST_CASE("a held action stays held but is pressed only once") {
    InputLatch latch;

    latch.Press(Action::Accelerate);
    const InputFrame first = latch.TakeFrame();
    const InputFrame second = latch.TakeFrame();

    CHECK(first.WasPressed(Action::Accelerate));
    CHECK(second.IsHeld(Action::Accelerate));
    CHECK_FALSE(second.WasPressed(Action::Accelerate));
}

TEST_CASE("key repeat while held doesn't count as a new press") {
    InputLatch latch;
    latch.Press(Action::SteerLeft);
    static_cast<void>(latch.TakeFrame());

    latch.Press(Action::SteerLeft);

    CHECK_FALSE(latch.TakeFrame().WasPressed(Action::SteerLeft));
}

TEST_CASE("releasing an action that isn't held is ignored") {
    InputLatch latch;

    latch.Release(Action::Brake);

    CHECK(latch.TakeFrame() == InputFrame{});
}

}
}
