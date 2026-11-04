#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// A track folder's `TRACK.TOML`. `preview` is a game path.
struct TrackInfo {
    std::string name;
    std::optional<std::string> preview;
    std::optional<int> lapCount;
    std::optional<std::string> grid;
    bool hasTimeTrial = false;
    bool hasMirrored = false;
    std::vector<std::string> music;
};

[[nodiscard]] Result<TrackInfo> ParseTrackInfo(std::string_view text, std::string_view track,
                                               std::string_view sourceName);

/// Unset when `track`'s folder has no `TRACK.TOML`.
[[nodiscard]] Result<std::optional<TrackInfo>> ReadTrackInfo(const assets::Vfs& vfs,
                                                             std::string_view track);

/// Logs a broken `TRACK.TOML` and reads it as missing.
[[nodiscard]] std::optional<TrackInfo> FindTrackInfo(const assets::Vfs& vfs,
                                                     std::string_view track);

/// The race music list with `music` in the race slots, the first one where the list has its own.
[[nodiscard]] std::vector<std::string> WithTrackMusic(std::vector<std::string> list,
                                                      const std::vector<std::string>& music);

/// Broken `TRACK.TOML` files, and grids or previews they name that don't exist.
[[nodiscard]] std::vector<std::string> TrackInfoProblems(const assets::Vfs& vfs);

}
