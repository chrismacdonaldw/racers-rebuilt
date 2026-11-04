#include "assets/driver_table.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rr::assets {
namespace {

std::vector<DriverPatch> Parse(std::string_view text) {
    Result<std::vector<DriverPatch>> patches = ParseDriverTable(text, "DRIVERS.TOML");
    REQUIRE_MESSAGE(patches.has_value(), patches.error().message);
    return std::move(*patches);
}

std::string ErrorFor(std::string_view text) {
    const Result<std::vector<DriverPatch>> patches = ParseDriverTable(text, "DRIVERS.TOML");
    REQUIRE_FALSE(patches.has_value());
    return patches.error().message;
}

std::vector<DriverEntry> Retail() {
    return {{.name = "CR", .nameIndex = 3, .champion = "CRCAR", .voiceBank = 2},
            {.name = "KK", .nameIndex = 4, .champion = "KKCAR", .voiceBank = 5}};
}

TEST_CASE("a driver table reads every key") {
    CHECK(Parse("[[driver]]\ncode = \"mg\"\nname = \"Magma Max\"\nracer = \"MAGMA MAX\"\n"
                "champion = \"KKCAR\"\nvoice = \"KK\"\n"
                "use_chances = { red = 200, green = 255 }\n"
                "charge = { colour = \"green\", level = 3 }\nstart_boost = true\n") ==
          std::vector<DriverPatch>{
              {.code = "MG",
               .name = "Magma Max",
               .racer = "MAGMA MAX",
               .champion = "KKCAR",
               .voice = "KK",
               .useChances = {std::uint8_t{200}, std::nullopt, std::uint8_t{255}, std::nullopt},
               .charge = DriverCharge{.colour = BrickColour::Green, .level = 3},
               .startBoost = true}});
    CHECK(Parse("[[driver]]\ncode = \"CR\"\ncharge = { colour = \"none\" }\n").front().charge ==
          DriverCharge{});
}

TEST_CASE("a driver table rejects bad codes, chances and charges") {
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {"[[driver]]\nname = \"Magma Max\"\n", "DRIVERS.TOML: driver 1: code must name the driver"},
        {"[[driver]]\ncode = \"MG\"\nuse_chances = { red = 300 }\n",
         "DRIVERS.TOML: driver 1: use_chances: red must be a whole number from 0 to 255"},
        {"[[driver]]\ncode = \"MG\"\ncharge = { colour = \"white\" }\n",
         "DRIVERS.TOML: driver 1: charge: colour must be red, blue, green, yellow or none"},
        {"[[driver]]\ncode = \"MG\"\ncharge = { colour = \"red\", level = 4 }\n",
         "DRIVERS.TOML: driver 1: charge: level must be a whole number from 0 to 3"},
        {"[[driver]]\ncode = \"MG\"\nstart_boost = 1\n",
         "DRIVERS.TOML: driver 1: start_boost must be true or false"},
        {"driver = 3\n", "DRIVERS.TOML: driver must be [[driver]] tables"}};

    for (const auto& [text, error] : cases) {
        CAPTURE(text);
        CHECK(ErrorFor(text) == error);
    }
}

TEST_CASE("a driver table adds a driver that races as a game racer") {
    std::vector<DriverEntry> drivers = Retail();

    REQUIRE(ApplyDriverPatches(
                std::vector<DriverPatch>{
                    {.code = "MG", .name = "Magma Max", .racer = "MAGMA MAX", .voice = "kk"}},
                drivers)
                .empty());

    REQUIRE(drivers.size() == 3);
    const DriverEntry& added = drivers.back();
    CHECK(added.name + added.title + added.racer == "MGMagma MaxMAGMA MAX");
    CHECK(added.nameIndex == -1);
    CHECK(added.voiceBank == 5);
}

TEST_CASE("a driver table changes a driver's habits in place") {
    std::vector<DriverEntry> drivers = Retail();

    REQUIRE(ApplyDriverPatches(
                std::vector<DriverPatch>{
                    {.code = "CR",
                     .useChances = {std::nullopt, std::uint8_t{60}, std::nullopt, std::nullopt},
                     .charge = DriverCharge{.colour = BrickColour::Yellow, .level = 2},
                     .startBoost = false}},
                drivers)
                .empty());

    const DriverEntry& cr = drivers.front();
    CHECK(cr.useChances[static_cast<std::size_t>(BrickColour::Blue)] == 60);
    CHECK((cr.aiChargeColor * 10) + cr.aiWhiteBrickTarget == 42);
    CHECK(cr.startBoost == false);
    CHECK(cr.champion == "CRCAR");
}

TEST_CASE("a driver table change that can't apply is skipped with its reason") {
    std::vector<DriverEntry> drivers = Retail();

    const std::vector<std::string> problems =
        ApplyDriverPatches(std::vector<DriverPatch>{{.code = "MG", .name = "Magma Max"},
                                                    {.code = "CR", .name = "Crash", .voice = "ZZ"}},
                           drivers);

    CHECK(problems ==
          std::vector<std::string>{
              "GAMEDATA/COMMON/DRIVERS.TOML: driver MG: a new driver needs name and racer",
              "GAMEDATA/COMMON/DRIVERS.TOML: driver CR: voice names no driver ZZ"});
    CHECK(drivers.size() == 2);
    CHECK(drivers.front().title.empty());
}

}
}
