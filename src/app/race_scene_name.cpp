#include "app/race_scene_name.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>

#include "assets/circuit_database.hpp"
#include "assets/game_path.hpp"
#include "core/duration.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kRacePrefix = "race:";
constexpr std::string_view kCircuitPrefix = "circuit:";
constexpr std::string_view kTrialPrefix = "trial:";
constexpr std::string_view kRespawnKey = "respawn";
constexpr std::string_view kRacerKey = "racer";
constexpr std::string_view kCircuitKey = "circuit";
constexpr std::string_view kAiKey = "ai";
constexpr std::string_view kMirroredKey = "mirrored";
constexpr std::string_view kPlayersKey = "players";
constexpr std::string_view kSecondRacerKey = "racer2";
constexpr std::string_view kLapsKey = "laps";
constexpr int kMaxLaps = 9;
constexpr std::string_view kOpponentsKey = "opponents";
constexpr std::string_view kRaceKey = "race";
constexpr std::string_view kPointsKey = "points";
constexpr std::string_view kPlayerKey = "player";
constexpr std::string_view kHexDigits = "0123456789ABCDEF";
constexpr int kHexBase = 16;
constexpr char kHashMark = '@';
constexpr char kChainMark = '>';
constexpr unsigned char kFirstPlain = 0x21;
constexpr unsigned char kLastPlain = 0x7E;
constexpr std::string_view kRespawnOff = "off";
constexpr std::string_view kPhysicsAi = "physics";
constexpr std::string_view kRouteAi = "route";

bool IsPlain(char c) {
    const auto byte = static_cast<unsigned char>(c);
    return byte >= kFirstPlain && byte <= kLastPlain && c != ':' && c != kChainMark && c != '%' &&
           c != kHashMark;
}

std::string Escaped(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (IsPlain(c)) {
            out += c;
        } else {
            const auto byte = static_cast<unsigned char>(c);
            out += '%';
            out += kHexDigits[byte / kHexBase];
            out += kHexDigits[byte % kHexBase];
        }
    }
    return out;
}

std::optional<std::string> Unescaped(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t at = 0; at < text.size(); ++at) {
        if (text[at] != '%') {
            out += text[at];
            continue;
        }
        unsigned int byte = 0;
        const std::string_view digits = text.substr(at + 1, 2);
        const auto [end, error] =
            std::from_chars(digits.data(), digits.data() + digits.size(), byte, kHexBase);
        if (digits.size() != 2 || error != std::errc() || end != digits.data() + digits.size()) {
            return std::nullopt;
        }
        out += static_cast<char>(byte);
        at += 2;
    }
    return out;
}

std::optional<RacerRef> ParseRef(std::string_view text) {
    const std::size_t mark = text.find(kHashMark);
    std::optional<std::string> name = Unescaped(text.substr(0, mark));
    if (!name || name->empty()) {
        return std::nullopt;
    }
    if (mark == std::string_view::npos) {
        return RacerRef{.name = std::move(*name)};
    }
    const std::string_view digits = text.substr(mark + 1);
    std::uint64_t hash = 0;
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(), hash, kHexBase);
    if (digits.empty() || error != std::errc() || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return RacerRef{.name = std::move(*name), .hash = hash};
}

std::string RefText(const RacerRef& ref) {
    return ref.hash ? std::format("{}{}{:016x}", Escaped(ref.name), kHashMark, *ref.hash)
                    : Escaped(ref.name);
}

struct Field {
    std::string_view key;
    std::string_view value;
};

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

bool ReadPoints(std::string_view text, sim::CircuitStanding& standing) {
    std::size_t slot = 0;
    for (const auto part : std::views::split(text, ',')) {
        const std::optional<std::int32_t> points =
            ParseNumber<std::int32_t>(std::string_view(part));
        if (!points || *points < 0 || slot == standing.points.size()) {
            return false;
        }
        standing.points[slot++] = *points;
    }
    return slot == standing.points.size();
}

bool ReadStanding(Field field, sim::CircuitStanding& standing) {
    if (field.key == kRaceKey) {
        standing.race = ParseNumber<std::size_t>(field.value).value_or(assets::kCircuitRaces);
        return standing.race < assets::kCircuitRaces;
    }
    return field.key == kPointsKey && ReadPoints(field.value, standing);
}

