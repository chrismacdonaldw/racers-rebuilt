#include "settings/command_line.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <system_error>
#include <utility>

namespace rr::settings {
namespace {

constexpr std::string_view kSetFlag = "--set";
constexpr std::string_view kPlayersFlag = "--players";
constexpr std::string_view kTrackExportFlag = "--export-track";
struct SwitchFlag {
    std::string_view flag;
    bool CommandLine::* field;
};

constexpr std::array kSwitchFlags{
    SwitchFlag{.flag = "--novideo", .field = &CommandLine::noVideo},
    SwitchFlag{.flag = "--check-mods", .field = &CommandLine::checkMods},
};

struct TextFlag {
    std::string_view flag;
    std::optional<std::string> CommandLine::* field;
};

constexpr std::array kTextFlags{
    TextFlag{.flag = "--game-path", .field = &CommandLine::gamePath},
    TextFlag{.flag = "--dump-images", .field = &CommandLine::imageDumpFolder},
    TextFlag{.flag = "--dump-textures", .field = &CommandLine::texturePackFolder},
    TextFlag{.flag = "--export-gltf", .field = &CommandLine::gltfExportFolder},
    TextFlag{.flag = "--dump-powerups", .field = &CommandLine::powerupDumpFile},
    TextFlag{.flag = "--track", .field = &CommandLine::track},
    TextFlag{.flag = "--race", .field = &CommandLine::race},
    TextFlag{.flag = "--cutscene", .field = &CommandLine::cutscene},
    TextFlag{.flag = "--show-car", .field = &CommandLine::shownCar},
    TextFlag{.flag = "--racer", .field = &CommandLine::racer},
    TextFlag{.flag = "--cheats", .field = &CommandLine::cheats},
    TextFlag{.flag = "--record", .field = &CommandLine::recordFile},
    TextFlag{.flag = "--replay", .field = &CommandLine::replayFile},
    TextFlag{.flag = "--import-save", .field = &CommandLine::importSaveFile},
};

struct FlagValue {
    std::string_view flag;
    std::string_view value;
};

Result<FlagValue> TakeFlag(std::span<const std::string_view> args, std::size_t& index) {
    const std::string_view arg = args[index];
    const std::size_t equals = arg.find('=');
    if (equals != std::string_view::npos) {
        return FlagValue{.flag = arg.substr(0, equals), .value = arg.substr(equals + 1)};
    }
    if (index + 1 >= args.size()) {
        return Fail(ErrorCode::InvalidData, std::format("{} needs a value", arg));
    }
    ++index;
    return FlagValue{.flag = arg, .value = args[index]};
}

Result<TrackExport> TakeTrackExport(std::span<const std::string_view> args, std::size_t& index) {
    if (index + 2 >= args.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} needs a track and a folder", kTrackExportFlag));
    }
    index += 2;
    return TrackExport{.track = std::string(args[index - 1]), .folder = std::string(args[index])};
}

Result<std::size_t> ParsePlayers(std::string_view text) {
    std::size_t players = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), players);
    if (error != std::errc() || end != text.data() + text.size() || players == 0) {
        return Fail(ErrorCode::InvalidData,
                    std::format("--players expects a player count, got \"{}\"", text));
    }
    return players;
}

Result<SettingOverride> ParseOverride(std::string_view text) {
    const std::size_t equals = text.find('=');
    if (equals == std::string_view::npos || equals == 0) {
        return Fail(ErrorCode::InvalidData,
                    std::format("--set expects key=value, got \"{}\"", text));
    }
    return SettingOverride{.key = std::string(text.substr(0, equals)),
                           .value = std::string(text.substr(equals + 1))};
}

}

Result<CommandLine> ParseCommandLine(std::span<const std::string_view> args) {
    CommandLine commandLine;
    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto switchFlag = std::ranges::find(kSwitchFlags, args[index], &SwitchFlag::flag);
        if (switchFlag != kSwitchFlags.end()) {
            commandLine.*(switchFlag->field) = true;
            continue;
        }
        if (args[index] == kTrackExportFlag) {
            Result<TrackExport> trackExport = TakeTrackExport(args, index);
            if (!trackExport) {
                return std::unexpected(std::move(trackExport.error()));
            }
            commandLine.trackExport = std::move(*trackExport);
            continue;
        }
        Result<FlagValue> flag = TakeFlag(args, index);
        if (!flag) {
            return std::unexpected(std::move(flag.error()));
        }
        const auto text = std::ranges::find(kTextFlags, flag->flag, &TextFlag::flag);
        if (text != kTextFlags.end()) {
            commandLine.*(text->field) = std::string(flag->value);
        } else if (flag->flag == kSetFlag) {
            Result<SettingOverride> setting = ParseOverride(flag->value);
            if (!setting) {
                return std::unexpected(std::move(setting.error()));
            }
            commandLine.overrides.push_back(std::move(*setting));
        } else if (flag->flag == kPlayersFlag) {
            Result<std::size_t> players = ParsePlayers(flag->value);
            if (!players) {
                return std::unexpected(std::move(players.error()));
            }
            commandLine.players = *players;
        } else {
            return Fail(ErrorCode::InvalidData, std::format("unknown option {}", flag->flag));
        }
    }
    return commandLine;
}

Result<void> ApplyOverrides(Registry& registry, std::span<const SettingOverride> overrides) {
    for (const SettingOverride& setting : overrides) {
        Result<void> assigned = registry.AssignText(setting.key, setting.value);
        if (!assigned) {
            return Fail(assigned.error().code,
                        std::format("--set {}: {}", setting.key, assigned.error().message));
        }
    }
    return {};
}

}
