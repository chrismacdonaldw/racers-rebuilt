#include "app/menu/edit_driver_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

#include "app/menu/menu_scenes.hpp"

namespace rr::app {
namespace {

using sim::MenuAction;

DriverRequest Press(EditDriverMenu& menu, std::initializer_list<MenuAction> actions) {
    DriverRequest request = DriverRequest::None;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        request = menu.Tick(frame);
    }
    return request;
}

}

TEST_CASE("the driver editor opens on the hat and steps the focused part") {
    EditDriverMenu menu;

    CHECK(menu.Slot() == DriverSlot::Hat);
    CHECK(Press(menu, {MenuAction::Left}) == DriverRequest::Step);
    CHECK(menu.Direction() == -1);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Right}) ==
          DriverRequest::Step);
    CHECK(menu.Slot() == DriverSlot::Torso);
    CHECK(menu.Direction() == 1);
    CHECK(Press(menu, {MenuAction::Confirm}) == DriverRequest::None);
}

TEST_CASE("the driver editor's buttons mix, finish and leave") {
    EditDriverMenu menu;

    CHECK(Press(menu, {MenuAction::Up}) == DriverRequest::None);
    CHECK(menu.Focus() == DriverRow::Back);
    CHECK_FALSE(menu.Slot().has_value());
    CHECK(Press(menu, {MenuAction::Left}) == DriverRequest::None);
    CHECK(Press(menu, {MenuAction::Confirm}) == DriverRequest::Leave);
    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == DriverRequest::Done);
    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == DriverRequest::Mix);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Down}) ==
          DriverRequest::None);
    CHECK(menu.Focus() == DriverRow::Hat);
    CHECK(Press(menu, {MenuAction::Back}) == DriverRequest::Leave);
}

TEST_CASE("menu scenes for one racer carry its index") {
    CHECK(RacerScene(kEditDriverScene, 3) == "menu:edit_driver:3");
    CHECK(SceneRacer("menu:edit_driver:3", kEditDriverScene) == std::optional<std::size_t>(3));
    CHECK(SceneRacer("menu:garage:12", kGarageScene) == std::optional<std::size_t>(12));
    CHECK_FALSE(SceneRacer("menu:garage", kGarageScene).has_value());
    CHECK_FALSE(SceneRacer("menu:garage:", kGarageScene).has_value());
    CHECK_FALSE(SceneRacer("menu:garage:1x", kGarageScene).has_value());
    CHECK_FALSE(SceneRacer("menu:edit_driver:2", kGarageScene).has_value());
}

}
