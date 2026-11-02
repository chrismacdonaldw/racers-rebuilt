#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

struct PartSetChoice {
    std::string piece;
    std::string colour;

    friend bool operator==(const PartSetChoice&, const PartSetChoice&) = default;
};

struct PartSet {
    std::uint16_t chassisType = 0;
    std::string chassisName;
    std::vector<PartSetChoice> choices;
};

[[nodiscard]] Result<std::vector<std::string>> ParsePieceColours(std::span<const std::byte> bytes);

/// Returns the set file names as listed, with their text `.leg` extension.
[[nodiscard]] Result<std::vector<std::string>> ParsePartSetList(std::span<const std::byte> bytes);

[[nodiscard]] Result<PartSet> ParsePartSet(std::span<const std::byte> bytes);

}
