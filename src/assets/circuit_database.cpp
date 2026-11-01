#include "assets/circuit_database.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kCircuitField = 0x27;
constexpr std::string_view kCircuitsPath = "MENUDATA/LEGORACE.CRB";
constexpr std::string_view kTrackPrefix = "RACEC";

constexpr std::array kCircuitFields{
    Field<&CircuitEntry::grid>(0x28),
    IgnoredField<CircuitEntry, std::int32_t>(0x29),
    IgnoredField<CircuitEntry, std::int32_t>(0x2A),
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

Result<std::vector<std::string>> LoadGrid(const Vfs& vfs, std::string_view track) {
    const std::string upper = ToUpperAscii(track);
    if (!upper.starts_with(kTrackPrefix) || upper.size() <= kTrackPrefix.size()) {
        return Fail(ErrorCode::NotFound, std::format("{} isn't on a circuit", track));
    }
    const std::string circuit = std::format("c{}", upper[kTrackPrefix.size()]);
    return vfs.Read(kCircuitsPath)
        .and_then([](const FileData& data) { return ParseCircuits(data.Bytes()); })
        .and_then([&](std::vector<CircuitEntry> circuits) -> Result<std::vector<std::string>> {
            const auto found = std::ranges::find(circuits, circuit, &CircuitEntry::name);
            if (found == circuits.end()) {
                return Fail(ErrorCode::NotFound,
                            std::format("{} has no circuit {}", kCircuitsPath, circuit));
            }
            return std::move(found->grid);
        });
}

}
