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

TEST_CASE("the controls screen opens on the keyboard with TURN LEFT focused") {
    const ControlsMenu menu;

    CHECK(menu.Device() == ControlsDevice::Keyboard);
    CHECK(menu.FocusedRow() == 0);
}

TEST_CASE("the device picker sits above the rows and steps between keyboard and gamepad") {
    ControlsMenu menu;

    static_cast<void>(Press(menu, {MenuAction::Up, MenuAction::Right, MenuAction::Right}));
    CHECK(menu.IsDeviceFocused());
    CHECK(menu.Device() == ControlsDevice::Gamepad);

    static_cast<void>(Press(menu, {MenuAction::Left}));
    CHECK(menu.Device() == ControlsDevice::Keyboard);
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

TEST_CASE("the camera row has no action to bind yet") {
    ControlsMenu menu;

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Down, MenuAction::Down,
                       MenuAction::Down, MenuAction::Confirm}) == ControlsRequest::NotBuilt);
    CHECK_FALSE(menu.ListeningRow());
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
