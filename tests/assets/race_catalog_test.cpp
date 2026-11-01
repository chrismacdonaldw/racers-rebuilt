#include "assets/race_catalog.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"

namespace rr::assets {
namespace {

CircuitEntry Circuit(std::string name, std::string boss) {
    return {.name = std::move(name), .grid = {"KK", std::move(boss)}};
}

RaceEntry Race(std::string folder, std::string circuit, std::int32_t position,
               bool isMirrored = false) {
    return {.name = folder,
            .folder = std::move(folder),
            .circuit = std::move(circuit),
            .position = position,
            .isMirrored = isMirrored};
}

RaceCatalog TwoCircuits() {
    Result<RaceCatalog> catalog = RaceCatalog::Create(
        {Circuit("c0", "CR"), Circuit("c1", "KK")},
        {Race("racec1r0", "c0", 1), Race("racec0r0", "c1", 1), Race("racec1r0", "c1", 0, true),
         Race("racec0r1", "c0", 0), RaceEntry{.name = "test", .folder = "test"}});
    REQUIRE(catalog.has_value());
    return std::move(*catalog);
}

TEST_CASE("races fill their circuit's slots by position") {
    const RaceCatalog catalog = TwoCircuits();

    CHECK(catalog.RacesOf(0) == CircuitRaces{3, 0, std::nullopt, std::nullopt});
    CHECK(catalog.RacesOf(1) == CircuitRaces{2, 1, std::nullopt, std::nullopt});
    CHECK(catalog.FindCircuit("c1") == 1);
    CHECK_FALSE(catalog.FindCircuit("c9").has_value());
}

TEST_CASE("a track's home circuit is the first that races it unmirrored") {
    const RaceCatalog catalog = TwoCircuits();

    CHECK(catalog.HomeCircuit("RACEC1R0") == 0);
    CHECK(catalog.HomeCircuit("racec0r0") == 1);
    CHECK_FALSE(catalog.HomeCircuit("TEST").has_value());
}

TEST_CASE("a grid comes from the named circuit, or the track's home circuit") {
    const RaceCatalog catalog = TwoCircuits();

    CHECK(catalog.Grid("RACEC0R0", "").value_or(std::vector<std::string>{}).at(1) == "KK");
    CHECK(catalog.Grid("RACEC0R0", "c0").value_or(std::vector<std::string>{}).at(1) == "CR");
    CHECK_FALSE(catalog.Grid("TEST", "").has_value());
    CHECK_FALSE(catalog.Grid("RACEC0R0", "c9").has_value());
}

TEST_CASE("a race on a missing circuit fails") {
    CHECK_FALSE(
        RaceCatalog::Create({Circuit("c0", "CR")}, {Race("racec0r1", "c5", 0)}).has_value());
}

TEST_CASE("two races at one circuit position fail") {
    CHECK_FALSE(RaceCatalog::Create({Circuit("c0", "CR")},
                                    {Race("racec0r1", "c0", 2), Race("racec0r3", "c0", 2)})
                    .has_value());
}

std::vector<std::string> SlotFolders(const RaceCatalog& catalog, std::string_view circuit) {
    std::vector<std::string> folders;
    for (const std::optional<std::size_t>& race : catalog.RacesOf(*catalog.FindCircuit(circuit))) {
        const RaceEntry* entry = race ? &catalog.Races()[*race] : nullptr;
        folders.push_back(entry == nullptr ? std::string()
                                           : entry->folder + (entry->isMirrored ? " M" : ""));
    }
    return folders;
}

std::optional<RaceCatalog> RetailCatalog() {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    Result<RaceCatalog> catalog = RaceCatalog::Load(*vfs);
    REQUIRE(catalog.has_value());
    REQUIRE(catalog->Circuits().size() == 7);
    return std::move(*catalog);
}

TEST_CASE("the retail circuits hold the original's races") {
    const std::optional<RaceCatalog> catalog = RetailCatalog();
    if (!catalog) {
        return;
    }

    CHECK(SlotFolders(*catalog, "c0") ==
          std::vector<std::string>{"racec0r1", "racec1r0", "racec0r3", "racec0r2"});
    CHECK(SlotFolders(*catalog, "c1") ==
          std::vector<std::string>{"racec1r1", "racec0r0", "racec1r3", "racec1r2"});
    CHECK(SlotFolders(*catalog, "c2") ==
          std::vector<std::string>{"racec2r0", "racec2r1", "racec2r2", "racec2r3"});
    CHECK(SlotFolders(*catalog, "c6") == std::vector<std::string>{"racec3r0", "", "", ""});
}

TEST_CASE("the retail mirrored circuits replay the first three in reverse") {
    const std::optional<RaceCatalog> catalog = RetailCatalog();
    if (!catalog) {
        return;
    }

    CHECK(SlotFolders(*catalog, "c3") ==
          std::vector<std::string>{"racec0r2 M", "racec0r3 M", "racec1r0 M", "racec0r1 M"});
    CHECK(SlotFolders(*catalog, "c4") ==
          std::vector<std::string>{"racec1r2 M", "racec1r3 M", "racec0r0 M", "racec1r1 M"});
    CHECK(SlotFolders(*catalog, "c5") ==
          std::vector<std::string>{"racec2r3 M", "racec2r2 M", "racec2r1 M", "racec2r0 M"});
}

TEST_CASE("a retail track's home circuit isn't always the one its folder names") {
    const std::optional<RaceCatalog> catalog = RetailCatalog();
    if (!catalog) {
        return;
    }

    CHECK(catalog->HomeCircuit("RACEC0R0") == catalog->FindCircuit("c1"));
    CHECK(catalog->Grid("RACEC0R0", "").value_or(std::vector<std::string>{}).at(1) == "KK");
}

}
}
