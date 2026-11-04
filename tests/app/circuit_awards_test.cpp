#include "app/circuit_awards.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/menu/race_setup_menu.hpp"
#include "assets/archive_survey.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

using settings::Progress;
using settings::Trophy;
using sim::CircuitAward;

assets::CircuitEntry Circuit() {
    return {.name = "c3", .partSet = 2, .next = "c4"};
}

Progress Granted(CircuitAward award) {
    Progress progress;
    GrantAward(progress, progress.trophies, Circuit(), award);
    return progress;
}

TEST_CASE("gold wins a circuit's named part set once") {
    const assets::CircuitEntry lava{.name = "lava-cup", .namedPartSet = "lava-parts"};
    Progress progress;

    CHECK(GrantAward(progress, progress.trophies, lava, CircuitAward::Silver));
    CHECK(progress.namedPartSets.empty());
    CHECK(GrantAward(progress, progress.trophies, lava, CircuitAward::Gold));
    CHECK(progress.namedPartSets == std::set<std::string>{"lava-parts"});
    CHECK_FALSE(GrantAward(progress, progress.trophies, lava, CircuitAward::Gold));
}

TEST_CASE("a mod circuit is open once a later circuit in the story is") {
    const assets::CircuitEntry c0{.name = "c0", .next = "lava-cup"};
    const assets::CircuitEntry lava{.name = "lava-cup", .next = "c1", .isFromMod = true};
    const assets::CircuitEntry c1{.name = "c1", .next = "c2"};
    const assets::CircuitEntry c2{.name = "c2"};
    const Result<assets::RaceCatalog> races = assets::RaceCatalog::Create({c0, c1, c2, lava}, {});
    REQUIRE(races.has_value());

    CHECK(OpenCircuits(*races, {}).empty());
    CHECK(OpenCircuits(*races, {"c2"}) == std::set<std::string>{"c2", "lava-cup"});
    CHECK(OpenCircuits(*races, {"lava-cup"}) == std::set<std::string>{"lava-cup"});
}

TEST_CASE("gold opens the part set and next circuit, silver only the circuit") {
    CHECK(Granted(CircuitAward::Gold) == Progress{.unlockedCircuits = {"c4"},
                                                  .partSets = {2},
                                                  .trophies = {{"c3", Trophy::Gold}}});
    CHECK(Granted(CircuitAward::Silver) ==
          Progress{.unlockedCircuits = {"c4"}, .trophies = {{"c3", Trophy::Silver}}});
    CHECK(Granted(CircuitAward::Bronze) == Progress{.trophies = {{"c3", Trophy::Bronze}}});
    CHECK(Granted(CircuitAward::None) == Progress{});
}

TEST_CASE("only gold and silver open the next circuit, and only once") {
    CHECK(OpensNewCircuit({}, Circuit(), CircuitAward::Gold));
    CHECK(OpensNewCircuit({}, Circuit(), CircuitAward::Silver));
    CHECK_FALSE(OpensNewCircuit({}, Circuit(), CircuitAward::Bronze));
    CHECK_FALSE(OpensNewCircuit({}, Circuit(), CircuitAward::None));
    CHECK_FALSE(OpensNewCircuit(Granted(CircuitAward::Silver), Circuit(), CircuitAward::Gold));
    CHECK_FALSE(OpensNewCircuit({}, {.name = "c6"}, CircuitAward::Gold));
}

TEST_CASE("an award shows the player's grid driver and the boss in slot 1") {
    const assets::CircuitEntry circuit{.name = "c0", .grid = {"DA", "CR", "KK"}};

    CHECK(DriversOf(circuit, 2).player == "KK");
    CHECK(DriversOf(circuit, 2).champion == "CR");
    CHECK(DriversOf(circuit, 7).player.empty());
    CHECK(DriversOf({.name = "c9"}, 0).champion.empty());
    CHECK(DriversOf({.name = "lava-cup", .grid = {"VV", "LV"}, .boss = "MG"}, 0).champion == "MG");
}

TEST_CASE("an award that adds nothing leaves progress unchanged") {
    Progress progress = Granted(CircuitAward::Gold);

    CHECK_FALSE(GrantAward(progress, progress.trophies, Circuit(), CircuitAward::Silver));
    CHECK_FALSE(GrantAward(progress, progress.trophies, Circuit(), CircuitAward::Gold));
    CHECK_FALSE(GrantAward(progress, progress.trophies, Circuit(), CircuitAward::None));
    CHECK(progress == Granted(CircuitAward::Gold));
}

TEST_CASE("gold on the first retail circuit opens the second after a restart") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;

    REQUIRE(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Gold}).has_value());

    CHECK(settings::LoadProgress(file) == Progress{.unlockedCircuits = {"c1"},
                                                   .partSets = {0},
                                                   .trophies = {{"c0", Trophy::Gold}}});
    const Result<std::vector<CourseChoice>> circuits =
        LoadCircuitChoices(*vfs, ReadProgress(file).unlockedCircuits);
    REQUIRE(circuits.has_value());
    CHECK_FALSE(circuits->at(1).isLocked);
}

TEST_CASE("only a first gold unlocks the circuit's part set") {
    CHECK(NewPartSet({}, Circuit(), CircuitAward::Gold) == 2);
    CHECK_FALSE(NewPartSet({}, Circuit(), CircuitAward::Silver).has_value());
    CHECK_FALSE(NewPartSet(Granted(CircuitAward::Gold), Circuit(), CircuitAward::Gold).has_value());
    CHECK_FALSE(NewPartSet({}, {.name = "c3"}, CircuitAward::Gold).has_value());
}

TEST_CASE("saving an award reports each unlock only the first time") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;

    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Silver}) ==
          AwardUnlocks{.opensCircuit = true});
    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Gold}) ==
          AwardUnlocks{.partSet = 0});
    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Gold}) == AwardUnlocks{});
}

TEST_CASE("the selected racer takes the trophy and progress keeps the unlocks") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;
    const std::filesystem::path racers = folder.Path() / kRacersFileName;
    const settings::RacerStore store{.racers = {{.name = "A"}, {.name = "B"}}, .selected = 1};
    REQUIRE(settings::SaveRacers(store, racers).has_value());

    REQUIRE(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Silver}).has_value());

    CHECK(settings::LoadProgress(file) == Progress{.unlockedCircuits = {"c1"}});
    CHECK(settings::LoadRacers(racers) ==
          settings::RacerStore{
              .racers = {{.name = "A"}, {.name = "B", .trophies = {{"c0", Trophy::Silver}}}},
              .selected = 1});
}

TEST_CASE("a broken racers file leaves the trophy to progress") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;
    std::ofstream(folder.Path() / kRacersFileName) << "[[racers]\n";

    REQUIRE(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Bronze}).has_value());

    CHECK(settings::LoadProgress(file) == Progress{.trophies = {{"c0", Trophy::Bronze}}});
}

TEST_CASE("an award that changes nothing doesn't write the file") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;

    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::None}) == AwardUnlocks{});

    CHECK_FALSE(std::filesystem::exists(file));
    CHECK_FALSE(SaveAward(file, *vfs, {.circuit = "c9", .award = CircuitAward::Gold}).has_value());
}

}
}
