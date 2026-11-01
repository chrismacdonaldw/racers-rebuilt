#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::size_t kCircuitRaces = 4;

struct RaceEntry {
    std::string name;
    std::string folder;
    std::optional<std::string> circuit;
    std::int32_t position = 0;
    std::optional<std::int32_t> nameLine;
    bool isMirrored = false;
    std::optional<std::string> previewScene;
    std::optional<std::string> previewDriver;
};

/// Fails when a race has no folder or a circuit position outside 0 to 3.
[[nodiscard]] Result<std::vector<RaceEntry>> ParseRaces(std::span<const std::byte> bytes);

}
