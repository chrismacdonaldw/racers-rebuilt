#include "sim/ai/route_choice.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"

namespace rr::sim {
namespace {

std::vector<assets::VfsEntry> Files(const std::vector<std::string>& names) {
    std::vector<assets::VfsEntry> entries;
    entries.reserve(names.size());
    for (const std::string& name : names) {
        entries.push_back({.name = name, .kind = assets::EntryKind::File});
    }
    return entries;
}

TEST_CASE("a slot's routes run fast, medium then slow, each up to its first gap") {
    const std::vector<assets::VfsEntry> folder =
        Files({"R2_S_0.RRB", "R2_F_0.RRB", "R2_F_1.RRB", "R2_F_3.RRB", "r2_m_0.rrb", "R1_F_0.RRB"});

    CHECK(SlotRoutes(folder, 2) ==
          std::vector<std::string>{"R2_F_0.RRB", "R2_F_1.RRB", "R2_M_0.RRB", "R2_S_0.RRB"});
}

TEST_CASE("a slot counts at most 10 routes of a pace and skips folders") {
    constexpr int kFiles = 12;
    std::vector<assets::VfsEntry> folder;
    folder.reserve(kFiles + 1);
    for (int number = 0; number < kFiles; ++number) {
        folder.push_back({.name = std::format("R4_M_{}.RRB", number)});
    }
    folder.push_back({.name = "R4_S_0.RRB", .kind = assets::EntryKind::Directory});

    const std::vector<std::string> routes = SlotRoutes(folder, 4);

    CHECK(routes.size() == 10);
    CHECK(routes.back() == "R4_M_9.RRB");
}

TEST_CASE("a slot with no routes gets none") {
    CHECK(SlotRoutes(Files({"R1_F_0.RRB"}), 0).empty());
}

std::vector<assets::VfsEntry> RetailFolder(const assets::Vfs& vfs) {
    Result<std::vector<assets::VfsEntry>> folder = vfs.List("GAMEDATA/RACEC0R0");
    REQUIRE(folder.has_value());
    return std::move(*folder);
}

TEST_CASE("retail slots keep their pace, and a numbering gap hides later routes") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<assets::VfsEntry> folder = RetailFolder(*vfs);

    CHECK(SlotRoutes(folder, 1).front().starts_with("R1_F_"));
    CHECK(SlotRoutes(folder, 2).front().starts_with("R2_M_"));
    const std::vector<std::string> third = SlotRoutes(folder, 3);
    CHECK(std::ranges::find(third, "R3_M_3.RRB") == third.end());
}

TEST_CASE("retail grid routes carry the pace in their file names") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<GridRoutes> routes = LoadGridRoutes(*vfs, "RACEC0R0", 6, false);

    REQUIRE(routes.has_value());
    REQUIRE((!routes->at(1).empty() && !routes->at(2).empty()));
    CHECK(routes->at(1).front().pace == assets::RoutePace::Fast);
    CHECK(routes->at(2).front().pace == assets::RoutePace::Medium);
}

TEST_CASE("a retail route loads mirrored for a mirrored race") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<assets::RaceRoute> route = LoadRoute(*vfs, "RACEC0R0", "R1_F_0.RRB", false);
    const Result<assets::RaceRoute> mirrored = LoadRoute(*vfs, "RACEC0R0", "R1_F_0.RRB", true);

    REQUIRE(route.has_value());
    REQUIRE(mirrored.has_value());
    CHECK(mirrored->startPosition == assets::Mirrored(*route).startPosition);
}

}
}
