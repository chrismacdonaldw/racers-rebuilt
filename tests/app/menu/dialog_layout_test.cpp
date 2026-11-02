#include "app/menu/dialog_layout.hpp"

#include <doctest/doctest.h>

#include <array>

namespace rr::app {
namespace {

bool Same(const render::Rect& a, const render::Rect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

TEST_CASE("a dialog grows for 300 ms, stays open until closed, then shrinks for 300 ms") {
    DialogAnimation animation;
    CHECK(animation.Scale() == 0.0F);
    animation.Close();
    animation.Advance(Milliseconds{150});
    CHECK(animation.Scale() == 0.5F);
    animation.Advance(Milliseconds{150});
    CHECK(animation.Phase() == DialogPhase::Opening);
    animation.Advance(Milliseconds{12});
    CHECK(animation.Phase() == DialogPhase::Open);
    CHECK(animation.Scale() == 1.0F);

    animation.Close();
    animation.Advance(Milliseconds{75});
    CHECK(animation.Phase() == DialogPhase::Closing);
    CHECK(animation.Scale() == 0.75F);
    animation.Advance(Milliseconds{300});
    CHECK(animation.Phase() == DialogPhase::Closed);
    CHECK(animation.Scale() == 0.0F);
}

constexpr DialogButtonSize kCheck{.image = {.width = 19.0F, .height = 24.0F},
                                  .text = {.width = 20.0F, .height = 16.0F}};

TEST_CASE("a dialog centres its frame and puts the button under the message") {
    const std::array buttons{kCheck};

    const DialogLayout layout =
        LayOutDialog({.message = {.width = 200.0F, .height = 40.0F}, .buttons = buttons});

    CHECK(Same(DialogFrame(layout, 1.0F),
               {.x = 200.0F, .y = 178.0F, .width = 240.0F, .height = 124.0F}));
    CHECK(Same(layout.message, {.x = 220.0F, .y = 198.0F, .width = 200.0F, .height = 40.0F}));
    REQUIRE(layout.buttons.size() == 1);
    CHECK(
        Same(layout.buttons[0].image, {.x = 225.0F, .y = 258.0F, .width = 19.0F, .height = 24.0F}));
    CHECK(
        Same(layout.buttons[0].text, {.x = 244.0F, .y = 262.0F, .width = 20.0F, .height = 16.0F}));
}

TEST_CASE("a second button sits 20 units under the first and grows the frame") {
    const DialogButtonSize cross{.image = {.width = 19.0F, .height = 26.0F},
                                 .text = {.width = 20.0F, .height = 16.0F}};
    const std::array buttons{kCheck, cross};

    const DialogLayout layout =
        LayOutDialog({.message = {.width = 200.0F, .height = 40.0F}, .buttons = buttons});

    CHECK(Same(DialogFrame(layout, 1.0F),
               {.x = 200.0F, .y = 155.0F, .width = 240.0F, .height = 170.0F}));
    REQUIRE(layout.buttons.size() == 2);
    CHECK(layout.buttons[0].image.y == 235.0F);
    CHECK(
        Same(layout.buttons[1].image, {.x = 225.0F, .y = 279.0F, .width = 19.0F, .height = 26.0F}));
    CHECK(layout.buttons[1].text.y == 284.0F);
}

TEST_CASE("a scaled dialog shrinks its frame in whole units from the centre") {
    const std::array buttons{kCheck};

    const DialogLayout layout =
        LayOutDialog({.message = {.width = 201.0F, .height = 40.0F}, .buttons = buttons});

    CHECK(Same(DialogFrame(layout, 0.5F),
               {.x = 260.0F, .y = 209.0F, .width = 120.0F, .height = 62.0F}));
    CHECK(layout.message.x == 220.0F);
}

TEST_CASE("a frame keeps its corners and stretches its edges and fill between them") {
    const BoxSize corner{.width = 8.0F, .height = 6.0F};
    const BoxSize across{.width = 1.0F, .height = 6.0F};
    const BoxSize down{.width = 8.0F, .height = 1.0F};

    const std::array<render::Rect, kFrameParts + 1> slices =
        FrameSlices({.x = 10.0F, .y = 20.0F, .width = 100.0F, .height = 50.0F},
                    {corner, across, corner, down, corner, across, corner, down});

    CHECK(Same(slices[0], {.x = 10.0F, .y = 20.0F, .width = 8.0F, .height = 6.0F}));
    CHECK(Same(slices[1], {.x = 18.0F, .y = 20.0F, .width = 84.0F, .height = 6.0F}));
    CHECK(Same(slices[2], {.x = 102.0F, .y = 20.0F, .width = 8.0F, .height = 6.0F}));
    CHECK(Same(slices[3], {.x = 102.0F, .y = 26.0F, .width = 8.0F, .height = 38.0F}));
    CHECK(Same(slices[4], {.x = 102.0F, .y = 64.0F, .width = 8.0F, .height = 6.0F}));
    CHECK(Same(slices[5], {.x = 18.0F, .y = 64.0F, .width = 84.0F, .height = 6.0F}));
    CHECK(Same(slices[6], {.x = 10.0F, .y = 64.0F, .width = 8.0F, .height = 6.0F}));
    CHECK(Same(slices[7], {.x = 10.0F, .y = 26.0F, .width = 8.0F, .height = 38.0F}));
    CHECK(Same(slices[8], {.x = 18.0F, .y = 26.0F, .width = 84.0F, .height = 38.0F}));
}

}
}
