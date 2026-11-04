#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::size_t kGridSlots = 6;

/// `grid` starts at slot 0. Gold unlocks part set `partSet`, gold or silver unlocks `next`.
/// An empty `title` or `boss` means the retail text and champion. An unset `intro` means the
/// retail intro for the circuit's name, and an empty one none.
struct CircuitEntry {
    std::string name;
    std::vector<std::string> grid;
    std::int32_t number = 0;
    std::optional<std::int32_t> partSet;
    std::optional<std::string> next;
    std::string title;
    std::string boss;
    std::optional<std::string> namedPartSet;
    std::optional<std::string> intro;
    bool isFromMod = false;
};

/// Fails when a circuit names no drivers, or more than there are grid slots.
[[nodiscard]] Result<std::vector<CircuitEntry>> ParseCircuits(std::span<const std::byte> bytes);

}
