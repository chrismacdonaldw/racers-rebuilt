#include "app/menu/controls_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

using sim::MenuAction;

ControlsRequest Press(ControlsMenu& menu, std::initializer_list<MenuAction> actions) {
    ControlsRequest request = ControlsRequest::None;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        request = menu.Tick(frame);
    }
    return request;
}

using enum InputSet;

TEST_CASE("the controls screen opens on the given device with TURN LEFT focused") {
    ControlsMenu menu;
    menu.SetEntries({Pad1, Keys1, Keys2, Keys3}, Keys2);

    CHECK(menu.Set() == Keys2);
    CHECK(menu.FocusedRow() == 0);
}

TEST_CASE("the device picker sits above the rows and stops at either end") {
    ControlsMenu menu;
    menu.SetEntries({Pad1, Keys1, Keys2, Keys3}, Keys1);

    static_cast<void>(
        Press(menu, {MenuAction::Up, MenuAction::Right, MenuAction::Right, MenuAction::Right}));
    CHECK(menu.IsDeviceFocused());
    CHECK(menu.Set() == Keys3);

    static_cast<void>(
        Press(menu, {MenuAction::Left, MenuAction::Left, MenuAction::Left, MenuAction::Left}));
    CHECK(menu.Set() == Pad1);
}

TEST_CASE("a pad's steering rows are disabled and focus skips them") {
    ControlsMenu menu;
    menu.SetEntries({Pad1, Keys1, Keys2, Keys3}, Pad1);

    CHECK_FALSE(menu.IsRowEnabled(0));
    CHECK_FALSE(menu.IsRowEnabled(1));
    CHECK(menu.IsRowEnabled(2));
    CHECK(menu.FocusedRow() == 2);
    static_cast<void>(Press(menu, {MenuAction::Up}));
    CHECK(menu.IsDeviceFocused());
    static_cast<void>(Press(menu, {MenuAction::Down}));
    CHECK(menu.FocusedRow() == 2);
}

TEST_CASE("pointing at a disabled row or while listening leaves the focus") {
    ControlsMenu menu;
    menu.SetEntries({Pad1, Keys1, Keys2, Keys3}, Pad1);

    CHECK_FALSE(menu.FocusOn(1));
    CHECK(menu.FocusedRow() == 2);
    CHECK(Press(menu, {MenuAction::Confirm}) == ControlsRequest::Listen);
    CHECK_FALSE(menu.FocusOn(0));
    CHECK(menu.ListeningRow() == 2);
}

TEST_CASE("a device that goes away leaves the picker on the first") {
    ControlsMenu menu;
    menu.SetEntries({Pad1, Pad2, Keys1, Keys2, Keys3}, Pad2);

    menu.SetEntries({Pad1, Keys1, Keys2, Keys3}, menu.Set());

    CHECK(menu.Set() == Pad1);
    CHECK(menu.Entry() == 0);
}

TEST_CASE("choosing a row listens, ignoring menu actions until it stops") {
    ControlsMenu menu;

    CHECK(Press(menu, {MenuAction::Confirm}) == ControlsRequest::Listen);
    CHECK(menu.ListeningRow() == 0);
    CHECK(Press(menu, {MenuAction::Back, MenuAction::Down}) == ControlsRequest::None);
    CHECK(menu.FocusedRow() == 0);

    menu.StopListening();

    CHECK_FALSE(menu.ListeningRow());
    CHECK(Press(menu, {MenuAction::Confirm}) == ControlsRequest::None);
    CHECK_FALSE(menu.ListeningRow());
}

TEST_CASE("the display row listens for a new input") {
    ControlsMenu menu;

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Down, MenuAction::Down,
                       MenuAction::Down, MenuAction::Down, MenuAction::Confirm}) ==
          ControlsRequest::Listen);
    CHECK(menu.ListeningRow() == 6);
}

TEST_CASE("the look back row, the last, listens for a new input") {
    ControlsMenu menu;

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Up, MenuAction::Up, MenuAction::Confirm}) ==
          ControlsRequest::Listen);
    CHECK(menu.ListeningRow() == 8);
}

TEST_CASE("FINISH and back both leave") {
    ControlsMenu menu;

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Up}) == ControlsRequest::None);
    CHECK(menu.IsFinishFocused());
    CHECK(Press(menu, {MenuAction::Confirm}) == ControlsRequest::Leave);
    CHECK(Press(menu, {MenuAction::Back}) == ControlsRequest::Leave);
}

}
}
