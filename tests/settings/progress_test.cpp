#include "settings/progress.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <string>

#include "temp_folder.hpp"

namespace rr::settings {
namespace {

void WriteText(const std::filesystem::path& path, std::string_view text) {
    std::ofstream(path, std::ios::binary) << text;
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

Progress Played() {
    return {.unlockedCircuits = {"c1", "c2"},
            .partSets = {0, 1},
            .beatenRecords = {3},
            .trophies = {{"c0", Trophy::Gold}, {"c1", Trophy::Bronze}},
            .bestTimes = {{"RACEC0R1",
                           {.lap = BestTime{.ms = 31250, .holder = "KING KAHUKA"},
                            .race = BestTime{.ms = 98000, .holder = "ROCKET RACER"}}},
                          {"RACEC1R0", {.race = BestTime{.ms = 101000}}}}};
}

TEST_CASE("progress survives a save and load") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "progress.toml";

    REQUIRE(SaveProgress(Played(), path).has_value());

    CHECK(LoadProgress(path) == Played());
}

TEST_CASE("the saved progress file reads as commented names") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "progress.toml";

    REQUIRE(SaveProgress(Played(), path).has_value());

    const std::string text = ReadText(path);
    CHECK(text.starts_with("# racers-rebuilt progress."));
    CHECK(text.find("unlocked_circuits = [ 'c1', 'c2' ]") != std::string::npos);
    CHECK(text.find("c0 = 'gold'") != std::string::npos);
}

TEST_CASE("a missing progress file is a fresh game") {
    const test::TempFolder folder;

    CHECK(LoadProgress(folder.Path() / "progress.toml") == Progress{});
}

TEST_CASE("hand-written progress skips the values that don't parse") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "progress.toml";
    WriteText(path,
              "unlocked_circuits = [\"c1\", 4]\n"
              "part_sets = \"all\"\n"
              "[trophies]\nc0 = \"platinum\"\nc1 = \"silver\"\n"
              "[best_times.RACEC0R1]\nlap = { ms = -5 }\nrace = { ms = 90000 }\n");

    const Result<Progress> progress = LoadProgress(path);

    REQUIRE(progress.has_value());
    CHECK(progress->unlockedCircuits == std::set<std::string>{"c1"});
    CHECK(progress->partSets.empty());
    CHECK(progress->trophies == Trophies{{"c1", Trophy::Silver}});
    CHECK(progress->bestTimes.at("RACEC0R1") == TrackRecords{.race = BestTime{.ms = 90000}});
}

TEST_CASE("saving progress keeps hand-written comments and unknown keys") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "progress.toml";
    WriteText(path, "# mine\n\nunlocked_circuits = [\"c1\"]\nnote = \"hi\"\n");

    REQUIRE(SaveProgress(Played(), path).has_value());
    REQUIRE(SaveProgress(Played(), path).has_value());

    const std::string text = ReadText(path);
    CHECK(text.starts_with("# mine\n\n"));
    CHECK(text.find("note = 'hi'") != std::string::npos);
    CHECK(LoadProgress(path) == Played());
}

TEST_CASE("a progress file that isn't TOML fails") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "progress.toml";
    WriteText(path, "unlocked_circuits = [\n");

    CHECK_FALSE(LoadProgress(path).has_value());
}

TEST_CASE("a trophy only ever improves") {
    Trophies trophies;

    CHECK(AwardTrophy(trophies, "c0", Trophy::Silver));
    CHECK_FALSE(AwardTrophy(trophies, "c0", Trophy::Bronze));
    CHECK_FALSE(AwardTrophy(trophies, "c0", Trophy::Silver));
    CHECK(AwardTrophy(trophies, "c0", Trophy::Gold));
    CHECK(trophies.at("c0") == Trophy::Gold);
}

}
}
