#include "assets/race_list.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kRaceField = 0x27;

constexpr std::array kRaceFields{
    Field<&RaceEntry::position>(0x28),
    Field<&RaceEntry::folder>(0x29),
    Field<&RaceEntry::circuit>(0x2A),
    Field<&RaceEntry::nameLine>(0x2B),
    FlagField<&RaceEntry::isMirrored, true>(0x2C),
    Field<&RaceEntry::previewScene>(0x2D),
    Field<&RaceEntry::previewDriver>(0x2E),
};

bool IsMisplaced(const RaceEntry& race) {
    return race.folder.empty() || race.position < 0 ||
           std::cmp_greater_equal(race.position, kCircuitRaces);
}

}

Result<std::vector<RaceEntry>> ParseRaces(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<RaceEntry>(bytes, kRaceField, kRaceFields)
        .and_then([](std::vector<RaceEntry> races) -> Result<std::vector<RaceEntry>> {
            const auto misfit = std::ranges::find_if(races, IsMisplaced);
            if (misfit != races.end()) {
                return Fail(ErrorCode::InvalidData,
                            std::format("race {} has folder '{}' at circuit position {}",
                                        misfit->name, misfit->folder, misfit->position));
            }
            return races;
        });
}

}
