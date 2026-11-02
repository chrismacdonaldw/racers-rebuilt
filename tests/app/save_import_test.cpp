#include "app/save_import.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "app/circuit_awards.hpp"
#include "assets/archive_survey.hpp"
#include "assets/save_bytes.hpp"
#include "game_files.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

using settings::BestTime;
using settings::Progress;
using settings::Racer;
using settings::RacerStore;
using settings::Trophy;

constexpr std::size_t kFirstScore = 0x009;
constexpr std::size_t kScoreSize = 0x40;
constexpr std::size_t kRaceTime = 0x20;

assets::RaceCatalog Catalog() {
    Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Create(
        {{.name = "c0", .grid = {"KK"}},
         {.name = "c1", .grid = {"KK"}},
         {.name = "c2", .grid = {"KK"}}},
        {{.name = "a", .folder = "racec0r0", .circuit = "c0"},
         {.name = "b", .folder = "racec0r1", .circuit = "c1", .isMirrored = true}});
    REQUIRE(catalog.has_value());
    return std::move(*catalog);
}

assets::OriginalSave Parsed(const test::SaveBytes& bytes) {
    Result<assets::OriginalSave> save = assets::ParseOriginalSave(bytes.File());
    REQUIRE(save.has_value());
    return std::move(*save);
}

test::SaveBytes Played() {
    test::SaveBytes save;
    save.Byte(0x005, 0b10).Byte(0x006, 0b101).U16(0x007, 0xF009);
    save.Time(kFirstScore, 31250, u"KING KAHUKA").Time(kFirstScore + kRaceTime, 98000, u"\u00C8ve");
    save.Time(kFirstScore + kScoreSize, 1000, u"MIRROR");
    save.Driver(0b11'00'10).Driver(0b00'00'01);
    return save;
}

TEST_CASE("an original save adds its circuits, part sets, records and times") {
    Progress progress;

    CHECK(MergeOriginalSave(progress, Parsed(Played()), Catalog()));

    CHECK(progress ==
          Progress{.unlockedCircuits = {"c2"},
                   .partSets = {1},
                   .beatenRecords = {0, 3},
                   .bestTimes = {{"RACEC0R0",
                                  {.lap = BestTime{.ms = 31250, .holder = "KING KAHUKA"},
                                   .race = BestTime{.ms = 98000, .holder = "\xC3\x88ve"}}}}});
}

TEST_CASE("a holder's surrogate pairs and stray surrogates survive a save and load") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "progress.toml";
    const std::u16string holder{u'A', 0xD83D, 0xDE00, 0xD800, u'B', 0xDC00};
    Progress progress;
    MergeOriginalSave(progress, Parsed(test::SaveBytes().Time(kFirstScore, 500, holder)),
                      Catalog());

    REQUIRE(settings::SaveProgress(progress, path).has_value());

    const Result<Progress> loaded = settings::LoadProgress(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->bestTimes.at("RACEC0R0").lap == BestTime{.ms = 500,
                                                           .holder = "A\xF0\x9F\x98\x80\xEF\xBF\xBD"
                                                                     "B\xEF\xBF\xBD"});
}

TEST_CASE("an original save keeps the better of each held time") {
    Progress progress{.bestTimes = {{"RACEC0R0",
                                     {.lap = BestTime{.ms = 30000, .holder = "ME"},
                                      .race = BestTime{.ms = 99000, .holder = "ME"}}}}};
    MergeOriginalSave(progress, Parsed(Played()), Catalog());

    CHECK(progress.bestTimes.at("RACEC0R0").lap == BestTime{.ms = 30000, .holder = "ME"});
    CHECK(progress.bestTimes.at("RACEC0R0").race == BestTime{.ms = 98000, .holder = "\xC3\x88ve"});
    CHECK_FALSE(MergeOriginalSave(progress, Parsed(Played()), Catalog()));
}

TEST_CASE("an original save adds its racers with their trophies, skipping exact copies") {
    const assets::OriginalSave save =
        Parsed(test::SaveBytes().Driver(0b11'00'10).Racer(u"\u00C8VE", {1, 2, 3, 4, 5}).Driver(0));
    const Racer eve{.name = "\xC3\x88VE",
                    .look = {.face = 1, .hat = 2, .legs = 3, .torso = 4, .expression = 5},
                    .trophies = {{"c0", Trophy::Silver}, {"c2", Trophy::Bronze}}};
    RacerStore store{.racers = {Racer{}}};

    CHECK(MergeSavedRacers(store, save.racers, Catalog()));

    CHECK(store.racers == std::vector<Racer>{Racer{}, eve});
    CHECK_FALSE(MergeSavedRacers(store, save.racers, Catalog()));
}

void WriteFile(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

TEST_CASE("importing a retail-shaped save writes progress once") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path save = folder.Path() / "LEGORac1";
    const std::filesystem::path progress = folder.Path() / "progress.toml";
    WriteFile(save, test::SaveBytes().Byte(0x006, 0b10).Driver(0b01).File());

    CHECK(ImportOriginalSave(save, progress, *vfs) == true);
    CHECK(ImportOriginalSave(save, progress, *vfs) == false);

    CHECK(settings::LoadProgress(progress) == Progress{.unlockedCircuits = {"c1"}});
    CHECK(settings::LoadRacers(folder.Path() / kRacersFileName) ==
          RacerStore{.racers = {{.trophies = {{"c0", Trophy::Gold}}}}});
}

TEST_CASE("importing a missing save fails") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;

    CHECK_FALSE(
        ImportOriginalSave(folder.Path() / "missing", folder.Path() / kProgressFileName, *vfs)
            .has_value());
}

Racer OnlyRacer(const std::filesystem::path& folder) {
    const Result<RacerStore> racers = settings::LoadRacers(folder / kRacersFileName);
    REQUIRE(racers.has_value());
    REQUIRE(racers->racers.size() == 1);
    return racers->racers.front();
}

TEST_CASE("importing the first retail save adds its racer and their car") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    const std::optional<std::filesystem::path> game = test::GameFolder();
    if (!vfs || !game) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path progress = folder.Path() / kProgressFileName;

    CHECK(ImportOriginalSave(*game / "Save/0/LEGORac1", progress, *vfs) == true);
    CHECK(ImportOriginalSave(*game / "Save/0/LEGORac1", progress, *vfs) == false);

    const Racer racer = OnlyRacer(folder.Path());
    CHECK(racer == Racer{.name = "OLDGAMESDOWNL",
                         .look = {.face = 14, .hat = 0, .legs = 5, .torso = 7},
                         .chassis = "ddchas0",
                         .car = racer.car});
    CHECK(racer.car != assets::CarData{});
}

TEST_CASE("original saves are found by folder number") {
    const test::TempFolder folder;
    for (const char* name : {"10", "2", "x", "0"}) {
        WriteFile(folder.Path() / "Save" / name / "LEGORac1", {});
    }
    std::filesystem::create_directories(folder.Path() / "Save" / "3");

    CHECK(FindOriginalSaves(folder.Path()) ==
          std::vector<std::filesystem::path>{folder.Path() / "Save" / "0" / "LEGORac1",
                                             folder.Path() / "Save" / "2" / "LEGORac1",
                                             folder.Path() / "Save" / "10" / "LEGORac1"});
    CHECK(FindOriginalSaves(folder.Path() / "missing").empty());
}

}
}
