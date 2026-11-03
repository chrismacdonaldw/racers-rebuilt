#include "app/gameplay_fingerprint.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "settings/racers.hpp"
#include "settings/registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("the gameplay fingerprint names each enabled mod and its version") {
    const test::TempFolder folder;
    for (const char* id : {"faster", "quiet"}) {
        std::filesystem::create_directories(folder.Path() / id);
        std::ofstream(folder.Path() / id / "mod.toml", std::ios::binary)
            << "id = \"" << id << "\"\nname = \"" << id << "\"\nversion = \"1.2\"\n";
    }
    const mods::ModList mods = mods::ModList::Discover(folder.Path());
    settings::Registry registry;
    mods.DeclareSettings(registry);
    registry.Set(mods.All().back().EnabledSetting(), false);

    CHECK(GameplayFingerprint(registry, mods, std::nullopt, {}) ==
          std::vector<std::string>{"mod:faster@1.2"});
}

TEST_CASE("the gameplay fingerprint records the overrides folder's hash") {
    CHECK(GameplayFingerprint(settings::Registry{}, mods::ModList{}, 0xABCULL, {}) ==
          std::vector<std::string>{"overrides:0000000000000abc"});
}

TEST_CASE("the gameplay fingerprint changes with the player's car but not their trophies") {
    const mods::ModList mods;
    const settings::Registry registry;
    PlayerRacers players{.first = settings::Racer{.name = "MINE", .chassis = "XXJMW"}};
    const std::vector<std::string> before =
        GameplayFingerprint(registry, mods, std::nullopt, players);
    players.first->trophies.emplace("c0", settings::Trophy::Gold);
    const std::vector<std::string> trophied =
        GameplayFingerprint(registry, mods, std::nullopt, players);
    players.first->car[0] = std::byte{1};
    const std::vector<std::string> rebuilt =
        GameplayFingerprint(registry, mods, std::nullopt, players);

    REQUIRE(before.size() == 1);
    CHECK(before.front().starts_with("racer:MINE@"));
    CHECK(trophied == before);
    CHECK(rebuilt != before);
}

TEST_CASE("the gameplay fingerprint names player 2's racer apart from player 1's") {
    const settings::Racer racer{.name = "MINE", .chassis = "XXJMW"};

    const std::vector<std::string> second =
        GameplayFingerprint(settings::Registry{}, mods::ModList{}, std::nullopt, {.second = racer});

    REQUIRE(second.size() == 1);
    CHECK(second.front().starts_with("racer2:MINE@"));
}

}
}
