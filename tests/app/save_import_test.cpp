#include "app/save_import.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/save_bytes.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

using settings::BestTime;
using settings::Progress;
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

TEST_CASE("an original save adds its circuits, part sets, records, times and best trophies") {
    Progress progress;

    CHECK(MergeOriginalSave(progress, Parsed(Played()), Catalog()));

    CHECK(progress ==
          Progress{.unlockedCircuits = {"c2"},
                   .partSets = {1},
                   .beatenRecords = {0, 3},
                   .trophies = {{"c0", Trophy::Gold}, {"c2", Trophy::Bronze}},
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

TEST_CASE("an original save keeps the better of each held time and trophy") {
    Progress progress{.trophies = {{"c2", Trophy::Gold}},
                      .bestTimes = {{"RACEC0R0",
                                     {.lap = BestTime{.ms = 30000, .holder = "ME"},
                                      .race = BestTime{.ms = 99000, .holder = "ME"}}}}};
    MergeOriginalSave(progress, Parsed(Played()), Catalog());

    CHECK(progress.trophies.at("c2") == Trophy::Gold);
    CHECK(progress.bestTimes.at("RACEC0R0").lap == BestTime{.ms = 30000, .holder = "ME"});
    CHECK(progress.bestTimes.at("RACEC0R0").race == BestTime{.ms = 98000, .holder = "\xC3\x88ve"});
    CHECK_FALSE(MergeOriginalSave(progress, Parsed(Played()), Catalog()));
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

    CHECK(settings::LoadProgress(progress) ==
          Progress{.unlockedCircuits = {"c1"}, .trophies = {{"c0", Trophy::Gold}}});
    CHECK_FALSE(ImportOriginalSave(folder.Path() / "missing", progress, *vfs).has_value());
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
