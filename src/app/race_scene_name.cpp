#include "app/race_scene_name.hpp"

#include <charconv>
#include <format>
#include <optional>
#include <ranges>
#include <system_error>

#include "assets/circuit_database.hpp"
#include "core/duration.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kRacePrefix = "race:";
constexpr std::string_view kRespawnKey = "respawn";
constexpr std::string_view kRacerKey = "racer";
constexpr std::string_view kAiKey = "ai";
constexpr std::string_view kRespawnOff = "off";
constexpr std::string_view kPhysicsAi = "physics";
constexpr std::string_view kRouteAi = "route";

template <typename T>
std::optional<T> ParseNumber(std::string_view text) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<sim::RespawnRule> ParseRespawn(std::string_view text) {
    if (text == kRespawnOff) {
        return sim::RespawnRule{.isEnabled = false};
    }
    const std::optional<Milliseconds::rep> timeout = ParseNumber<Milliseconds::rep>(text);
    if (!timeout || *timeout <= 0) {
        return std::nullopt;
    }
    return sim::RespawnRule{.timeout = Milliseconds{*timeout}};
}

bool ReadAiDriving(std::string_view text, sim::AiDriving& driving) {
    if (text != kPhysicsAi && text != kRouteAi) {
        return false;
    }
    driving = text == kPhysicsAi ? sim::AiDriving::Physics : sim::AiDriving::Route;
    return true;
}

bool ReadField(std::string_view field, RaceSceneSetup& setup,
               std::optional<sim::RespawnRule>& respawn) {
    const std::size_t equals = field.find('=');
    const std::string_view key = field.substr(0, equals);
    const std::string_view value =
        equals == std::string_view::npos ? std::string_view{} : field.substr(equals + 1);
    if (key == kRespawnKey) {
        respawn = ParseRespawn(value);
        return respawn.has_value();
    }
    if (key == kRacerKey) {
        setup.racer = ParseNumber<std::size_t>(value).value_or(assets::kGridSlots);
        return setup.racer < assets::kGridSlots;
    }
    if (key == kAiKey) {
        return ReadAiDriving(value, setup.aiDriving);
    }
    return false;
}

}

std::string RaceSceneName(const RaceSceneSetup& setup) {
    const std::string respawn = setup.respawn.isEnabled
                                    ? std::to_string(setup.respawn.timeout.count())
                                    : std::string(kRespawnOff);
    std::string name = std::format("{}{}:{}={}:{}={}", kRacePrefix, setup.track, kRespawnKey,
                                   respawn, kRacerKey, setup.racer);
    if (setup.aiDriving == sim::AiDriving::Physics) {
        name += std::format(":{}={}", kAiKey, kPhysicsAi);
    }
    return name;
}

bool IsRaceScene(std::string_view scene) {
    return scene.starts_with(kRacePrefix);
}

Result<RaceSceneSetup> ParseRaceScene(std::string_view scene) {
    RaceSceneSetup setup;
    std::optional<sim::RespawnRule> respawn;
    bool isValid = IsRaceScene(scene);
    std::string_view rest = isValid ? scene.substr(kRacePrefix.size()) : std::string_view{};
    const std::size_t trackEnd = rest.find(':');
    setup.track = rest.substr(0, trackEnd);
    rest = trackEnd == std::string_view::npos ? std::string_view{} : rest.substr(trackEnd + 1);
    for (const auto field : std::views::split(rest, ':')) {
        isValid = isValid && ReadField(std::string_view(field), setup, respawn);
    }
    if (!isValid || setup.track.empty() || !respawn) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't a race scene", scene));
    }
    setup.respawn = *respawn;
    return setup;
}

}
