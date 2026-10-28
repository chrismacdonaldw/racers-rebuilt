#include "app/startup_settings.hpp"

#include <doctest/doctest.h>

#include <array>
#include <fstream>
#include <iterator>
#include <string>

#include "app/game_settings.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::array<std::string_view, 1> kChangedKeys{kGamePath.key};

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST_CASE("command-line overrides apply for the session but aren't saved") {
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "settings.toml";
    const std::array overrides{
        settings::SettingOverride{.key = "game.language", .value = "GERMAN"}};

    Result<settings::Registry> registry = LoadStartupSettings(file, overrides, Extensions{});
    REQUIRE(registry.has_value());
    CHECK(registry->Get(kGameLanguage) == "GERMAN");
    registry->Set(kGamePath, "D:/Racers");
    PersistSettings(file, *registry, kChangedKeys);

    const std::string saved = ReadText(file);
    CHECK(saved.find("D:/Racers") != std::string::npos);
    CHECK(saved.find("GERMAN") == std::string::npos);
}

TEST_CASE("a settings file that can't be parsed is never overwritten") {
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "settings.toml";
    std::ofstream(file, std::ios::binary) << "[game\n";

    Result<settings::Registry> registry = LoadStartupSettings(file, {}, Extensions{});
    REQUIRE(registry.has_value());
    registry->Set(kGamePath, "D:/Racers");
    PersistSettings(file, *registry, kChangedKeys);

    CHECK(ReadText(file) == "[game\n");
}

TEST_CASE("an override for an unknown setting stops startup") {
    const test::TempFolder folder;
    const std::array overrides{settings::SettingOverride{.key = "video.missing", .value = "1"}};

    CHECK_FALSE(
        LoadStartupSettings(folder.Path() / "settings.toml", overrides, Extensions{}).has_value());
}

}
}
