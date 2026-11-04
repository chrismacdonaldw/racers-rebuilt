#include "app/menu/workshop/workshop_panels.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "sim/input_frame.hpp"

namespace rr::app {
namespace {

constexpr render::Rect kWindow{.width = 640.0F, .height = 480.0F};
constexpr std::uint8_t kLeftHeld = 1U << static_cast<unsigned>(sim::PointerButton::Left);

assets::PieceCatalog Catalog() {
    assets::PieceCatalog catalog;
    catalog.partSets.resize(3);
    catalog.partSets[0].choices.resize(40);
    catalog.partSets[1].choices.assign(2, {.piece = 0, .colour = 4});
    catalog.partSets[2].choices.resize(1);
    catalog.colours.resize(10);
    catalog.extras.resize(3);
    return catalog;
}

WorkshopPanels Fitted(const assets::PieceCatalog& catalog) {
    WorkshopPanels panels(catalog, {0, 1}, {},
                          {{.folder = "RACEC0R0", .name = u"Knightmare"},
                           {.folder = "RACEC1R0", .name = u"Star Base"}});
    panels.Fit(kWindow);
    return panels;
}

bool ChoseTrack(const PanelTick& tick, std::string_view folder) {
    return tick.isOver && tick.background.has_value() && tick.background->background == folder;
}

bool ChoseColour(const PanelTick& tick) {
    return tick.background.has_value() && tick.background->colour.has_value();
}

sim::MenuPointer PressOn(const render::Rect& area) {
    return {.x = area.x + (area.width / 2.0F),
            .y = area.y + (area.height / 2.0F),
            .pressed = true,
            .held = kLeftHeld};
}

TEST_CASE("the toolbar and DONE give their actions") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);
    const WorkshopLayout& layout = panels.Layout();

    CHECK(panels.Tick(PressOn(layout.done)).action == PanelAction::Done);
    CHECK(panels.Tick(PressOn(layout.tools[0])).action == PanelAction::Undo);
    CHECK(panels.Tick(PressOn(layout.tools[4])).action == PanelAction::Frame);
    const PanelTick missed = panels.Tick(PressOn(layout.scene));
    CHECK_FALSE(missed.isOver);
    CHECK_FALSE(missed.action.has_value());
}

TEST_CASE("the background tool opens a menu that gives a track or a colour") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);
    const render::Rect tool = panels.Layout().tools[5];
    const BackgroundLayout& menu = panels.Backgrounds().Layout();

    CHECK_FALSE(panels.Tick(PressOn(tool)).action.has_value());
    REQUIRE(panels.Backgrounds().IsOpen());
    CHECK(ChoseTrack(panels.Tick(PressOn(menu.tracks[1])), "RACEC1R0"));
    CHECK_FALSE(panels.Backgrounds().IsOpen());

    static_cast<void>(panels.Tick(PressOn(tool)));
    CHECK(ChoseColour(panels.Tick(PressOn(menu.colours[0]))));
}

TEST_CASE("a press outside the open background menu only closes it") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);

    static_cast<void>(panels.Tick(PressOn(panels.Layout().tools[5])));
    const PanelTick outside = panels.Tick(PressOn(panels.Layout().done));
    CHECK(outside.isOver);
    CHECK_FALSE(outside.action.has_value());
    CHECK_FALSE(panels.Backgrounds().IsOpen());

    static_cast<void>(panels.Tick(PressOn(panels.Layout().tools[5])));
    static_cast<void>(panels.Tick(PressOn(panels.Layout().tools[5])));
    CHECK_FALSE(panels.Backgrounds().IsOpen());
}

TEST_CASE("a tab shows its set and a tile press grabs its piece") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);

    const PanelTick tab = panels.Tick(PressOn(TabOf(panels.Layout(), 1)));
    CHECK(tab.isOver);
    CHECK(panels.Tab() == 1);
    REQUIRE(panels.Drawer().Entries().size() == 2);

    const PanelTick tile = panels.Tick(PressOn(*panels.Drawer().TileOf(1)));
    CHECK(tile.grabbed == PaletteEntry{.set = 1, .choice = 1});
}

TEST_CASE("a swatch press gives its colour and a chosen colour paints new pieces") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);
    const PaletteEntry entry{.set = 1, .choice = 0};

    CHECK(panels.ColourOf(entry) == 4);
    const sim::MenuPointer onThird{.x = panels.Layout().colours.x + 70.0F,
                                   .y = panels.Layout().colours.y + 14.0F,
                                   .pressed = true};
    CHECK(panels.Tick(onThird).swatch == 2);

    panels.ChooseColour(7);
    CHECK(panels.ColourOf(entry) == 7);
    panels.Fit({.x = -106.667F, .width = 853.333F, .height = 480.0F});
    CHECK(panels.ColourOf(entry) == 7);
    panels.ChooseColour(std::nullopt);
    CHECK(panels.ColourOf(entry) == 4);
}

TEST_CASE("the wheel over the drawer scrolls it") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);
    const render::Rect& drawer = panels.Layout().drawer;

    static_cast<void>(panels.Tick({.x = drawer.x + 4.0F, .y = drawer.y + 4.0F, .wheel = -1.0F}));
    CHECK(panels.Drawer().FirstRow() == 1);
    static_cast<void>(panels.Tick({.x = 600.0F, .y = 240.0F, .wheel = -1.0F}));
    CHECK(panels.Drawer().FirstRow() == 1);
}

TEST_CASE("the LDraw tab comes and goes with the extra pieces") {
    const assets::PieceCatalog catalog = Catalog();
    WorkshopPanels panels = Fitted(catalog);

    panels.OfferExtras(true);
    REQUIRE(panels.Sets().size() == 3);
    CHECK(panels.Sets().back() == catalog.ExtraSet());
    static_cast<void>(panels.Tick(PressOn(TabOf(panels.Layout(), 2))));
    CHECK(panels.Drawer().Entries().size() == 3);

    panels.OfferExtras(false);
    CHECK(panels.Sets().size() == 2);
    CHECK(panels.Tab() == 1);
    CHECK(panels.Drawer().Entries().size() == 2);
}

}
}
