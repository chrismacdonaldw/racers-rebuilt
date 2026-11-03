#include <doctest/doctest.h>

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/car_edit.hpp"
#include "app/menu/edit_car_menu.hpp"
#include "sim/car_build/car_pieces.hpp"

namespace rr::app {
namespace {

using sim::test::Block;

constexpr std::size_t kRaceSet = 10;

struct Pieces {
    assets::PieceCatalog catalog = Catalog();

    static assets::PieceCatalog Catalog() {
        assets::PieceCatalog catalog;
        assets::PieceRecord starter = sim::test::Chassis();
        starter.name = "ddchas0";
        assets::PieceRecord racer = Block(18, 10, 6, 1);
        racer.name = "rrchas0";
        catalog.library.pieces = {starter, racer, Block(0x900, 1, 1, 3)};
        catalog.partSets.resize(kPartSetCount,
                                {.chassisName = "ddchas0", .choices = {{.piece = 2}}});
        catalog.partSets.at(1).chassisName = "nochas";
        catalog.partSets.at(kRaceSet).chassisName = "RRCHAS0";
        return catalog;
    }

    [[nodiscard]] const assets::PieceRecord& Brick() const { return catalog.library.pieces.at(2); }
};

assets::SavedRacer Build(std::string chassis, std::byte mark) {
    assets::SavedRacer build{.chassis = std::move(chassis)};
    build.car.front() = mark;
    return build;
}

std::vector<std::byte> Picks(QuickBuilds& builds, std::string_view chassis, int count) {
    std::vector<std::byte> picked;
    for (int pick = 0; pick < count; ++pick) {
        const assets::SavedRacer* build = builds.Next(chassis);
        picked.push_back(build == nullptr ? std::byte{0xFF} : build->car.front());
    }
    return picked;
}

sim::InputFrame Pressed(sim::MenuAction action) {
    sim::InputFrame input;
    input.menuPressed.set(sim::InputFrame::Index(action));
    return input;
}

}

TEST_CASE_FIXTURE(Pieces, "picking a set leaves only that set's chassis") {
    sim::BuiltCar car;
    REQUIRE(ResetToChassis(car, catalog, 0, 3).has_value());
    REQUIRE(car.Place(Brick(), {.x = 2, .y = 1}, 7, 6).has_value());
    REQUIRE(car.Pieces().size() == 2);

    REQUIRE(ResetToChassis(car, catalog, kRaceSet, 5).has_value());

    REQUIRE(car.Pieces().size() == 1);
    CHECK(car.Pieces().front().piece->name == "rrchas0");
    CHECK(car.Pieces().front().colour == 5);
}

TEST_CASE_FIXTURE(Pieces, "a car that doesn't load starts empty") {
    assets::CarData data{};
    data.front() = std::byte{0x41};

    CHECK(LoadEditedCar(data, catalog.library).Pieces().empty());
}

TEST_CASE_FIXTURE(Pieces, "a set whose chassis isn't in the library keeps the car") {
    sim::BuiltCar car;
    REQUIRE(ResetToChassis(car, catalog, 0, 3).has_value());
    REQUIRE(car.Place(Brick(), {.x = 2, .y = 1}, 7, 6).has_value());

    CHECK_FALSE(ResetToChassis(car, catalog, 1, 3).has_value());
    CHECK_FALSE(ResetToChassis(car, catalog, kPartSetCount, 3).has_value());
    CHECK(car.Pieces().size() == 2);
}

TEST_CASE_FIXTURE(Pieces, "the starter sets, unlocked sets and the car's own sets are available") {
    sim::BuiltCar car;
    REQUIRE(ResetToChassis(car, catalog, 0, 3).has_value());

    catalog.partSets.at(6).chassis = 2;
    CHECK(AvailablePartSets({}, car, catalog) == std::vector<std::size_t>{0, 1, 2, 3});
    CHECK(AvailablePartSets({0, 7, 8, -1}, car, catalog) ==
          std::vector<std::size_t>{0, 1, 2, 3, 4, 11});

    REQUIRE(car.Place(Brick(), {.x = 2, .y = 1}, 7, Brick().type).has_value());
    CHECK(AvailablePartSets({0}, car, catalog) == std::vector<std::size_t>{0, 1, 2, 3, 4, 6});

    catalog.partSets.at(1).choices.clear();
    CHECK(AvailablePartSets({0}, car, catalog) == std::vector<std::size_t>{0, 2, 3, 4, 6});
}

TEST_CASE_FIXTURE(Pieces, "a mod's sets are always there and show their chassis set's icon") {
    catalog.partSets.at(kRaceSet).chassis = 1;
    catalog.partSets.push_back(catalog.partSets.at(kRaceSet));
    const sim::BuiltCar car;

    const std::vector<std::size_t> sets = AvailablePartSets({}, car, catalog);

    CHECK(sets == std::vector<std::size_t>{0, 1, 2, 3, kPartSetCount});
    CHECK(SetIcons(catalog, sets) == std::vector<std::size_t>{0, 1, 2, 3, kRaceSet});
}

TEST_CASE("quick build cycles exactly the builds on the chassis, in any case") {
    QuickBuilds builds({Build("ddchas0", std::byte{0}), Build("rrchas0", std::byte{1}),
                        Build("DDCHAS0", std::byte{2}), Build("bbchas0", std::byte{3}),
                        Build("ddchas0", std::byte{4})});

    CHECK(Picks(builds, "ddchas0", 4) ==
          std::vector{std::byte{0}, std::byte{2}, std::byte{4}, std::byte{0}});
    CHECK(Picks(builds, "RRCHAS0", 1) == std::vector{std::byte{1}});
    CHECK(builds.Next("nochas") == nullptr);
}

TEST_CASE("a car that matches a quick build counts as one") {
    const QuickBuilds builds({Build("ddchas0", std::byte{7})});

    CHECK(builds.Contains(Build("other", std::byte{7}).car));
    CHECK_FALSE(builds.Contains(Build("ddchas0", std::byte{8}).car));
}

TEST_CASE("the car menu opens on BUILD and skips rows that are off") {
    EditCarMenu menu;
    CHECK(menu.Focus() == CarRow::Build);

    menu.Enable(CarRow::RemoveBricks, false);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Down)) == CarRequest::None);
    CHECK(menu.Focus() == CarRow::QuickBuild);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == CarRequest::QuickBuild);

    menu.Enable(CarRow::QuickBuild, false);
    CHECK(menu.Focus() == CarRow::Done);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Back)) == CarRequest::Leave);
}