bool ReadRaceField(Field field, RaceSceneSetup& setup) {
    if (field.key == kPlayersKey) {
        setup.players = ParseNumber<std::size_t>(field.value).value_or(0);
        return setup.players >= 1 && setup.players <= sim::kMaxPlayers;
    }
    if (field.key == kSecondRacerKey) {
        setup.secondRacer = ParseNumber<std::size_t>(field.value).value_or(assets::kGridSlots);
        return setup.secondRacer < assets::kGridSlots;
    }
    if (field.key == kLapsKey) {
        setup.lapCount = ParseNumber<int>(field.value).value_or(0);
        return setup.lapCount >= 1 && setup.lapCount <= kMaxLaps;
    }
    if (field.key == kOpponentsKey) {
        setup.opponents = ParseNumber<std::size_t>(field.value).value_or(assets::kGridSlots);
        return setup.opponents < assets::kGridSlots;
    }
    return false;
}

template <typename ReadOther>
bool ReadField(Field field, RaceSceneSetup& setup, std::optional<sim::RespawnRule>& respawn,
               ReadOther readOther) {
    if (field.key == kRespawnKey) {
        respawn = ParseRespawn(field.value);
        return respawn.has_value();
    }
    if (field.key == kRacerKey) {
        setup.racer = ParseNumber<std::size_t>(field.value).value_or(assets::kGridSlots);
        return setup.racer < assets::kGridSlots;
    }
    if (field.key == kCircuitKey) {
        setup.circuit = field.value;
        return !field.value.empty();
    }
    if (field.key == kAiKey) {
        return ReadAiDriving(field.value, setup.aiDriving);
    }
    if (field.key == kPlayerKey) {
        setup.player = ParseRef(field.value).value_or(RacerRef{});
        return !setup.player.name.empty();
    }
    if (field.key == kMirroredKey) {
        setup.isMirrored = true;
        return field.value.empty();
    }
    return readOther(field, setup);
}

Field SplitField(std::string_view text) {
    const std::size_t equals = text.find('=');
    return {
        .key = text.substr(0, equals),
        .value = equals == std::string_view::npos ? std::string_view{} : text.substr(equals + 1)};
}

template <typename ReadOther>
Result<RaceSceneSetup> ParseScene(std::string_view scene, std::string_view prefix,
                                  std::string RaceSceneSetup::* head, ReadOther readOther) {
    RaceSceneSetup setup;
    std::optional<sim::RespawnRule> respawn;
    bool isValid = scene.starts_with(prefix);
    std::string_view rest = isValid ? scene.substr(prefix.size()) : std::string_view{};
    const std::size_t headEnd = rest.find(':');
    const std::string_view headText = rest.substr(0, headEnd);
    rest = headEnd == std::string_view::npos ? std::string_view{} : rest.substr(headEnd + 1);
    for (const auto field : std::views::split(rest, ':')) {
        isValid =
            isValid && ReadField(SplitField(std::string_view(field)), setup, respawn, readOther);
    }
    if (!isValid || headText.empty() || !respawn) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't a {} scene", scene,
                                                        prefix.substr(0, prefix.size() - 1)));
    }
    setup.*head = headText;
    setup.respawn = *respawn;
    return setup;
}

std::string RulesFields(const RaceSceneSetup& setup) {
    const std::string respawn = setup.respawn.isEnabled
                                    ? std::to_string(setup.respawn.timeout.count())
                                    : std::string(kRespawnOff);
    std::string fields = std::format(":{}={}:{}={}", kRespawnKey, respawn, kRacerKey, setup.racer);
    if (!setup.player.name.empty()) {
        fields += std::format(":{}={}", kPlayerKey, RefText(setup.player));
    }
    return fields;
}

std::string AiField(const RaceSceneSetup& setup) {
    return setup.aiDriving == sim::AiDriving::Physics ? std::format(":{}={}", kAiKey, kPhysicsAi)
                                                      : std::string();
}

}

