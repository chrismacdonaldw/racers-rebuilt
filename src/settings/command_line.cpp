#include "settings/command_line.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace rr::settings {
namespace {

constexpr std::string_view kSetFlag = "--set";

struct TextFlag {
    std::string_view flag;
    std::optional<std::string> CommandLine::* field;
};

constexpr std::array kTextFlags{
    TextFlag{.flag = "--game-path", .field = &CommandLine::gamePath},
    TextFlag{.flag = "--dump-images", .field = &CommandLine::imageDumpFolder},
    TextFlag{.flag = "--track", .field = &CommandLine::track},
    TextFlag{.flag = "--race", .field = &CommandLine::race},
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