TEST_CASE("left and right step the set only while the picker is on") {
    EditCarMenu menu;
    static_cast<void>(menu.Tick(Pressed(sim::MenuAction::Up)));
    REQUIRE(menu.Focus() == CarRow::PartSet);

    CHECK(menu.Tick(Pressed(sim::MenuAction::Left)) == CarRequest::PreviousSet);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Right)) == CarRequest::NextSet);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == CarRequest::None);

    menu.Enable(CarRow::Build, false);
    menu.Enable(CarRow::PartSet, false);
    CHECK(menu.Focus() == CarRow::RemoveBricks);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Left)) == CarRequest::None);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == CarRequest::RemoveBricks);
}

TEST_CASE("a row turned back on can be chosen again") {
    EditCarMenu menu;
    menu.Enable(CarRow::Build, false);
    menu.Enable(CarRow::Build, true);
    CHECK(menu.Focus() == CarRow::RemoveBricks);

    static_cast<void>(menu.Tick(Pressed(sim::MenuAction::Up)));
    CHECK(menu.Focus() == CarRow::Build);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == CarRequest::Build);
}

TEST_CASE("pointing at a row that's off leaves the focus") {
    EditCarMenu menu;
    menu.Enable(CarRow::RemoveBricks, false);

    CHECK_FALSE(menu.FocusOn(CarRow::RemoveBricks));
    CHECK(menu.Focus() == CarRow::Build);
    CHECK(menu.FocusOn(CarRow::Done));
    CHECK(menu.Focus() == CarRow::Done);
}

}
