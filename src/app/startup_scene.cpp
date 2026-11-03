#include "app/startup_scene.hpp"

#include <format>
#include <string>

#include "app/car_viewer_scene.hpp"
#include "app/cutscene/cutscene_scene.hpp"
#include "app/cutscene/cutscene_screen.hpp"
#include "app/race_scene_name.hpp"
#include "app/race_settings.hpp"

namespace rr::app {

std::string RaceNamed(RaceSceneSetup setup, const settings::Registry& settings) {
    setup.respawn = RespawnRuleFrom(settings);
    setup.aiDriving = AiDrivingFrom(settings);
    return RaceSceneName(setup);
}

Result<std::optional<std::string>> StartupScene(const settings::CommandLine& commandLine,
                                                const settings::Registry& settings) {
    if (commandLine.track && commandLine.race) {
        return Fail(ErrorCode::InvalidData, "--track and --race can't be used together");
    }
    if (commandLine.players && !commandLine.race) {
        return Fail(ErrorCode::InvalidData, "--players needs --race");
    }
    if (commandLine.shownCar) {
        if (commandLine.track || commandLine.race || commandLine.cutscene) {
            return Fail(ErrorCode::InvalidData, "--show-car can't be used with another scene");
        }
        return std::optional(std::format("{}{}", kCarViewerScenePrefix, *commandLine.shownCar));
    }
    if (commandLine.cutscene) {
        if (commandLine.track || commandLine.race) {
            return Fail(ErrorCode::InvalidData, "--cutscene can't be used with --track or --race");
        }
        return std::optional(std::format("{}{}", kCutsceneScenePrefix, *commandLine.cutscene));
    }
    if (commandLine.race) {
        return std::optional(RaceNamed({.track = *commandLine.race,
                                        .players = commandLine.players.value_or(1),
                                        .player = commandLine.racer.value_or(std::string())},
                                       settings));
    }
    return std::optional(
        commandLine.track.value_or(CutsceneScreenName({.menu = std::string(kLegalScreen)})));
}

}
