#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/car_data.hpp"
#include "assets/original_save.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/car_build/built_car.hpp"

namespace rr::app {

inline constexpr std::size_t kPartSetCount = 12;

/// Starter and mod sets, those `partSets` unlocks and those the car's pieces use, with choices.
[[nodiscard]] std::vector<std::size_t> AvailablePartSets(const std::set<std::int32_t>& partSets,
                                                         const sim::BuiltCar& car,
                                                         const assets::PieceCatalog& catalog);

/// A placed piece's part type is its set's chassis type, or 0 for an extra piece.
[[nodiscard]] std::uint16_t PartTypeOf(const assets::PieceCatalog& catalog, std::size_t set);

[[nodiscard]] std::optional<std::size_t> PartSetOfType(const assets::PieceCatalog& catalog,
                                                       std::uint16_t partType);

/// A mod's set shows the icon of the game's set with its chassis.
[[nodiscard]] std::vector<std::size_t> SetIcons(const assets::PieceCatalog& catalog,
                                                std::span<const std::size_t> sets);

/// The first set when none has that chassis.
[[nodiscard]] std::size_t PartSetOfChassis(const assets::PieceCatalog& catalog,
                                           std::string_view chassis);

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

[[nodiscard]] Result<QuickBuilds> LoadQuickBuilds(const assets::Vfs& vfs);

}
