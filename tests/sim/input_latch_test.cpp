#include "sim/input_latch.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

TEST_CASE("a tap between two ticks reaches the next frame") {
    InputLatch latch;

    latch.Press(0, Action::UsePowerup);
    latch.Release(0, Action::UsePowerup);
    const InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].WasPressed(Action::UsePowerup));
    CHECK(frame.players[0].WasReleased(Action::UsePowerup));
    CHECK_FALSE(frame.players[0].IsHeld(Action::UsePowerup));
}

TEST_CASE("a held action stays held but is pressed only once") {
    InputLatch latch;

    latch.Press(0, Action::Accelerate);
    const InputFrame first = latch.TakeFrame();
    const InputFrame second = latch.TakeFrame();

    CHECK(first.players[0].WasPressed(Action::Accelerate));
    CHECK(second.players[0].IsHeld(Action::Accelerate));
    CHECK_FALSE(second.players[0].WasPressed(Action::Accelerate));
}

TEST_CASE("key repeat while held doesn't count as a new press") {
    InputLatch latch;
    latch.Press(0, Action::SteerLeft);
    static_cast<void>(latch.TakeFrame());

    latch.Press(0, Action::SteerLeft);

    CHECK_FALSE(latch.TakeFrame().players[0].WasPressed(Action::SteerLeft));
}

TEST_CASE("the steer axis holds its last value across frames") {
    InputLatch latch;

    latch.SetSteerLeftAxis(0, -40);

    CHECK(latch.TakeFrame().players[0].steerLeftAxis == -40);
    CHECK(latch.TakeFrame().players[0].steerLeftAxis == -40);
}

TEST_CASE("releasing everything releases only what was held and centres the axis") {
    InputLatch latch;
    latch.Press(0, Action::Accelerate);
    latch.SetSteerLeftAxis(0, 90);
    static_cast<void>(latch.TakeFrame());

    latch.ReleaseAll();
    const InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].WasReleased(Action::Accelerate));
    CHECK_FALSE(frame.players[0].IsHeld(Action::Accelerate));
    CHECK_FALSE(frame.players[0].WasReleased(Action::Brake));
    CHECK(frame.players[0].steerLeftAxis == 0);
}

TEST_CASE("releasing an action that isn't held is ignored") {
    InputLatch latch;

    latch.Release(0, Action::Brake);

    CHECK(latch.TakeFrame() == InputFrame{});
}

TEST_CASE("menu presses last one frame and each repeat counts") {
    InputLatch latch;

    latch.Press(MenuAction::Down);
    const InputFrame first = latch.TakeFrame();
    latch.Press(MenuAction::Down);
    const InputFrame repeat = latch.TakeFrame();

    CHECK(first.WasPressed(MenuAction::Down));
    CHECK(repeat.WasPressed(MenuAction::Down));
    CHECK_FALSE(latch.TakeFrame().WasPressed(MenuAction::Down));
}

}
}
