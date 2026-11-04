#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mods/mod.hpp"

namespace rr::mods {

struct HeldMod {
    std::size_t index;
    std::string reason;
};

/// Indices into the resolved mods. `mountOrder` is bottom first.
struct ModResolution {
    std::vector<std::size_t> mountOrder;
    std::vector<HeldMod> held;
};

[[nodiscard]] std::optional<std::size_t> FindMod(std::span<const Mod> mods, std::string_view id);

/// True when either names the other in `conflicts` with a range the other's version matches.
[[nodiscard]] bool AreInConflict(const Mod& a, const Mod& b);

/// `mods` is highest priority first and `isOn` holds each one's toggle. Required and `after` mods
/// mount below the mods naming them; otherwise lower priority mounts first.
[[nodiscard]] ModResolution ResolveMods(std::span<const Mod> mods, const std::vector<bool>& isOn);

}
