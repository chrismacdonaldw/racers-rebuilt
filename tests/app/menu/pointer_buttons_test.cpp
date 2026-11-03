#include "app/menu/pointer_buttons.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>
#include <vector>

namespace rr::app {
namespace {

const std::array kTargets{
    PointerTarget{.area = {.x = 10.0F, .y = 10.0F, .width = 50.0F, .height = 20.0F}, .control = 3},
    PointerTarget{.area = {.x = 10.0F, .y = 40.0F, .width = 50.0F, .height = 20.0F},
                  .control = 5,
                  .click = sim::MenuAction::Right}};

sim::MenuPointer At(float x, float y) {
    return {.x = x, .y = y, .isOver = true};
}

sim::MenuPointer Moved(float x, float y) {
    sim::MenuPointer pointer = At(x, y);
    pointer.moved = true;
    return pointer;
}

sim::MenuPointer Pressed(float x, float y) {
    sim::MenuPointer pointer = At(x, y);
    pointer.pressed = true;
    return pointer;
}

sim::MenuPointer Released(float x, float y) {
    sim::MenuPointer pointer = At(x, y);
    pointer.released = true;
    return pointer;
}

TEST_CASE("moving over a target hovers its control, resting doesn't") {
    PointerButtons buttons;

    CHECK(buttons.Tick(Moved(20.0F, 45.0F), kTargets).hovered == 5);
    CHECK_FALSE(buttons.Tick(At(20.0F, 45.0F), kTargets).hovered.has_value());
    CHECK_FALSE(buttons.Tick(Moved(100.0F, 100.0F), kTargets).hovered.has_value());
}

TEST_CASE("a press and release over one target clicks it with its action") {
    PointerButtons buttons;
    static_cast<void>(buttons.Tick(Pressed(20.0F, 45.0F), kTargets));
    const PointerChoice choice = buttons.Tick(Released(25.0F, 50.0F), kTargets);

    CHECK(choice.clicked == 5);
    CHECK(choice.Focus() == 5);
    CHECK(choice.Clicks(sim::MenuAction::Right));
}

TEST_CASE("releasing off the pressed target doesn't click") {
    PointerButtons buttons;
    static_cast<void>(buttons.Tick(Pressed(20.0F, 15.0F), kTargets));

    CHECK_FALSE(buttons.Tick(Released(20.0F, 45.0F), kTargets).clicked.has_value());
}

TEST_CASE("a pointer off the canvas hits nothing") {
    sim::MenuPointer pointer = At(20.0F, 15.0F);
    pointer.isOver = false;

    CHECK_FALSE(HitTarget(kTargets, pointer).has_value());
    CHECK(HitTarget(kTargets, At(20.0F, 15.0F)) == 0);
}

TEST_CASE("a selector's arrows step it and its middle only focuses it") {
    std::vector<PointerTarget> targets;
    AddSelectorTargets({.bounds = {.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 20.0F},
                        .previous = {.x = 0.0F, .y = 0.0F, .width = 10.0F, .height = 20.0F},
                        .next = {.x = 90.0F, .y = 0.0F, .width = 10.0F, .height = 20.0F}},
                       7, targets);

    REQUIRE(targets.size() == 3);
    CHECK(targets[HitTarget(targets, At(5.0F, 5.0F)).value_or(9)].click == sim::MenuAction::Left);
    CHECK(targets[HitTarget(targets, At(95.0F, 5.0F)).value_or(9)].click == sim::MenuAction::Right);
    CHECK_FALSE(targets[HitTarget(targets, At(50.0F, 5.0F)).value_or(9)].click.has_value());
}

TEST_CASE("a click presses its action on the tick's input") {
    const sim::InputFrame input =
        WithClick({}, PointerChoice{.clicked = 2, .click = sim::MenuAction::Confirm});

    CHECK(input.WasPressed(sim::MenuAction::Confirm));
    CHECK_FALSE(WithClick({}, PointerChoice{.hovered = 2}).menuPressed.any());
}

}
}
