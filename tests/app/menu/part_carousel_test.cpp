#include "app/menu/part_carousel.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "app/racer/racer_models.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

constexpr CarouselShape kShape{.slots = 5, .focused = 2};
constexpr Milliseconds kScroll{400};
constexpr std::int32_t kRecordsUnlock = 0x80;

assets::DriverPartCatalog Catalog() {
    assets::DriverPartCatalog catalog;
    for (const std::int32_t unlock : {0, 2, 3, 4, kRecordsUnlock}) {
        catalog.legs.parts.push_back({.unlock = unlock});
    }
    return catalog;
}

settings::Progress EverythingWon() {
    settings::Progress progress{.partSets = {0, 1}};
    for (std::int32_t race = 0; std::cmp_less(race, settings::kRecordRaces); ++race) {
        progress.beatenRecords.insert(race);
    }
    return progress;
}

std::vector<std::uint8_t> Shown(const PartCarousel& carousel) {
    std::vector<std::uint8_t> shown;
    shown.reserve(kShape.slots);
    for (std::size_t slot = 0; slot < kShape.slots; ++slot) {
        shown.push_back(carousel.PartInSlot(slot).value_or(UINT8_MAX));
    }
    return shown;
}

settings::Progress EverySetWon() {
    settings::Progress won = EverythingWon();
    for (std::int32_t set = 0; set < 32; ++set) {
        won.partSets.insert(set);
    }
    return won;
}

void CheckShelf(const assets::DriverPartCatalog& catalog, std::size_t slot, std::size_t count) {
    CAPTURE(slot);
    const auto shelf = static_cast<DriverSlot>(slot);
    CHECK(UnlockedParts(catalog, shelf, 0, EverySetWon()).size() == count);
    CHECK(UnlockedParts(catalog, shelf, 0, {}).size() < count);
}

}

TEST_CASE("a fresh game lists only the parts it starts with, plus the racer's own") {
    const assets::DriverPartCatalog catalog = Catalog();

    CHECK(UnlockedParts(catalog, DriverSlot::Legs, 0, {}) == std::vector<std::uint8_t>{0, 1});
    CHECK(UnlockedParts(catalog, DriverSlot::Legs, 3, {}) == std::vector<std::uint8_t>{0, 1, 3});
    CHECK(UnlockedParts(catalog, DriverSlot::Legs, 0, {.partSets = {0}}) ==
          std::vector<std::uint8_t>{0, 1, 2});
    CHECK(UnlockedParts(catalog, DriverSlot::Legs, 0, EverythingWon()) ==
          std::vector<std::uint8_t>{0, 1, 2, 3, 4});
    CHECK(UnlockedParts(catalog, DriverSlot::Hat, 0, EverythingWon()).empty());
}

TEST_CASE("every retail part shows once every part set and record is won") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<assets::DriverPartCatalog> catalog = LoadDriverParts(*vfs);
    REQUIRE(catalog.has_value());
    constexpr std::array kShelves{std::size_t{36}, std::size_t{30}, std::size_t{29},
                                  std::size_t{20}};
    for (std::size_t slot = 0; slot < kDriverSlots; ++slot) {
        CheckShelf(*catalog, slot, kShelves.at(slot));
    }
    CHECK(UnlockedParts(*catalog, DriverSlot::Legs, 0, {}).size() == 12);
}

TEST_CASE("a carousel steps round its ring and ignores steps while it scrolls") {
    PartCarousel carousel({10, 11, 12, 13, 14, 15}, 11, kShape, kScroll);

    CHECK(carousel.Part() == 11);
    CHECK(Shown(carousel) == std::vector<std::uint8_t>{15, 10, 11, 12, 13});
    CHECK(carousel.Step(-1) == CarouselStep::Moved);
    CHECK(carousel.Part() == 10);
    CHECK(carousel.Step(-1) == CarouselStep::Ignored);
    CHECK(carousel.Lag() == doctest::Approx(1.0F));
    CHECK(carousel.CameFrom(2) == std::optional<std::size_t>(1));
    CHECK_FALSE(carousel.CameFrom(0).has_value());

    carousel.Tick(Milliseconds{100});
    CHECK(carousel.Lag() == doctest::Approx(0.75F));
    carousel.Tick(kScroll);
    CHECK_FALSE(carousel.IsScrolling());
    CHECK_FALSE(carousel.CameFrom(2).has_value());
    CHECK(carousel.Step(-1) == CarouselStep::Moved);
    CHECK(carousel.Part() == 15);
}

TEST_CASE("a carousel with fewer parts than its slots but one stops at its ends") {
    PartCarousel carousel({4, 7, 9}, 9, kShape, kScroll);

    CHECK(Shown(carousel) == std::vector<std::uint8_t>{4, 7, 9, UINT8_MAX, UINT8_MAX});
    CHECK(carousel.Step(1) == CarouselStep::AtEnd);
    CHECK(carousel.Step(-1) == CarouselStep::Moved);
    CHECK(carousel.Part() == 7);

    carousel.Select(0);
    CHECK_FALSE(carousel.IsScrolling());
    CHECK(carousel.Part() == 4);
    CHECK(carousel.Step(-1) == CarouselStep::AtEnd);
}

TEST_CASE("a carousel of the slots but one wraps") {
    PartCarousel carousel({1, 2, 3, 4}, 4, kShape, kScroll);

    CHECK(carousel.Step(1) == CarouselStep::Moved);
    CHECK(carousel.Part() == 1);
}

TEST_CASE("a look's parts are its hat, face, torso and legs") {
    assets::DriverLook look{
        .face = 1, .hat = 2, .legs = 3, .torso = 4, .hatName = "kk_hats", .torsoName = "tu_chst"};

    PickPart(look, DriverSlot::Torso, 9);

    CHECK(PartOf(look, DriverSlot::Hat) == 2);
    CHECK(PartOf(look, DriverSlot::Face) == 1);
    CHECK(PartOf(look, DriverSlot::Legs) == 3);
    CHECK(look.torso == 9);
    CHECK(look.torsoName.empty());
    CHECK(look.hatName == "kk_hats");
}

}
