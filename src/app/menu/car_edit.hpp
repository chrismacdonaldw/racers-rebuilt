#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/car_data.hpp"
#include "assets/original_save.hpp"
#include "assets/piece_catalog.hpp"
#include "core/result.hpp"
#include "sim/car_build/built_car.hpp"

namespace rr::app {

inline constexpr std::size_t kPartSetCount = 12;

/// The four starter sets, those `partSets` unlocks, and any set the car's pieces come from.
[[nodiscard]] std::vector<std::size_t> AvailablePartSets(const std::set<std::int32_t>& partSets,
                                                         const sim::BuiltCar& car,
                                                         const assets::PieceCatalog& catalog);

/// A placed piece's part type is its set's chassis type.
[[nodiscard]] std::uint16_t PartTypeOf(const assets::PieceCatalog& catalog, std::size_t set);

[[nodiscard]] std::optional<std::size_t> PartSetOfType(const assets::PieceCatalog& catalog,
                                                       std::uint16_t partType);

/// An empty car, with a warning, when `data` doesn't load, so the editor can start it afresh.
[[nodiscard]] sim::BuiltCar LoadEditedCar(const assets::CarData& data,
                                          const assets::PieceLibrary& library);

/// Fails, leaving the car as it was, when the set or its chassis is missing.
[[nodiscard]] Result<void> ResetToChassis(sim::BuiltCar& car, const assets::PieceCatalog& catalog,
                                          std::size_t set, std::uint8_t colour);

class QuickBuilds {
public:
    explicit QuickBuilds(std::vector<assets::SavedRacer> builds) : builds_(std::move(builds)) {}

    /// The next build on `chassis`, carrying on from the last one picked, or null when none is.
    [[nodiscard]] const assets::SavedRacer* Next(std::string_view chassis);

    [[nodiscard]] bool Contains(const assets::CarData& car) const;

private:
    std::vector<assets::SavedRacer> builds_;
    std::size_t next_ = 0;
};

}
