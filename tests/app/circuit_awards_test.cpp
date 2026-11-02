#include "app/circuit_awards.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
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
    GrantAward(progress, Circuit(), award);
    return progress;
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

TEST_CASE("an award that adds nothing leaves progress unchanged") {
    Progress progress = Granted(CircuitAward::Gold);

    CHECK_FALSE(GrantAward(progress, Circuit(), CircuitAward::Silver));
    CHECK_FALSE(GrantAward(progress, Circuit(), CircuitAward::Gold));
    CHECK_FALSE(GrantAward(progress, Circuit(), CircuitAward::None));
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
        LoadCircuitChoices(*vfs, UnlockedCircuits(file));
    REQUIRE(circuits.has_value());
    CHECK_FALSE(circuits->at(1).isLocked);
}

TEST_CASE("saving an award reports a new circuit only the first time it opens") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;

    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Silver}) == true);
    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::Gold}) == false);
}

TEST_CASE("an award that changes nothing doesn't write the file") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;

    CHECK(SaveAward(file, *vfs, {.circuit = "c0", .award = CircuitAward::None}) == false);

    CHECK_FALSE(std::filesystem::exists(file));
    CHECK_FALSE(SaveAward(file, *vfs, {.circuit = "c9", .award = CircuitAward::Gold}).has_value());
}

}
}