std::string RaceSceneName(const RaceSceneSetup& setup) {
    if (setup.isTimeTrial) {
        return std::format("{}{}{}", kTrialPrefix, setup.track, RulesFields(setup));
    }
    std::string name = std::format("{}{}{}", kRacePrefix, setup.track, RulesFields(setup));
    if (!setup.circuit.empty()) {
        name += std::format(":{}={}", kCircuitKey, setup.circuit);
    }
    if (setup.isMirrored) {
        name += std::format(":{}", kMirroredKey);
    }
    if (setup.players > 1) {
        name += std::format(":{}={}:{}={}", kPlayersKey, setup.players, kSecondRacerKey,
                            setup.secondRacer);
    }
    if (setup.lapCount != kDefaultLapCount) {
        name += std::format(":{}={}", kLapsKey, setup.lapCount);
    }
    if (setup.opponents != kDefaultOpponentCount) {
        name += std::format(":{}={}", kOpponentsKey, setup.opponents);
    }
    return name + AiField(setup);
}

std::size_t RacerCount(const RaceSceneSetup& setup, std::size_t gridSize) {
    if (setup.players > 1) {
        return setup.players;
    }
    return std::min(gridSize, setup.players + setup.opponents);
}

bool IsRaceScene(std::string_view scene) {
    return scene.starts_with(kRacePrefix);
}

namespace {

std::string NamingPlayer(std::string_view scene, const RacerRef& player) {
    if (IsCircuitScene(scene)) {
        Result<CircuitSceneSetup> circuit = ParseCircuitScene(scene);
        if (circuit) {
            circuit->race.player = player;
            return CircuitSceneName(*circuit);
        }
    } else if (IsRaceScene(scene) || IsTrialScene(scene)) {
        Result<RaceSceneSetup> race =
            IsTrialScene(scene) ? ParseTrialScene(scene) : ParseRaceScene(scene);
        if (race) {
            race->player = player;
            return RaceSceneName(*race);
        }
    }
    return std::string(scene);
}

}

RacerRef ScenePlayer(std::string_view scene) {
    if (IsCircuitScene(scene)) {
        const Result<CircuitSceneSetup> circuit = ParseCircuitScene(scene);
        return circuit ? circuit->race.player : RacerRef{};
    }
    const Result<RaceSceneSetup> race =
        IsTrialScene(scene) ? ParseTrialScene(scene) : ParseRaceScene(scene);
    return race ? race->player : RacerRef{};
}

std::string WithPlayer(std::string_view scenes, const RacerRef& player) {
    std::string out;
    for (const auto part : std::views::split(scenes, kChainMark)) {
        if (!out.empty()) {
            out += kChainMark;
        }
        out += NamingPlayer(std::string_view(part), player);
    }
    return out;
}

Result<RaceSceneSetup> ParseRaceScene(std::string_view scene) {
    return ParseScene(scene, kRacePrefix, &RaceSceneSetup::track, ReadRaceField);
}

bool IsTrialScene(std::string_view scene) {
    return scene.starts_with(kTrialPrefix);
}

Result<RaceSceneSetup> ParseTrialScene(std::string_view scene) {
    return ParseScene(scene, kTrialPrefix, &RaceSceneSetup::track,
                      [](Field, RaceSceneSetup&) { return false; })
        .transform([](RaceSceneSetup setup) {
            setup.track = assets::ToUpperAscii(setup.track);
            setup.isTimeTrial = true;
            setup.opponents = 0;
            return setup;
        });
}

std::string CircuitSceneName(const CircuitSceneSetup& setup) {
    const std::array<std::int32_t, assets::kGridSlots>& points = setup.standing.points;
    return std::format("{}{}{}:{}={}:{}={},{},{},{},{},{}{}", kCircuitPrefix, setup.race.circuit,
                       RulesFields(setup.race), kRaceKey, setup.standing.race, kPointsKey,
                       points[0], points[1], points[2], points[3], points[4], points[5],
                       AiField(setup.race));
}

bool IsCircuitScene(std::string_view scene) {
    return scene.starts_with(kCircuitPrefix);
}

Result<CircuitSceneSetup> ParseCircuitScene(std::string_view scene) {
    sim::CircuitStanding standing;
    return ParseScene(scene, kCircuitPrefix, &RaceSceneSetup::circuit,
                      [&](Field field, RaceSceneSetup&) { return ReadStanding(field, standing); })
        .transform([&](RaceSceneSetup race) {
            return CircuitSceneSetup{.race = std::move(race), .standing = standing};
        });
}

}
