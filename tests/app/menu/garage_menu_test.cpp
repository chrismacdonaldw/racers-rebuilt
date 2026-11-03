#include "app/menu/garage_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

using sim::MenuAction;

GarageRequest Press(GarageMenu& menu, std::initializer_list<MenuAction> actions) {
    GarageRequest request = GarageRequest::None;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        request = menu.Tick(frame);
    }
    return request;
}

TEST_CASE("an empty garage offers only NEW RACER and back") {
    GarageMenu menu(0);

    CHECK(menu.Focus() == GarageButton::New);
    CHECK_FALSE(menu.IsEnabled(GarageButton::Edit));
    CHECK_FALSE(menu.IsEnabled(GarageButton::Copy));
    CHECK_FALSE(menu.IsEnabled(GarageButton::Delete));
    CHECK_FALSE(menu.IsEnabled(GarageButton::TestDrive));
    CHECK(Press(menu, {MenuAction::Down}) == GarageRequest::None);
    CHECK(menu.Focus() == GarageButton::Back);
    CHECK(Press(menu, {MenuAction::Right}) == GarageRequest::None);
    CHECK(Press(menu, {MenuAction::Confirm}) == GarageRequest::Leave);
}

TEST_CASE("a full garage opens on EDIT RACER and can't make or copy racers") {
    GarageMenu menu(kGarageCapacity);

    CHECK(menu.Focus() == GarageButton::Edit);
    CHECK_FALSE(menu.IsEnabled(GarageButton::New));
    CHECK_FALSE(menu.IsEnabled(GarageButton::Copy));
    CHECK(Press(menu, {MenuAction::Down}) == GarageRequest::None);
    CHECK(menu.Focus() == GarageButton::Delete);
}

TEST_CASE("left and right step through the racers, wrapping") {
    GarageMenu menu(3);

    CHECK(Press(menu, {MenuAction::Left}) == GarageRequest::Step);
    CHECK(menu.Racer() == 2);
    CHECK(Press(menu, {MenuAction::Right, MenuAction::Right}) == GarageRequest::Step);
    CHECK(menu.Racer() == 1);
}

TEST_CASE("EDIT RACER opens its page, and back returns to it") {
    GarageMenu menu(2);

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Confirm}) == GarageRequest::Page);
    CHECK(menu.IsEditing());
    CHECK(menu.Focus() == GarageButton::Driver);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Confirm}) == GarageRequest::Licence);
    CHECK(Press(menu, {MenuAction::Right}) == GarageRequest::None);
    CHECK(menu.Racer() == 0);
    CHECK(Press(menu, {MenuAction::Back}) == GarageRequest::Page);
    CHECK_FALSE(menu.IsEditing());
    CHECK(menu.Focus() == GarageButton::Edit);
    CHECK(Press(menu, {MenuAction::Back}) == GarageRequest::Leave);
}

TEST_CASE("deleting the last racer moves focus back to NEW RACER") {
    GarageMenu menu(1);

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Down}) ==
          GarageRequest::None);
    CHECK(Press(menu, {MenuAction::Confirm}) == GarageRequest::Delete);
    menu.Refresh(0, 0, GarageButton::Delete);
    CHECK(menu.Focus() == GarageButton::New);
    menu.Refresh(2, 5, GarageButton::Delete);
    CHECK(menu.Racer() == 1);
    CHECK(menu.Focus() == GarageButton::Delete);
}

}
}
