#include "app/startup_scene.hpp"

#include <format>
#include <utility>

#include "app/cutscene/cutscene_scene.hpp"
#include "app/cutscene/cutscene_screen.hpp"
#include "app/race_scene_name.hpp"
#include "app/race_settings.hpp"

namespace rr::app {

std::string RaceNamed(std::string track, std::size_t players, const settings::Registry& settings) {
    return RaceSceneName({.track = std::move(track),
                          .respawn = RespawnRuleFrom(settings),
                          .aiDriving = AiDrivingFrom(settings),
                          .players = players});
}

Result<std::optional<std::string>> StartupScene(const settings::CommandLine& commandLine,
                                                const settings::Registry& settings) {
    if (commandLine.track && commandLine.race) {
        return Fail(ErrorCode::InvalidData, "--track and --race can't be used together");
    }
    if (commandLine.players && !commandLine.race) {
        return Fail(ErrorCode::InvalidData, "--players needs --race");
    }
    if (commandLine.cutscene) {
        if (commandLine.track || commandLine.race) {
            return Fail(ErrorCode::InvalidData, "--cutscene can't be used with --track or --race");
        }
        return std::optional(std::format("{}{}", kCutsceneScenePrefix, *commandLine.cutscene));
    }
    if (commandLine.race) {
        return std::optional(
            RaceNamed(*commandLine.race, commandLine.players.value_or(1), settings));
    }
    return std::optional(
        commandLine.track.value_or(CutsceneScreenName({.menu = std::string(kLegalScreen)})));
}

}
