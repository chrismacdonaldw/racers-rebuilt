#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::size_t kGridSlots = 6;

/// `grid` holds a driver code for each grid slot, from slot 0.
struct CircuitEntry {
    std::string name;
    std::vector<std::string> grid;
    std::optional<std::string> next;
};

/// Fails when a circuit names no drivers, or more than there are grid slots.
[[nodiscard]] Result<std::vector<CircuitEntry>> ParseCircuits(std::span<const std::byte> bytes);

/// The driver codes of `track`'s circuit, such as `c3` for `RACEC3R1`.
[[nodiscard]] Result<std::vector<std::string>> LoadGrid(const Vfs& vfs, std::string_view track);

}
