#include "settings/racers.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

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

assets::CarData Car() {
    assets::CarData car{};
    car[1] = std::byte{1};
    car[3] = std::byte{0x14};
    car[0x1FF] = std::byte{0xAB};
    return car;
}

RacerStore Garage() {
    return {.racers = {{.name = "JOAN OF KART",
                        .look = {.face = 3, .hat = 1, .legs = 2, .torso = 4, .expression = 5},
                        .chassis = "dachas0",
                        .car = Car(),
                        .trophies = {{"c0", Trophy::Gold}}},
                       {.name = "\xC3\x88VE"}},
            .selected = 1};
}

TEST_CASE("racers survive a save and load") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "racers.toml";

    REQUIRE(SaveRacers(Garage(), path).has_value());

    CHECK(LoadRacers(path) == Garage());
}

TEST_CASE("a saved racer reads as named fields and its car as hex") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "racers.toml";

    REQUIRE(SaveRacers({.racers = {{.name = "A", .car = Car()}}}, path).has_value());

    const std::string text = ReadText(path);
    CHECK(text.starts_with("# racers-rebuilt racers."));
    CHECK(text.find("[[racers]]") != std::string::npos);
    CHECK(text.find("car = '00010014" + std::string(std::size_t{0x1FB} * 2, '0') + "ab'") !=
          std::string::npos);
    CHECK(text.find("\nselected") == std::string::npos);
}

TEST_CASE("a missing racers file holds no racers") {
    const test::TempFolder folder;

    CHECK(LoadRacers(folder.Path() / "racers.toml") == RacerStore{});
}

constexpr std::string_view kHandWritten =
    "selected = 2\n"
    "[[racers]]\nname = \"A\"\nface = 1\nhat = 0\nlegs = 0\ntorso = 0\n"
    "expression = 0\nchassis = \"dachas0\"\ncar = \"0001\"\n"
    "[racers.trophies]\nc0 = \"platinum\"\nc1 = \"bronze\"\n";

TEST_CASE("hand-written racers skip the trophies and pick that don't parse") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "racers.toml";
    WriteText(path, kHandWritten);

    const Result<RacerStore> store = LoadRacers(path);

    REQUIRE(store.has_value());
    REQUIRE(store->racers.size() == 1);
    CHECK(store->racers[0].name == "A");
    CHECK(store->racers[0].car[1] == std::byte{1});
    CHECK(store->racers[0].trophies == Trophies{{"c1", Trophy::Bronze}});
    CHECK_FALSE(store->selected.has_value());
}

TEST_CASE("a racer that doesn't parse fails the whole file") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "racers.toml";
    const std::string racer =
        "[[racers]]\nname = \"B\"\nhat = 0\nlegs = 0\ntorso = 0\n"
        "expression = 0\nchassis = \"dachas0\"\n";
    for (const std::string_view broken :
         {"face = 300\ncar = \"\"\n", "face = 0\ncar = \"0g\"\n", "face = 0\ncar = 1\n"}) {
        WriteText(path, std::string(kHandWritten) + racer + std::string(broken));

        CHECK_FALSE(LoadRacers(path).has_value());
    }
    WriteText(path, "racers = 3\n");
    CHECK_FALSE(LoadRacers(path).has_value());
}

TEST_CASE("saving racers keeps hand-written comments and unknown keys, and drops a stale pick") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "racers.toml";
    WriteText(path, "# mine\n\nnote = \"hi\"\n");
    REQUIRE(SaveRacers(Garage(), path).has_value());

    REQUIRE(SaveRacers({.racers = Garage().racers}, path).has_value());

    const std::string text = ReadText(path);
    CHECK(text.starts_with("# mine\n\n"));
    CHECK(text.find("note = 'hi'") != std::string::npos);
    CHECK(LoadRacers(path) == RacerStore{.racers = Garage().racers});
}

TEST_CASE("a racers file that isn't TOML fails and isn't written over") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "racers.toml";
    WriteText(path, "[[racers]\n");

    CHECK_FALSE(LoadRacers(path).has_value());
    CHECK_FALSE(SaveRacers(Garage(), path).has_value());
    CHECK(ReadText(path) == "[[racers]\n");
}

TEST_CASE("trophies go to the selected racer, or to progress while none is selected") {
    Progress progress;
    RacerStore store = Garage();

    CHECK(&TrophiesOf(progress, store) == &store.racers[1].trophies);
    store.selected.reset();
    CHECK(&TrophiesOf(progress, store) == &progress.trophies);
}

}
}
