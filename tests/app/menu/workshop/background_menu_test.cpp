#include "app/menu/workshop/background_menu.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "app/menu/builder_settings.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {
namespace {

constexpr render::Rect kScene{.x = 196.0F, .width = 444.0F, .height = 480.0F};
constexpr render::Rect kButton{.x = 492.0F, .y = 8.0F, .width = 52.0F, .height = 22.0F};
constexpr std::uint8_t kLeftHeld = 1U << static_cast<unsigned>(sim::PointerButton::Left);

bool IsInside(const render::Rect& inner, const render::Rect& outer) {
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width + 0.01F &&
           inner.y + inner.height <= outer.y + outer.height + 0.01F;
}

bool FitsInPanel(const BackgroundLayout& layout) {
    return std::ranges::all_of(
               layout.colours,
               [&](const render::Rect& swatch) { return IsInside(swatch, layout.panel); }) &&
           std::ranges::all_of(layout.tracks, [&](const render::Rect& track) {
               return IsInside(track, layout.panel);
           });
}

std::vector<BackdropTrack> Tracks() {
    return {{.folder = "RACEC0R0", .name = u"Knightmare"},
            {.folder = "RACEC1R0", .name = u"Star Base"},
            {.folder = "RACEC2R0", .name = u"Ice Planet"}};
}

bool IsTrack(const std::optional<BackgroundChoice>& choice, std::string_view folder) {
    return choice.has_value() && choice->background == folder && !choice->colour.has_value();
}

bool IsColour(const std::optional<BackgroundChoice>& choice, std::size_t index) {
    return choice.has_value() && choice->background == kColourBackground &&
           choice->colour.has_value() &&
           FormatHexColour(*choice->colour) == FormatHexColour(BackgroundColour(index));
}

sim::MenuPointer PressOn(const render::Rect& area) {
    return {.x = area.x + (area.width / 2.0F),
            .y = area.y + (area.height / 2.0F),
            .pressed = true,
            .held = kLeftHeld};
}

TEST_CASE("the background menu hangs under its button inside the scene") {
    const BackgroundLayout layout = LayOutBackgroundMenu(kButton, kScene, 13);

    CHECK(IsInside(layout.panel, kScene));
    CHECK(layout.panel.y > kButton.y + kButton.height);
    CHECK(FitsInPanel(layout));

    const render::Rect nearEdge{.x = kScene.x + 4.0F, .y = 8.0F, .width = 52.0F, .height = 22.0F};
    CHECK(IsInside(LayOutBackgroundMenu(nearEdge, kScene, 13).panel, kScene));
}

TEST_CASE("tracks fill two columns, row by row") {
    const BackgroundLayout layout = LayOutBackgroundMenu(kButton, kScene, 3);

    REQUIRE(layout.tracks.size() == 3);
    CHECK(layout.tracks[1].x > layout.tracks[0].x);
    CHECK(layout.tracks[1].y == doctest::Approx(layout.tracks[0].y));
    CHECK(layout.tracks[2].x == doctest::Approx(layout.tracks[0].x));
    CHECK(layout.tracks[2].y > layout.tracks[0].y);
    CHECK(layout.tracks[0].y > layout.colours[0].y + layout.colours[0].height);
    CHECK(LayOutBackgroundMenu(kButton, kScene, 0).panel.height < layout.panel.height);
}

TEST_CASE("the menu offers only its own tracks") {
    const BackgroundMenu menu(Tracks());

    CHECK(menu.Offers("RACEC1R0"));
    CHECK_FALSE(menu.Offers("RACEC1R1"));
    CHECK_FALSE(menu.Offers(kColourBackground));
    CHECK_FALSE(menu.Offers("../SAVE"));
}

TEST_CASE("a closed menu covers nothing and gives nothing") {
    BackgroundMenu menu(Tracks());
    menu.Place(kButton, kScene);

    CHECK_FALSE(menu.Covers(menu.Layout().panel.x + 1.0F, menu.Layout().panel.y + 1.0F));
    CHECK_FALSE(menu.Tick(PressOn(menu.Layout().tracks[0])).has_value());
}

TEST_CASE("a press on a track chooses it and closes the menu") {
    BackgroundMenu menu(Tracks());
    menu.Place(kButton, kScene);
    const render::Rect track = menu.Layout().tracks[1];

    menu.Toggle();
    CHECK_FALSE(menu.Tick({.x = track.x + 2.0F, .y = track.y + 2.0F}).has_value());
    CHECK(IsTrack(menu.Tick(PressOn(track)), "RACEC1R0"));
    CHECK_FALSE(menu.IsOpen());
}

TEST_CASE("a press on a colour chooses the colour background") {
    BackgroundMenu menu(Tracks());
    menu.Place(kButton, kScene);

    menu.Toggle();
    CHECK(IsColour(menu.Tick(PressOn(menu.Layout().colours[2])), 2));
    CHECK_FALSE(menu.IsOpen());
}

}
}
