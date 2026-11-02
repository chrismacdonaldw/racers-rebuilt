#pragma once

#include <string_view>

#include "settings/progress.hpp"

#include <toml++/toml.hpp>

namespace rr::settings {

/// Logs and skips a circuit whose trophy isn't `gold`, `silver` or `bronze`.
[[nodiscard]] Trophies ReadTrophyTable(const toml::table& table, std::string_view fileName);

[[nodiscard]] toml::table TrophyTable(const Trophies& trophies);

}
