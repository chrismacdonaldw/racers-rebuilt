#include "app/menu/options_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>

namespace rr::app {
namespace {

using sim::MenuAction;

OptionsRequest Press(OptionsMenu& menu, std::initializer_list<MenuAction> actions) {
    OptionsRequest request = OptionsRequest::None;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        request = menu.Tick(frame);
    }
    return request;
}

TEST_CASE("the options screen opens on the main page with GAME focused") {
    const OptionsMenu menu(OptionValues{});

    CHECK(menu.Page() == OptionsPage::Main);
    CHECK(menu.Focus() == OptionsRow::Game);
}

TEST_CASE("game options open at once, focused on the opponent count") {
    OptionsMenu menu(OptionValues{});

    CHECK(Press(menu, {MenuAction::Confirm}) == OptionsRequest::None);

    CHECK(menu.Page() == OptionsPage::Game);
    CHECK(menu.Focus() == OptionsRow::Opponents);
}

TEST_CASE("video and audio options warn first and open once the warning is dismissed") {
    OptionsMenu menu(OptionValues{});

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Confirm}) ==
          OptionsRequest::Warn);
    CHECK(menu.Page() == OptionsPage::Main);

    menu.Proceed();

    CHECK(menu.Page() == OptionsPage::Audio);
    CHECK(menu.Focus() == OptionsRow::MusicVolume);
}

TEST_CASE("back leaves a page for the main page with GAME focused, then leaves the screen") {
    OptionsMenu menu(OptionValues{});
    static_cast<void>(Press(menu, {MenuAction::Confirm, MenuAction::Down}));

    CHECK(Press(menu, {MenuAction::Back}) == OptionsRequest::None);
    CHECK(menu.Page() == OptionsPage::Main);
    CHECK(menu.Focus() == OptionsRow::Game);

    CHECK(Press(menu, {MenuAction::Back}) == OptionsRequest::Leave);
}

TEST_CASE("the screens that don't exist yet only report it") {
    OptionsMenu menu(OptionValues{});

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Up, MenuAction::Confirm}) ==
          OptionsRequest::NotBuilt);
    CHECK(menu.Focus() == OptionsRow::Credits);
}

TEST_CASE("player 1 controls opens the controls screen") {
    OptionsMenu menu(OptionValues{});

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Down,
                       MenuAction::Confirm}) == OptionsRequest::Controls);
    CHECK(menu.Focus() == OptionsRow::Player1);
}

TEST_CASE("a selector steps its value and stops at either end") {
    OptionsMenu menu(OptionValues{.opponents = 4, .versusLaps = 1});
    static_cast<void>(Press(menu, {MenuAction::Confirm}));

    CHECK(Press(menu, {MenuAction::Right}) == OptionsRequest::ChangeValue);
    CHECK(Press(menu, {MenuAction::Right}) == OptionsRequest::None);
    CHECK(menu.Values().opponents == kMaxOpponents);

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Left}) == OptionsRequest::None);
    CHECK(menu.Values().versusLaps == 1);
}

TEST_CASE("audio sliders step by one of 20, and left picks stereo, right mono") {
    OptionsMenu menu(OptionValues{.soundStep = 0});
    static_cast<void>(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Confirm}));
    menu.Proceed();

    CHECK(Press(menu, {MenuAction::Left}) == OptionsRequest::ChangeValue);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Left}) == OptionsRequest::None);
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Right}) == OptionsRequest::ChangeValue);

    CHECK(menu.Values() ==
          OptionValues{.musicStep = kMaxVolumeStep - 1, .soundStep = 0, .isStereo = false});
}

TEST_CASE("the video page's one driver doesn't step") {
    OptionsMenu menu(OptionValues{});
    static_cast<void>(Press(menu, {MenuAction::Down, MenuAction::Confirm}));
    menu.Proceed();

    CHECK(menu.Focus() == OptionsRow::Driver);
    CHECK(Press(menu, {MenuAction::Right}) == OptionsRequest::None);
}

}
}
