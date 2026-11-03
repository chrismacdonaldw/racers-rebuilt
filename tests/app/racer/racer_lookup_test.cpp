#include "app/racer/racer_lookup.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/folder_layer.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("a racer in racers.toml wins over the game's racer of the same name") {
    const rr::test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "racers.toml";
    REQUIRE(settings::SaveRacers({.racers = {{.name = "Joan of Kart", .chassis = "rrchas0"}}}, file)
                .has_value());

    const Result<settings::Racer> racer = FindRacer(assets::Vfs{}, file, {.name = "JOAN OF KART"});

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

    CHECK_FALSE(
        FindRacer(assets::Vfs{}, folder.Path() / "racers.toml", {.name = "NOBODY"}).has_value());
}

TEST_CASE("a racers.toml that won't load leaves the game's racers") {
    const std::optional<assets::Vfs> vfs = rr::test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const rr::test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "racers.toml";
    std::ofstream(file) << "[[racers\n";

    const Result<settings::Racer> racer = FindRacer(*vfs, file, {.name = "SCOOTER"});

    REQUIRE(racer.has_value());
    CHECK(racer->name == "SCOOTER");
}

TEST_CASE("the game's racers are found in any case") {
    const std::optional<assets::Vfs> vfs = rr::test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const rr::test::TempFolder folder;

    const Result<settings::Racer> racer =
        FindRacer(*vfs, folder.Path() / "racers.toml", {.name = "scooter"});

    REQUIRE(racer.has_value());
    CHECK(racer->name == "SCOOTER");
    CHECK(racer->look ==
          assets::DriverLook{.face = 25, .hat = 21, .legs = 12, .torso = 21, .expression = 3});
    CHECK(racer->trophies.empty());
}

void MountLayer(assets::Vfs& vfs, const std::filesystem::path& folder) {
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder);
    REQUIRE(layer.has_value());
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
}

void MountModRacers(assets::Vfs& vfs, const std::filesystem::path& mod,
                    const settings::RacerStore& racers) {
    std::filesystem::create_directories(mod / "MENUDATA");
    REQUIRE(settings::SaveRacers(racers, mod / "MENUDATA" / "RACERS.TOML").has_value());
    MountLayer(vfs, mod);
}

std::optional<assets::Vfs> RetailWithModRacers(const std::filesystem::path& mods) {
    std::optional<assets::Vfs> vfs = rr::test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    MountModRacers(
        *vfs, mods / "one",
        {.racers = {{.name = "scooter", .chassis = "a"}, {.name = "NEW", .chassis = "b"}}});
    MountModRacers(*vfs, mods / "two", {.racers = {{.name = "New", .chassis = "c"}}});
    std::filesystem::create_directories(mods / "broken" / "MENUDATA");
    std::ofstream(mods / "broken" / "MENUDATA" / "RACERS.TOML") << "[[racers\n";
    MountLayer(*vfs, mods / "broken");
    return vfs;
}

TEST_CASE("mod racers join the game's racers, replacing one of the same name") {
    const rr::test::TempFolder folder;
    const std::optional<assets::Vfs> vfs = RetailWithModRacers(folder.Path());
    if (!vfs) {
        return;
    }

    const Result<std::vector<settings::Racer>> racers = ReadDefaultRacers(*vfs);

    REQUIRE(racers.has_value());
    CHECK(racers->size() == 5);
    CHECK(racers->back().name == "New");
    CHECK(racers->back().chassis == "c");
}

TEST_CASE("mod racers are found by name like the game's") {
    const rr::test::TempFolder folder;
    const std::optional<assets::Vfs> vfs = RetailWithModRacers(folder.Path());
    if (!vfs) {
        return;
    }

    CHECK(FindGameRacer(*vfs, {.name = "SCOOTER"}).value().chassis == "a");
    CHECK(FindGameRacer(*vfs, {.name = "new"}).value().chassis == "c");
}

}
}
