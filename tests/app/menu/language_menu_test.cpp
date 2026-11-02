#include "app/menu/language_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

using sim::MenuAction;

LanguageRequest Press(LanguageMenu& menu, std::initializer_list<MenuAction> actions) {
    LanguageRequest request = LanguageRequest::None;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        request = menu.Tick(frame);
    }
    return request;
}

}

TEST_CASE("the picker opens on the language in use, with the picker focused") {
    const LanguageMenu menu("GERMAN");

    CHECK(menu.Index() == 3);
    CHECK(menu.Language() == "GERMAN");
    CHECK_FALSE(menu.IsBackFocused());
}

TEST_CASE("a language the picker can't show starts on English") {
    CHECK(LanguageMenu("FINNISH").Language() == "ENGLISH");
}

TEST_CASE("stepping wraps around both ends of the list") {
    LanguageMenu menu("ENGLISH");

    CHECK(Press(menu, {MenuAction::Left}) == LanguageRequest::Change);
    CHECK(menu.Language() == "DUTCH");
    CHECK(Press(menu, {MenuAction::Right}) == LanguageRequest::Change);
    CHECK(menu.Language() == "ENGLISH");
}

TEST_CASE("left and right do nothing while BACK is focused") {
    LanguageMenu menu("SPANISH");

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Right}) == LanguageRequest::None);
    CHECK(menu.IsBackFocused());
    CHECK(menu.Language() == "SPANISH");
}

TEST_CASE("BACK leaves when confirmed, and the back action leaves from anywhere") {
    LanguageMenu menu("ENGLISH");

    CHECK(Press(menu, {MenuAction::Confirm}) == LanguageRequest::None);
    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == LanguageRequest::Leave);

    LanguageMenu other("ENGLISH");
    CHECK(Press(other, {MenuAction::Back}) == LanguageRequest::Leave);
}

}
