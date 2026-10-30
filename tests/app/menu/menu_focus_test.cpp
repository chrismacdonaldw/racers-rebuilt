#include "app/menu/menu_focus.hpp"

#include <doctest/doctest.h>

#include <array>

namespace rr::app {
namespace {

constexpr std::array kColumn{
    render::Rect{.x = 3, .y = 118, .width = 32, .height = 32},
    render::Rect{.x = 3, .y = 178, .width = 32, .height = 32},
    render::Rect{.x = 3, .y = 218, .width = 32, .height = 32},
};

TEST_CASE("focus moves down a column and wraps round to the top") {
    CHECK(NextFocus(kColumn, 0, FocusMove::Down) == 1);
    CHECK(NextFocus(kColumn, 1, FocusMove::Down) == 2);
    CHECK(NextFocus(kColumn, 2, FocusMove::Down) == 0);
    CHECK(NextFocus(kColumn, 0, FocusMove::Up) == 2);
}

TEST_CASE("focus stays put with nothing to either side") {
    CHECK(NextFocus(kColumn, 1, FocusMove::Left) == 1);
    CHECK(NextFocus(kColumn, 1, FocusMove::Right) == 1);
}

TEST_CASE("focus prefers what lines up over what is nearer but off to the side") {
    constexpr std::array kGrid{
        render::Rect{.x = 0, .y = 0, .width = 10, .height = 10},
        render::Rect{.x = 100, .y = 20, .width = 10, .height = 10},
        render::Rect{.x = 0, .y = 60, .width = 10, .height = 10},
    };

    CHECK(NextFocus(kGrid, 0, FocusMove::Down) == 2);
}

}
}
