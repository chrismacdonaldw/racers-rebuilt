#include "settings/registry.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace rr::settings {
namespace {

constexpr SettingSpec<int> kFpsCap{
    .key = "video.fps_cap", .defaultValue = 144, .range = SettingRange<int>{.min = 30, .max = 360}};
constexpr SettingSpec<double> kVolume{.key = "audio.volume", .defaultValue = 1.0};
constexpr SettingSpec<bool> kVsync{.key = "video.vsync", .defaultValue = true};
constexpr SettingSpec<std::string_view> kGamePath{.key = "game.path", .defaultValue = ""};

Registry DeclaredRegistry() {
    Registry registry;
    registry.Declare(kFpsCap);
    registry.Declare(kVolume);
    registry.Declare(kVsync);
    registry.Declare(kGamePath);
    return registry;
}

TEST_CASE("declared settings start at their defaults") {
    const Registry registry = DeclaredRegistry();

    CHECK(registry.Get(kFpsCap) == 144);
    CHECK(registry.Get(kVsync));
    CHECK(registry.Get(kGamePath).empty());
}

TEST_CASE("values outside a setting's range are clamped") {
    Registry registry = DeclaredRegistry();

    registry.Set(kFpsCap, 1000);
    CHECK(registry.Get(kFpsCap) == 360);
    CHECK(registry.Assign("video.fps_cap", std::int64_t{5}).has_value());
    CHECK(registry.Get(kFpsCap) == 30);
}

TEST_CASE("a value of the wrong type is rejected and the old value kept") {
    Registry registry = DeclaredRegistry();

    CHECK_FALSE(registry.Assign("video.fps_cap", std::string("fast")).has_value());
    CHECK(registry.Get(kFpsCap) == 144);
}

TEST_CASE("a whole number is accepted for a fractional setting") {
    Registry registry = DeclaredRegistry();

    CHECK(registry.Assign("audio.volume", std::int64_t{0}).has_value());
    CHECK(registry.Get(kVolume) == 0.0);
}

TEST_CASE("text is parsed as the declared type") {
    Registry registry = DeclaredRegistry();

    CHECK(registry.AssignText("video.fps_cap", "240").has_value());
    CHECK(registry.AssignText("video.vsync", "false").has_value());
    CHECK(registry.AssignText("audio.volume", "0.5").has_value());
    CHECK(registry.AssignText("game.path", "C:/Games/LEGO Racers").has_value());

    CHECK(registry.Get(kFpsCap) == 240);
    CHECK_FALSE(registry.Get(kVsync));
    CHECK(registry.Get(kVolume) == 0.5);
    CHECK(registry.Get(kGamePath) == "C:/Games/LEGO Racers");
}

TEST_CASE("text that doesn't parse or names no setting is rejected") {
    Registry registry = DeclaredRegistry();

    CHECK_FALSE(registry.AssignText("video.fps_cap", "12abc").has_value());
    CHECK_FALSE(registry.AssignText("video.vsync", "yes").has_value());
    CHECK(registry.AssignText("video.missing", "1").error().code == ErrorCode::NotFound);
}

TEST_CASE("a value loaded before its declaration is converted on declare") {
    Registry registry;
    CHECK(registry.Assign("video.fps_cap", std::int64_t{999}).has_value());

    registry.Declare(kFpsCap);

    CHECK(registry.Get(kFpsCap) == 360);
}

TEST_CASE("observers hear about changes only") {
    Registry registry = DeclaredRegistry();
    std::vector<std::string> changed;
    registry.Observe([&](std::string_view key) { changed.emplace_back(key); });

    registry.Set(kFpsCap, 144);
    registry.Set(kFpsCap, 60);

    REQUIRE(changed.size() == 1);
    CHECK(changed[0] == "video.fps_cap");
}

}
}
