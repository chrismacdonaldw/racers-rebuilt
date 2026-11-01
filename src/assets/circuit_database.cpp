#include "assets/circuit_database.hpp"

#include <algorithm>
#include <array>
#include <format>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kCircuitField = 0x27;

constexpr std::array kCircuitFields{
    Field<&CircuitEntry::grid>(0x28),
    Field<&CircuitEntry::number>(0x29),
    Field<&CircuitEntry::partSet>(0x2A),
    Field<&CircuitEntry::next>(0x2B),
};

}

Result<std::vector<CircuitEntry>> ParseCircuits(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<CircuitEntry>(bytes, kCircuitField, kCircuitFields)
        .and_then([](std::vector<CircuitEntry> circuits) -> Result<std::vector<CircuitEntry>> {
            const auto misfit = std::ranges::find_if(circuits, [](const CircuitEntry& circuit) {
                return circuit.grid.empty() || circuit.grid.size() > kGridSlots;
            });
            if (misfit != circuits.end()) {
                return Fail(ErrorCode::InvalidData,
                            std::format("circuit {} names {} drivers for 1 to {} grid slots",
                                        misfit->name, misfit->grid.size(), kGridSlots));
            }
            return circuits;
        });
}

}
