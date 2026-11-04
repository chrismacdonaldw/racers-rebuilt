#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "settings/registry.hpp"

namespace rr::settings {

struct SettingOverride {
    std::string key;
    std::string value;
};

struct CommandLine {
    std::optional<std::string> gamePath;
    std::optional<std::string> imageDumpFolder;
    std::optional<std::string> texturePackFolder;
    std::optional<std::string> gltfExportFolder;
    std::optional<std::string> powerupDumpFile;
    std::optional<std::string> track;
    std::optional<std::string> race;
    std::optional<std::size_t> players;
    std::optional<std::string> cutscene;
    std::optional<std::string> shownCar;
    std::optional<std::string> racer;
    std::optional<std::string> cheats;
    std::optional<std::string> recordFile;
    std::optional<std::string> replayFile;
    std::optional<std::string> importSaveFile;
    std::vector<SettingOverride> overrides;
    bool noVideo = false;
    bool checkMods = false;
};

/// Parses the flags the README lists, valued ones also as --flag=value; args excludes argv[0].
[[nodiscard]] Result<CommandLine> ParseCommandLine(std::span<const std::string_view> args);

[[nodiscard]] Result<void> ApplyOverrides(Registry& registry,
                                          std::span<const SettingOverride> overrides);

}
