#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/brick_layout.hpp"
#include "assets/driver_database.hpp"
#include "assets/mod_tables.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::string_view kDriverTablePath = "GAMEDATA/COMMON/DRIVERS.TOML";

/// An unset `colour` charges nothing.
struct DriverCharge {
    std::optional<BrickColour> colour;
    std::int32_t level = 0;

    friend bool operator==(const DriverCharge&, const DriverCharge&) = default;
};

/// A mod's new driver, or its change to the driver with the same code. Unset fields keep the
/// driver's. `voice` names the driver whose voice lines to use.
struct DriverPatch {
    std::string code;
    std::optional<std::string> name;
    std::optional<std::string> racer;
    std::optional<std::string> champion;
    std::optional<std::string> voice;
    std::array<std::optional<std::uint8_t>, kBrickColourNames.size()> useChances{};
    std::optional<DriverCharge> charge;
    std::optional<bool> startBoost;

    friend bool operator==(const DriverPatch&, const DriverPatch&) = default;
};

[[nodiscard]] Result<std::vector<DriverPatch>> ParseDriverTable(std::string_view text,
                                                                std::string_view sourceName);

/// Every layer's table, lowest first. A table that doesn't parse is left out as a problem.
[[nodiscard]] Result<ModTables<DriverPatch>> ReadDriverTables(const Vfs& vfs);

/// Applies `patches` in order. Returns why each one that can't apply was skipped.
[[nodiscard]] std::vector<std::string> ApplyDriverPatches(std::span<const DriverPatch> patches,
                                                          std::vector<DriverEntry>& drivers);

}
