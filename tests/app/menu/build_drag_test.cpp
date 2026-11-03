#include "app/menu/build_drag.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

const BuildRegions kRegions{
    .scene = {.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 100.0F},
    .piece = render::Rect{.x = 10.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F},
    .car = render::Rect{.x = 0.0F, .y = 0.0F, .width = 80.0F, .height = 80.0F}};

sim::MenuPointer At(float x, float y) {
    return {.x = x, .y = y, .isOver = true};
}

sim::MenuPointer PressedAt(float x, float y) {
    sim::MenuPointer pointer = At(x, y);
    pointer.pressed = true;
    return pointer;
}

sim::MenuPointer Moved(float dx, float dy) {
    sim::MenuPointer pointer = At(15.0F, 15.0F);
    pointer.dx = dx;
    pointer.dy = dy;
    return pointer;
}

int TicksBeforeMove(BuildDrag& drag, CarBuildMenu& menu) {
    int waited = 0;
    while (drag.Tick(Moved(-9.0F, 0.0F), {}, menu).request.command == BuildCommand::None &&
           waited < 20) {
        ++waited;
    }
    return waited;
}

sim::MenuPointer Released() {
    sim::MenuPointer pointer = At(15.0F, 15.0F);
    pointer.released = true;
    return pointer;
}

}

TEST_CASE("a sphere projects to a square around its centre") {
    const render::Camera camera{.verticalFovDegrees = 90.0F};
    const render::Rect view{.x = 0.0F, .y = 0.0F, .width = 200.0F, .height = 100.0F};

    const render::Rect ahead =
        SphereOnScreen(camera, view, {.centre = {.x = 2.0F, .z = -10.0F}, .radius = 1.0F})
            .value_or(render::Rect{});
    CHECK(ahead.x == doctest::Approx(105.0F));
    CHECK(ahead.y == doctest::Approx(45.0F));
    CHECK(ahead.width == doctest::Approx(10.0F));

    CHECK_FALSE(SphereOnScreen(camera, view, {.centre = {.z = 5.0F}, .radius = 1.0F}).has_value());
}

TEST_CASE("the piece lights before the car, and nothing lights off the scene") {
    BuildDrag drag;
    CarBuildMenu menu;

    static_cast<void>(drag.Tick(At(15.0F, 15.0F), kRegions, menu));
    CHECK(drag.Lit() == BuildRegion::Piece);
    static_cast<void>(drag.Tick(At(50.0F, 50.0F), kRegions, menu));
    CHECK(drag.Lit() == BuildRegion::Car);
    static_cast<void>(drag.Tick(At(90.0F, 90.0F), kRegions, menu));
    CHECK_FALSE(drag.Lit().has_value());
    static_cast<void>(drag.Tick(At(150.0F, 15.0F), kRegions, menu));
    CHECK_FALSE(drag.Lit().has_value());
}

TEST_CASE("dragging the held piece moves it, then waits 100 ms") {
    BuildDrag drag;
    CarBuildMenu menu;

    static_cast<void>(drag.Tick(PressedAt(15.0F, 15.0F), kRegions, menu));
    REQUIRE(menu.Held() == BuildPad::Move);
    CHECK(drag.Tick(Moved(3.0F, 0.0F), {}, menu).request.command == BuildCommand::None);
    CHECK(drag.Tick(Moved(3.0F, -6.0F), {}, menu).request ==
          BuildRequest{.command = BuildCommand::Move, .direction = 1});
    CHECK(TicksBeforeMove(drag, menu) == 8);
    CHECK(drag.Lit() == BuildRegion::Piece);
}

TEST_CASE("a release lets go of the held piece") {
    BuildDrag drag;
    CarBuildMenu menu;

    static_cast<void>(drag.Tick(PressedAt(15.0F, 15.0F), kRegions, menu));
    REQUIRE(menu.Held() == BuildPad::Move);
    static_cast<void>(drag.Tick(Released(), kRegions, menu));
    CHECK_FALSE(menu.Held().has_value());
}

TEST_CASE("a second press on the piece within 500 ms places it") {
    BuildDrag drag;
    CarBuildMenu menu;

    static_cast<void>(drag.Tick(PressedAt(15.0F, 15.0F), kRegions, menu));
    static_cast<void>(drag.Tick(Released(), kRegions, menu));
    CHECK(drag.Tick(PressedAt(15.0F, 15.0F), kRegions, menu).request.command ==
          BuildCommand::Place);
    CHECK_FALSE(menu.Held().has_value());

    static_cast<void>(drag.Tick(PressedAt(15.0F, 15.0F), kRegions, menu));
    static_cast<void>(drag.Tick(Released(), kRegions, menu));
    for (int tick = 0; tick < 42; ++tick) {
        static_cast<void>(drag.Tick(At(15.0F, 15.0F), kRegions, menu));
    }
    CHECK(drag.Tick(PressedAt(15.0F, 15.0F), kRegions, menu).request.command == BuildCommand::None);
    CHECK(menu.Held() == BuildPad::Move);
}

TEST_CASE("dragging the held car swings the view") {
    BuildDrag drag;
    CarBuildMenu menu;

    static_cast<void>(drag.Tick(PressedAt(50.0F, 50.0F), kRegions, menu));
    REQUIRE(menu.Held() == BuildPad::View);
    const SceneMouse mouse = drag.Tick(Moved(10.0F, 20.0F), {}, menu);
    CHECK(mouse.turn == doctest::Approx(-0.1F));
    CHECK(mouse.tilt == doctest::Approx(0.2F));
}

TEST_CASE("a right press on the piece rotates it") {
    BuildDrag drag;
    CarBuildMenu menu;

    sim::MenuPointer right = PressedAt(15.0F, 15.0F);
    right.rightPressed = true;
    CHECK(drag.Tick(right, kRegions, menu).request.command == BuildCommand::Rotate);
    CHECK_FALSE(menu.Held().has_value());

    right.x = 50.0F;
    right.y = 50.0F;
    CHECK(drag.Tick(right, kRegions, menu).request.command == BuildCommand::None);
}

}
