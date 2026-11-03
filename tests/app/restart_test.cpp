#include "app/restart.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {
namespace {

constexpr settings::SettingSpec<bool> kModSetting{
    .key = "mods.hd.enabled", .defaultValue = true, .category = "Mods"};
constexpr settings::SettingSpec<bool> kVsync{
    .key = "video.vsync", .defaultValue = true, .category = "Video"};

settings::Registry Declared() {
    settings::Registry registry;
    registry.Declare(kModSetting);
    registry.Declare(kVsync);
    return registry;
}

TEST_CASE("a relaunch keeps the program and its arguments but drops one-shot work") {
    const std::array<std::string_view, 12> argv{
        "racers-rebuilt.exe", "--race", "RACEC0R0",     "--record",      "a.rpl", "--replay=b.rpl",
        "--players",          "2",      "--recordings", "--dump-images", "out",   "--import-save"};

    CHECK(RelaunchCommand(argv) == std::vector<std::string>{"racers-rebuilt.exe", "--race",
                                                            "RACEC0R0", "--players", "2",
                                                            "--recordings"});
}

TEST_CASE("only a Mods setting changed since startup counts as a change") {
    settings::Registry registry = Declared();
    const ModChanges changes(registry);

    registry.Set(kVsync, false);
    CHECK_FALSE(changes.Any(registry));

    registry.Set(kModSetting, false);
    CHECK(changes.Any(registry));

    registry.Set(kModSetting, true);
    CHECK_FALSE(changes.Any(registry));
}

TEST_CASE("restart is offered only once a Mods setting changes") {
    settings::Registry registry = Declared();
    const Restart restart({"racers-rebuilt.exe"}, registry);
    const Restart unknown({}, registry);
    bool isQuitting = false;

    CHECK_FALSE(restart.Choice(registry, isQuitting).has_value());
    registry.Set(kModSetting, false);

    CHECK(restart.Choice(registry, isQuitting).value_or(overlay::MenuChoice{}).name ==
          "Restart now");
    CHECK_FALSE(unknown.Choice(registry, isQuitting).has_value());
    CHECK_FALSE(isQuitting);
}

}
}
