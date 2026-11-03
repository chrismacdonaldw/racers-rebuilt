#include "app/menu/car_build_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

sim::InputFrame Pressed(sim::MenuAction action) {
    sim::InputFrame input;
    input.menuPressed.set(sim::InputFrame::Index(action));
    return input;
}

BuildRequest Press(CarBuildMenu& menu, std::initializer_list<sim::MenuAction> actions) {
    BuildRequest last;
    for (const sim::MenuAction action : actions) {
        last = menu.Tick(Pressed(action));
    }
    return last;
}

}

TEST_CASE("the builder keys follow the original's") {
    CHECK(BuildKeyRequest(SDLK_UP) == BuildRequest{.command = BuildCommand::Move});
    CHECK(BuildKeyRequest(SDLK_KP_3) ==
          BuildRequest{.command = BuildCommand::Move, .direction = 3});
    CHECK(BuildKeyRequest(SDLK_KP_5) == BuildRequest{.command = BuildCommand::Rotate});
    CHECK(BuildKeyRequest(SDLK_KP_PLUS) == BuildRequest{.command = BuildCommand::Place});
    CHECK(BuildKeyRequest(SDLK_MINUS) == BuildRequest{.command = BuildCommand::Undo});
    CHECK(BuildKeyRequest(SDLK_PAGEUP) == BuildRequest{.command = BuildCommand::NextSet});
    CHECK(BuildKeyRequest(SDLK_DELETE) == BuildRequest{.command = BuildCommand::PreviousPiece});
    CHECK(BuildKeyRequest(SDLK_A) == BuildRequest{.command = BuildCommand::TurnLeft});
    CHECK(BuildKeyRequest(SDLK_X) == BuildRequest{.command = BuildCommand::TiltDown});
    CHECK_FALSE(BuildKeyRequest(SDLK_B).has_value());
}

TEST_CASE("left and right step the focused carousel") {
    CarBuildMenu menu;

    CHECK(Press(menu, {sim::MenuAction::Right}).command == BuildCommand::NextSet);
    CHECK(Press(menu, {sim::MenuAction::Down, sim::MenuAction::Left}).command ==
          BuildCommand::PreviousPiece);
    CHECK(menu.Focus() == BuildRow::Pieces);
}

TEST_CASE("the move pad holds the directions until confirm") {
    CarBuildMenu menu;
    Press(menu, {sim::MenuAction::Down, sim::MenuAction::Down, sim::MenuAction::Confirm});
    REQUIRE(menu.Held() == BuildPad::Move);

    CHECK(Press(menu, {sim::MenuAction::Down}) ==
          BuildRequest{.command = BuildCommand::Move, .direction = 4});
    CHECK(Press(menu, {sim::MenuAction::Left}) ==
          BuildRequest{.command = BuildCommand::Move, .direction = 6});
    CHECK(Press(menu, {sim::MenuAction::Back}).command == BuildCommand::None);
    CHECK_FALSE(menu.Held().has_value());
    CHECK(menu.Focus() == BuildRow::Move);
}

TEST_CASE("the view pad turns and tilts the view until confirm") {
    CarBuildMenu menu;
    Press(menu, {sim::MenuAction::Down, sim::MenuAction::Down, sim::MenuAction::Down,
                 sim::MenuAction::Confirm});
    REQUIRE(menu.Held() == BuildPad::View);

    CHECK(Press(menu, {sim::MenuAction::Up}).command == BuildCommand::TiltUp);
    CHECK(Press(menu, {sim::MenuAction::Left}).command == BuildCommand::TurnLeft);
    CHECK(Press(menu, {sim::MenuAction::Confirm}).command == BuildCommand::None);
    CHECK_FALSE(menu.Held().has_value());
}

TEST_CASE("the buttons act on confirm and back is done") {
    CarBuildMenu menu;

    CHECK(Press(menu, {sim::MenuAction::Down, sim::MenuAction::Down, sim::MenuAction::Down,
                       sim::MenuAction::Down, sim::MenuAction::Confirm})
              .command == BuildCommand::Rotate);
    CHECK(Press(menu, {sim::MenuAction::Down, sim::MenuAction::Confirm}).command ==
          BuildCommand::Place);
    CHECK(Press(menu, {sim::MenuAction::Down, sim::MenuAction::Confirm}).command ==
          BuildCommand::Undo);
    CHECK(Press(menu, {sim::MenuAction::Down, sim::MenuAction::Down}).command ==
          BuildCommand::None);
    CHECK(menu.Focus() == BuildRow::Done);
    CHECK(Press(menu, {sim::MenuAction::Back}).command == BuildCommand::Done);
}

}
