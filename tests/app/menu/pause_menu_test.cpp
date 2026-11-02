#include "app/menu/pause_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>
#include <optional>

namespace rr::app {
namespace {

using sim::Action;
using sim::MenuAction;

sim::InputFrame Pressing(Action action) {
    sim::InputFrame frame;
    frame.players[0].pressed.set(sim::PlayerInput::Index(action));
    return frame;
}

std::optional<PauseRow> Press(PauseMenu& menu, std::initializer_list<MenuAction> actions) {
    std::optional<PauseRow> chosen;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        chosen = menu.Tick(frame);
    }
    return chosen;
}

PauseMenu Opened() {
    PauseMenu menu;
    static_cast<void>(menu.Tick(Pressing(Action::Pause)));
    return menu;
}

TEST_CASE("the pause action opens the menu on Resume and closes it again") {
    PauseMenu menu;
    CHECK_FALSE(menu.IsOpen());

    CHECK_FALSE(menu.Tick(Pressing(Action::Pause)).has_value());
    CHECK(menu.IsOpen());
    CHECK(menu.Focus() == PauseRow::Resume);

    CHECK_FALSE(menu.Tick(Pressing(Action::Pause)).has_value());
    CHECK_FALSE(menu.IsOpen());
}

TEST_CASE("menu presses do nothing while the pause menu is closed") {
    PauseMenu menu;

    CHECK_FALSE(Press(menu, {MenuAction::Down, MenuAction::Confirm}).has_value());
    CHECK_FALSE(menu.IsOpen());
}

TEST_CASE("back and confirming Resume close the pause menu") {
    PauseMenu backed = Opened();
    CHECK_FALSE(Press(backed, {MenuAction::Back}).has_value());
    CHECK_FALSE(backed.IsOpen());

    PauseMenu resumed = Opened();
    CHECK_FALSE(Press(resumed, {MenuAction::Confirm}).has_value());
    CHECK_FALSE(resumed.IsOpen());
}

TEST_CASE("a held pause key's menu back repeats leave the pause menu open") {
    PauseMenu menu = Opened();
    sim::InputFrame held;
    held.players[1].held.set(sim::PlayerInput::Index(Action::Pause));
    held.menuPressed.set(sim::InputFrame::Index(MenuAction::Back));

    CHECK_FALSE(menu.Tick(held).has_value());
    CHECK(menu.IsOpen());
}

TEST_CASE("restart and quit ask are you sure first, starting on no") {
    PauseMenu restart = Opened();
    CHECK_FALSE(Press(restart, {MenuAction::Down, MenuAction::Confirm}).has_value());
    CHECK(restart.IsOpen());
    CHECK(restart.Confirming() == ConfirmRow::No);

    PauseMenu quit = Opened();
    CHECK_FALSE(Press(quit, {MenuAction::Up, MenuAction::Confirm}).has_value());
    CHECK(quit.Focus() == PauseRow::Quit);
    CHECK(quit.Confirming() == ConfirmRow::No);
}

TEST_CASE("yes acts on the asked row") {
    PauseMenu restart = Opened();
    CHECK(Press(restart, {MenuAction::Down, MenuAction::Confirm, MenuAction::Up,
                          MenuAction::Confirm}) == PauseRow::Restart);

    PauseMenu quit = Opened();
    CHECK(Press(quit, {MenuAction::Up, MenuAction::Confirm, MenuAction::Down,
                       MenuAction::Confirm}) == PauseRow::Quit);
}

TEST_CASE("no on are you sure resumes the race") {
    PauseMenu menu = Opened();
    CHECK_FALSE(
        Press(menu, {MenuAction::Up, MenuAction::Confirm, MenuAction::Confirm}).has_value());
    CHECK_FALSE(menu.IsOpen());
    CHECK_FALSE(menu.Confirming().has_value());

    static_cast<void>(menu.Tick(Pressing(Action::Pause)));
    CHECK(menu.Focus() == PauseRow::Resume);
    CHECK_FALSE(menu.Confirming().has_value());
}

TEST_CASE("back and pause leave are you sure open") {
    PauseMenu menu = Opened();
    static_cast<void>(Press(menu, {MenuAction::Up, MenuAction::Confirm, MenuAction::Back}));
    CHECK_FALSE(menu.Tick(Pressing(Action::Pause)).has_value());

    CHECK(menu.IsOpen());
    CHECK(menu.Confirming() == ConfirmRow::No);
}

TEST_CASE("the pause menu's state hash tells closed, each focused row and the confirm apart") {
    PauseMenu menu;
    CHECK(menu.StateHash() == 0);

    menu = Opened();
    const std::uint64_t onResume = menu.StateHash();
    static_cast<void>(Press(menu, {MenuAction::Down}));

    const std::uint64_t onRestart = menu.StateHash();
    static_cast<void>(Press(menu, {MenuAction::Confirm}));

    CHECK(onResume != 0);
    CHECK(onRestart != onResume);
    CHECK(menu.StateHash() != onRestart);
}

}
}
