#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/powerup/powerup_tuning.hpp"

namespace rr::app {

inline constexpr std::string_view kPowerupTuningFile = "GAMEDATA/COMMON/POWERUPS.TOML";

/// Applies only the keys `text` names. Unknown or mistyped keys are logged and skipped, and values
/// out of range are clamped with a warning.
[[nodiscard]] Result<void> ApplyPowerupTuning(std::string_view text, std::string_view source,
                                              sim::PowerupTuning& tuning);

/// The original values with every layer's `POWERUPS.TOML` applied, lowest first. A file that
/// doesn't parse is logged and skipped.
[[nodiscard]] sim::PowerupTuning LoadPowerupTuning(const assets::Vfs& vfs);

/// Every key, as `POWERUPS.TOML` reads it.
[[nodiscard]] std::string FormatPowerupTuning(const sim::PowerupTuning& tuning);

[[nodiscard]] Result<void> DumpPowerupTuning(const std::filesystem::path& file);

/// Empty while `tuning` holds the original values.
[[nodiscard]] std::optional<std::string> PowerupTuningFingerprint(const sim::PowerupTuning& tuning);

}
