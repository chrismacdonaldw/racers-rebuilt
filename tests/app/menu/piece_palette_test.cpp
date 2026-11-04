#include "app/menu/piece_palette.hpp"

#include <doctest/doctest.h>

#include "app/menu/colour_swatches.hpp"

namespace rr::app {
namespace {

constexpr PaletteGrid kGrid{.area = {.x = 10.0F, .y = 20.0F, .width = 100.0F, .height = 40.0F},
                            .columns = 3,
                            .rows = 2,
                            .gap = 5.0F};

assets::PieceCatalog Catalog() {
    assets::PieceCatalog catalog;
    catalog.partSets.resize(2);
    catalog.partSets[0].choices.resize(8);
    catalog.partSets[1].choices.resize(2);
    return catalog;
}

TEST_CASE("the palette shows only the chosen set's pieces") {
    const assets::PieceCatalog catalog = Catalog();
    PiecePalette palette(kGrid);

    palette.Show(catalog, 1);
    CHECK(palette.Entries().size() == 2);
    CHECK(palette.Entries().back() == PaletteEntry{.set = 1, .choice = 1});

    palette.Show(catalog, 0);
    CHECK(palette.Entries().size() == 8);
    palette.Show(catalog, 5);
    CHECK(palette.Entries().empty());
}

TEST_CASE("tiles fill the grid row by row and scroll a row at a time") {
    const assets::PieceCatalog catalog = Catalog();
    PiecePalette palette(kGrid);
    palette.Show(catalog, 0);

    const render::Rect fifth = palette.TileOf(4).value_or(render::Rect{});
    CHECK(fifth.x == doctest::Approx(45.0F));
    CHECK(fifth.y == doctest::Approx(42.5F));
    CHECK(fifth.width == doctest::Approx(30.0F));
    CHECK_FALSE(palette.TileOf(6).has_value());
    CHECK(palette.EntryAt(50.0F, 50.0F) == 4);
    CHECK_FALSE(palette.EntryAt(42.0F, 50.0F).has_value());

    palette.Scroll(5);
    CHECK(palette.FirstRow() == 1);
    CHECK(palette.TileOf(6).has_value());
    CHECK_FALSE(palette.TileOf(0).has_value());
    palette.Scroll(-3);
    CHECK(palette.FirstRow() == 0);
}

TEST_CASE("the cursor stays on a piece and scrolls it into view") {
    const assets::PieceCatalog catalog = Catalog();
    PiecePalette palette(kGrid);
    palette.Show(catalog, 0);

    palette.MoveCursor(5, 0);
    CHECK(palette.Cursor() == 2);
    palette.MoveCursor(0, 2);
    CHECK(palette.Cursor() == 7);
    CHECK(palette.FirstRow() == 1);
    palette.MoveCursor(0, -2);
    CHECK(palette.Cursor() == 1);
    CHECK(palette.FirstRow() == 0);
}

TEST_CASE("swatches spread across their row and tell which one is under the pointer") {
    const ColourSwatches swatches(4, {.x = 0.0F, .y = 0.0F, .width = 190.0F, .height = 40.0F});

    CHECK(swatches.SwatchOf(0).width == doctest::Approx(40.0F));
    CHECK(swatches.SwatchOf(3).x == doctest::Approx(150.0F));
    CHECK(swatches.ColourAt(160.0F, 20.0F) == 3);
    CHECK_FALSE(swatches.ColourAt(45.0F, 20.0F).has_value());
    CHECK(ColourSwatches(300, {.width = 300.0F, .height = 1.0F}).Count() == 256);
}

}
}
