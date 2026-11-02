#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "core/result.hpp"

namespace rr::settings {

inline constexpr std::size_t kRecordRaces = 12;

enum class Trophy : std::uint8_t { Gold, Silver, Bronze };

struct BestTime {
    std::int64_t ms = 0;
    std::string holder;

    friend bool operator==(const BestTime&, const BestTime&) = default;
};

struct TrackRecords {
    std::optional<BestTime> lap;
    std::optional<BestTime> race;

    friend bool operator==(const TrackRecords&, const TrackRecords&) = default;
};

/// Circuits are CRB names and tracks are race folders.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct Progress {
    std::set<std::string> unlockedCircuits;
    std::set<std::int32_t> partSets;
    std::set<std::int32_t> beatenRecords;
    std::map<std::string, Trophy> trophies;
    std::map<std::string, TrackRecords> bestTimes;

    friend bool operator==(const Progress&, const Progress&) = default;
};

/// Keeps whichever of the held and new trophy is better. Returns whether it changed.
bool AwardTrophy(Progress& progress, const std::string& circuit, Trophy trophy);

/// A missing file is a fresh game. Values that don't parse are logged and skipped.
[[nodiscard]] Result<Progress> LoadProgress(const std::filesystem::path& path);

/// Keeps the file's leading comments and unknown keys. Fails on a file that isn't TOML.
[[nodiscard]] Result<void> SaveProgress(const Progress& progress,
                                        const std::filesystem::path& path);

}
