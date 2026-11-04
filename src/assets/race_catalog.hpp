#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/circuit_database.hpp"
#include "assets/race_list.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Indices into `RaceCatalog::Races()`, one per circuit position.
using CircuitRaces = std::array<std::optional<std::size_t>, kCircuitRaces>;

class RaceCatalog {
public:
    /// Fails when a race names a missing circuit or a position another race holds.
    [[nodiscard]] static Result<RaceCatalog> Create(std::vector<CircuitEntry> circuits,
                                                    std::vector<RaceEntry> races);
    /// Applies every mod's circuit table, and logs the changes it skips.
    [[nodiscard]] static Result<RaceCatalog> Load(const Vfs& vfs);
    /// Collects the skipped circuit table changes instead of logging them.
    [[nodiscard]] static Result<RaceCatalog> Load(const Vfs& vfs,
                                                  std::vector<std::string>& problems);

    [[nodiscard]] std::span<const CircuitEntry> Circuits() const { return circuits_; }
    [[nodiscard]] std::span<const RaceEntry> Races() const { return races_; }
    [[nodiscard]] const CircuitRaces& RacesOf(std::size_t circuit) const {
        return slots_.at(circuit);
    }
    [[nodiscard]] std::optional<std::size_t> FindCircuit(std::string_view name) const;

    /// Circuits in unlock order: the first, then each one's `next` until the chain ends.
    [[nodiscard]] std::vector<std::size_t> Story() const;

    /// The first retail circuit that races `track` unmirrored. `track` matches in any case.
    [[nodiscard]] std::optional<std::size_t> HomeCircuit(std::string_view track) const;

    /// An empty `circuit` means `track`'s home circuit.
    [[nodiscard]] Result<std::vector<std::string>> Grid(std::string_view track,
                                                        std::string_view circuit) const;

private:
    RaceCatalog(std::vector<CircuitEntry> circuits, std::vector<RaceEntry> races,
                std::vector<CircuitRaces> slots);

    std::vector<CircuitEntry> circuits_;
    std::vector<RaceEntry> races_;
    std::vector<CircuitRaces> slots_;
};

[[nodiscard]] Result<std::vector<std::string>> LoadGrid(const Vfs& vfs, std::string_view track,
                                                        std::string_view circuit);

}
