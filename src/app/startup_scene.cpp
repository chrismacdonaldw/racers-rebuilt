#include "app/startup_scene.hpp"

#include <utility>

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
    return commandLine.race ? std::optional(RaceNamed(*commandLine.race,
                                                      commandLine.players.value_or(1), settings))
                            : commandLine.track;
}

}
