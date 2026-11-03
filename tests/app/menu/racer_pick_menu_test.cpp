#include "app/menu/racer_pick_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

using sim::MenuAction;

RacerPickRequest Press(RacerPickMenu& menu, std::initializer_list<MenuAction> actions) {
    RacerPickRequest request = RacerPickRequest::None;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        request = menu.Tick(frame);
    }
    return request;
}

TEST_CASE("the racer pick opens on START with the selected racer showing") {
    RacerPickMenu menu(3, 2);

    CHECK(menu.Focus() == RacerPickRow::Start);
    CHECK(menu.Racer() == 2);
    CHECK(Press(menu, {MenuAction::Confirm}) == RacerPickRequest::Start);
}

TEST_CASE("the racer row steps through the racers both ways, wrapping") {
    RacerPickMenu menu(3, 0);

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Up, MenuAction::Left}) ==
          RacerPickRequest::Step);
    CHECK(menu.Focus() == RacerPickRow::Racer);
    CHECK(menu.Racer() == 2);
    CHECK(Press(menu, {MenuAction::Right, MenuAction::Right}) == RacerPickRequest::Step);
    CHECK(menu.Racer() == 1);
}

TEST_CASE("left and right only step the racer while its row has focus") {
    RacerPickMenu menu(3, 0);

    CHECK(Press(menu, {MenuAction::Right}) == RacerPickRequest::None);
    CHECK(menu.Racer() == 0);
}

TEST_CASE("CONFIG and BACK ask for their screens, and the back key leaves from any row") {
    RacerPickMenu menu(1, 0);

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == RacerPickRequest::Config);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Confirm}) ==
          RacerPickRequest::Leave);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Back}) == RacerPickRequest::Leave);
}

TEST_CASE("with no racers the pick can't start or step, and an out-of-range pick starts first") {
    RacerPickMenu empty(0, 0);
    RacerPickMenu lone(1, 4);

    CHECK(Press(empty, {MenuAction::Confirm}) == RacerPickRequest::None);
    CHECK(Press(empty, {MenuAction::Up, MenuAction::Up, MenuAction::Right}) ==
          RacerPickRequest::None);
    CHECK(lone.Racer() == 0);
    CHECK(Press(lone, {MenuAction::Up, MenuAction::Up, MenuAction::Right}) ==
          RacerPickRequest::None);
}

}
}
