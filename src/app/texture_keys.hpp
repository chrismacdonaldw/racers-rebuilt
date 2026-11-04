#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "assets/texture_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Keyed by upper-case image file name, such as `BRICK.TGA`. `conflicts` kept their first key.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct ColorKeys {
    std::unordered_map<std::string, assets::ColorKey> byName;
    std::vector<std::string> conflicts;
};

/// Reads every TDB, IDB and FDB in `layer` only, skipping a table that doesn't parse.
[[nodiscard]] Result<ColorKeys> CollectColorKeys(const assets::Vfs& vfs, std::size_t layer);

}
