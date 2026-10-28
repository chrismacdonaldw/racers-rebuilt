#include "settings/settings_file.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <string>

#include "temp_folder.hpp"

namespace rr::settings {
namespace {

constexpr SettingSpec<int> kFpsCap{
    .key = "video.fps_cap", .defaultValue = 144, .range = SettingRange<int>{.min = 30, .max = 360}};
constexpr SettingSpec<std::string_view> kGamePath{.key = "game.path", .defaultValue = ""};

Registry DeclaredRegistry() {
    Registry registry;
    registry.Declare(kFpsCap);
    registry.Declare(kGamePath);
    return registry;
}

void WriteText(const std::filesystem::path& path, std::string_view text) {
    std::ofstream(path, std::ios::binary) << text;
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST_CASE("settings survive a save and load") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "settings.toml";
    Registry saved = DeclaredRegistry();
    saved.Set(kFpsCap, 240);
    saved.Set(kGamePath, R"(C:\Games\LEGO Racers)");

    REQUIRE(SaveSettings(saved, path).has_value());
    Registry loaded = DeclaredRegistry();
    REQUIRE(LoadSettings(loaded, path).has_value());

    CHECK(loaded.Get(kFpsCap) == 240);
    CHECK(loaded.Get(kGamePath) == R"(C:\Games\LEGO Racers)");
}

TEST_CASE("unknown keys are kept when settings are saved") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "settings.toml";
    WriteText(path, "[future]\nfeature = \"on\"\nlevel = 3\n");
    Registry registry = DeclaredRegistry();

    REQUIRE(LoadSettings(registry, path).has_value());
    REQUIRE(SaveSettings(registry, path).has_value());

    const std::string text = ReadText(path);
    CHECK(text.find("feature = 'on'") != std::string::npos);
    CHECK(text.find("level = 3") != std::string::npos);
}

TEST_CASE("loaded values are clamped and mistyped values keep the default") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "settings.toml";
    WriteText(path, "[video]\nfps_cap = 9000\n[game]\npath = 12\n");
    Registry registry = DeclaredRegistry();

    REQUIRE(LoadSettings(registry, path).has_value());

    CHECK(registry.Get(kFpsCap) == 360);
    CHECK(registry.Get(kGamePath).empty());
}

TEST_CASE("a missing settings file loads nothing") {
    const test::TempFolder folder;
    Registry registry = DeclaredRegistry();

    CHECK(LoadSettings(registry, folder.Path() / "settings.toml").has_value());
    CHECK(registry.Get(kFpsCap) == 144);
}

TEST_CASE("malformed TOML fails with its line number") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "settings.toml";
    WriteText(path, "[video]\nfps_cap = = 3\n");
    Registry registry = DeclaredRegistry();

    Result<void> loaded = LoadSettings(registry, path);

    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().message.find("line 2") != std::string::npos);
}

}
}
