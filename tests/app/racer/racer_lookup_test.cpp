#include "app/racer/racer_lookup.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>

#include "assets/archive_survey.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("a racer in racers.toml wins over the game's racer of the same name") {
    const rr::test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "racers.toml";
    REQUIRE(settings::SaveRacers({.racers = {{.name = "Joan of Kart", .chassis = "rrchas0"}}}, file)
                .has_value());

    const Result<settings::Racer> racer = FindRacer(assets::Vfs{}, file, "JOAN OF KART");

    REQUIRE(racer.has_value());
    CHECK(racer->chassis == "rrchas0");
}

TEST_CASE("the selected racer comes from racers.toml, and none while nothing is selected") {
    const rr::test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "racers.toml";
    const Result<std::optional<settings::Racer>> missing = SelectedRacer(file);
    REQUIRE(missing.has_value());
    CHECK_FALSE(missing->has_value());
    REQUIRE(settings::SaveRacers(
                {.racers = {{.name = "ONE", .chassis = "a"}, {.name = "TWO", .chassis = "b"}},
                 .selected = 1},
                file)
                .has_value());

    const Result<std::optional<settings::Racer>> selected = SelectedRacer(file);

    REQUIRE(selected.has_value());
    CHECK(selected->value_or(settings::Racer{}).name == "TWO");
}

TEST_CASE("a racer nobody has fails") {
    const rr::test::TempFolder folder;

    CHECK_FALSE(FindRacer(assets::Vfs{}, folder.Path() / "racers.toml", "NOBODY").has_value());
}

TEST_CASE("the game's racers are found in any case") {
    const std::optional<assets::Vfs> vfs = rr::test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const rr::test::TempFolder folder;

    const Result<settings::Racer> racer = FindRacer(*vfs, folder.Path() / "racers.toml", "scooter");

    REQUIRE(racer.has_value());
    CHECK(racer->name == "SCOOTER");
    CHECK(racer->look ==
          assets::DriverLook{.face = 25, .hat = 21, .legs = 12, .torso = 21, .expression = 3});
    CHECK(racer->trophies.empty());
}

}
}
