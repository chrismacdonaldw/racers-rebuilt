#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/circuit_database.hpp"
#include "assets/mod_tables.hpp"
#include "assets/race_list.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::string_view kCircuitTablePath = "MENUDATA/CIRCUITS.TOML";

struct CircuitRace {
    std::string track;
    bool isMirrored = false;

    friend bool operator==(const CircuitRace&, const CircuitRace&) = default;
};

/// A mod's new circuit, or its change to one with the same name. Unset fields keep the circuit's.
struct CircuitPatch {
    std::string name;
    std::optional<std::string> title;
    std::optional<std::string> boss;
    std::optional<std::vector<std::string>> grid;
    std::optional<std::vector<CircuitRace>> races;
    std::optional<std::string> after;
    std::optional<std::string> partSet;
    std::optional<std::string> intro;

    friend bool operator==(const CircuitPatch&, const CircuitPatch&) = default;
};

[[nodiscard]] Result<std::vector<CircuitPatch>> ParseCircuitTable(std::string_view text,
                                                                  std::string_view sourceName);

/// Every layer's table, lowest first. A table that doesn't parse is left out as a problem.
[[nodiscard]] Result<ModTables<CircuitPatch>> ReadCircuitTables(const Vfs& vfs);

/// Applies `patches` in order. Returns why each one that can't apply was skipped.
[[nodiscard]] std::vector<std::string> ApplyCircuitPatches(std::span<const CircuitPatch> patches,
                                                           std::vector<CircuitEntry>& circuits,
                                                           std::vector<RaceEntry>& races);

/// Also names patched circuits' missing tracks and drivers.
[[nodiscard]] std::vector<std::string> CircuitTableProblems(const Vfs& vfs);

}
