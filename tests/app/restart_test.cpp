#include "app/restart.hpp"

#include <doctest/doctest.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/game_settings.hpp"
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
    registry.Declare(kGameVersion);
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

TEST_CASE("a game version change offers a restart in the Game menu but no live remount") {
    settings::Registry registry = Declared();
    const Restart restart({"racers-rebuilt.exe"}, registry);
    LiveRemount remount(true, registry);
    bool isQuitting = false;
    CHECK(GameActions(restart, registry, isQuitting).empty());

    registry.Set(kGameVersion, "2001");

    CHECK(GameActions(restart, registry, isQuitting).size() == 1);
    CHECK_FALSE(remount.Choice(registry).has_value());
}

std::vector<std::string> Names(std::span<const overlay::MenuChoice> choices) {
    std::vector<std::string> names;
    for (const overlay::MenuChoice& choice : choices) {
        names.push_back(choice.name);
    }
    return names;
}

TEST_CASE("applying mods in place is offered before restarting, and only asks once") {
    settings::Registry registry = Declared();
    const Restart restart({"racers-rebuilt.exe"}, registry);
    LiveRemount remount(true, registry);
    bool isQuitting = false;
    registry.Set(kModSetting, false);

    const std::vector<overlay::MenuChoice> actions =
        ModActions(remount, restart, registry, isQuitting);
    actions.front().choose();

    CHECK(Names(actions) == std::vector<std::string>{"Apply now", "Restart now"});
    CHECK(remount.TakeRequest());
    CHECK_FALSE(remount.TakeRequest());
    remount.Rebase(registry);
    CHECK_FALSE(remount.Choice(registry).has_value());
}

TEST_CASE("mods aren't applied in place while a replay records or plays") {
    settings::Registry registry = Declared();
    LiveRemount remount(false, registry);
    registry.Set(kModSetting, false);

    CHECK_FALSE(remount.Choice(registry).has_value());
}

}
}
