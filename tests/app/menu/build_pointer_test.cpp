#include "app/menu/build_pointer.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

constexpr render::Rect kMoveSpot{.x = 10.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F};
constexpr render::Rect kViewSpot{.x = 50.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F};
constexpr render::Rect kPlaceButton{.x = 100.0F, .y = 100.0F, .width = 40.0F, .height = 20.0F};

CarBuildLook PadsLook() {
    CarBuildLook look;
    look.pads.at(0).spots = {{.id = 3, .area = kMoveSpot}};
    look.pads.at(1).spots = {{.id = 9, .area = kViewSpot}};
    look.buttons.at(3).bounds = kPlaceButton;
    return look;
}

sim::InputFrame Over(const render::Rect& area) {
    sim::InputFrame input;
    input.pointer = {.x = area.x + 1.0F, .y = area.y + 1.0F, .isOver = true};
    return input;
}

sim::InputFrame Pressed(const render::Rect& area) {
    sim::InputFrame input = Over(area);
    input.pointer.pressed = true;
    return input;
}

int Hold(BuildPointer& pointer, const CarBuildLook& look, CarBuildMenu& menu, int ticks) {
    int fired = 0;
    for (int tick = 0; tick < ticks; ++tick) {
        if (pointer.Tick(Over(kMoveSpot), look, menu).pad.command != BuildCommand::None) {
            ++fired;
        }
    }
    return fired;
}

}

TEST_CASE("the pads' spot ids follow the original's") {
    CHECK(PadRequest(BuildPad::Move, 1) ==
          BuildRequest{.command = BuildCommand::Move, .direction = 0});
    CHECK(PadRequest(BuildPad::Move, 8) ==
          BuildRequest{.command = BuildCommand::Move, .direction = 7});
    CHECK(PadRequest(BuildPad::Move, 0).command == BuildCommand::None);
    CHECK(PadRequest(BuildPad::View, 1).command == BuildCommand::TiltUp);
    CHECK(PadRequest(BuildPad::View, 3).command == BuildCommand::TurnRight);
    CHECK(PadRequest(BuildPad::View, 5).command == BuildCommand::TiltDown);
    CHECK(PadRequest(BuildPad::View, 7).command == BuildCommand::TurnLeft);
    CHECK(PadRequest(BuildPad::View, 9).command == BuildCommand::ResetView);
    CHECK(PadRequest(BuildPad::View, 2).command == BuildCommand::None);
}

TEST_CASE("a held pad fires on the press, after 500 ms and then every 250 ms") {
    const CarBuildLook look = PadsLook();
    CarBuildMenu menu;
    BuildPointer pointer;

    CHECK(pointer.Tick(Pressed(kMoveSpot), look, menu).pad ==
          BuildRequest{.command = BuildCommand::Move, .direction = 2});
    CHECK(Hold(pointer, look, menu, 41) == 0);
    CHECK(Hold(pointer, look, menu, 1) == 1);
    CHECK(Hold(pointer, look, menu, 20) == 0);
    CHECK(Hold(pointer, look, menu, 1) == 1);

    sim::InputFrame released = Over(kMoveSpot);
    released.pointer.released = true;
    static_cast<void>(pointer.Tick(released, look, menu));
    CHECK(Hold(pointer, look, menu, 100) == 0);
}

TEST_CASE("a held pad repeats only on its own spots") {
    const CarBuildLook look = PadsLook();
    CarBuildMenu menu;
    BuildPointer pointer;

    static_cast<void>(pointer.Tick(Pressed(kViewSpot), look, menu));
    for (int tick = 0; tick < 100; ++tick) {
        CHECK(pointer.Tick(Over(kMoveSpot), look, menu).pad.command == BuildCommand::None);
    }
}

TEST_CASE("a release while the builder is busy lets go of the pad") {
    const CarBuildLook look = PadsLook();
    CarBuildMenu menu;
    BuildPointer pointer;

    static_cast<void>(pointer.Tick(Pressed(kMoveSpot), look, menu));
    sim::InputFrame released = Over(kMoveSpot);
    released.pointer.released = true;
    pointer.TickIdle(released.pointer);
    CHECK(Hold(pointer, look, menu, 100) == 0);
}

TEST_CASE("the view pad's reset spot resets the view") {
    const CarBuildLook look = PadsLook();
    CarBuildMenu menu;
    BuildPointer pointer;

    CHECK(pointer.Tick(Pressed(kViewSpot), look, menu).pad.command == BuildCommand::ResetView);
}

TEST_CASE("a pad held from the keyboard ignores the pointer") {
    const CarBuildLook look = PadsLook();
    CarBuildMenu menu;
    BuildPointer pointer;
    sim::InputFrame keys;
    for (const sim::MenuAction action :
         {sim::MenuAction::Down, sim::MenuAction::Down, sim::MenuAction::Confirm}) {
        keys.menuPressed.reset();
        keys.menuPressed.set(sim::InputFrame::Index(action));
        static_cast<void>(menu.Tick(keys));
    }
    REQUIRE(menu.Held() == BuildPad::Move);

    CHECK(pointer.Tick(Pressed(kMoveSpot), look, menu).pad.command == BuildCommand::None);
    sim::InputFrame moved = Over(kPlaceButton);
    moved.pointer.moved = true;
    static_cast<void>(pointer.Tick(moved, look, menu));
    CHECK(menu.Focus() == BuildRow::Move);
}

TEST_CASE("clicking a button focuses and confirms it") {
    const CarBuildLook look = PadsLook();
    CarBuildMenu menu;
    BuildPointer pointer;

    sim::InputFrame moved = Over(kPlaceButton);
    moved.pointer.moved = true;
    static_cast<void>(pointer.Tick(moved, look, menu));
    CHECK(menu.Focus() == BuildRow::Place);

    static_cast<void>(pointer.Tick(Pressed(kPlaceButton), look, menu));
    sim::InputFrame released = Over(kPlaceButton);
    released.pointer.released = true;
    const BuildPointing pointing = pointer.Tick(released, look, menu);
    CHECK(menu.Tick(pointing.input).command == BuildCommand::Place);
}

}
