#include "app/menu/car_edit.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>

#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kStarterPartSets = 4;

}

sim::BuiltCar LoadEditedCar(const assets::CarData& data, const assets::PieceLibrary& library) {
    Result<sim::BuiltCar> car = sim::BuiltCar::Load(data, library);
    if (!car) {
        log::Warn("{}, so the car editor starts the car afresh", car.error().message);
        return {};
    }
    return std::move(*car);
}

std::vector<std::size_t> AvailablePartSets(const std::set<std::int32_t>& partSets,
                                           const sim::BuiltCar& car) {
    std::array<bool, kPartSetCount> isAvailable{};
    std::fill_n(isAvailable.begin(), kStarterPartSets, true);
    for (const std::int32_t unlocked : partSets) {
        const auto set = kStarterPartSets + static_cast<std::size_t>(unlocked);
        if (unlocked >= 0 && set < kPartSetCount) {
            isAvailable.at(set) = true;
        }
    }
    for (const sim::CarPiece& piece : car.Pieces()) {
        if (piece.partSet < kPartSetCount) {
            isAvailable.at(piece.partSet) = true;
        }
    }
    std::vector<std::size_t> sets;
    for (std::size_t set = 0; set < kPartSetCount; ++set) {
        if (isAvailable.at(set)) {
            sets.push_back(set);
        }
    }
    return sets;
}

Result<void> ResetToChassis(sim::BuiltCar& car, const assets::PieceCatalog& catalog,
                            std::size_t set, std::uint8_t colour) {
    if (set >= catalog.partSets.size()) {
        return Fail(ErrorCode::NotFound, std::format("there's no part set {}", set));
    }
    const std::string& name = catalog.partSets[set].chassisName;
    const assets::PieceRecord* chassis = catalog.library.Find(name);
    if (chassis == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("the piece library has no chassis {}", name));
    }
    car.Clear();
    if (!car.Place(*chassis, {}, colour, 0)) {
        return Fail(ErrorCode::InvalidData, std::format("chassis {} doesn't fit the grid", name));
    }
    return {};
}

const assets::SavedRacer* QuickBuilds::Next(std::string_view chassis) {
    const std::string wanted = assets::ToUpperAscii(chassis);
    for (std::size_t tried = 0; tried < builds_.size(); ++tried) {
        const assets::SavedRacer& build = builds_[next_];
        next_ = (next_ + 1) % builds_.size();
        if (assets::ToUpperAscii(build.chassis) == wanted) {
            return &build;
        }
    }
    return nullptr;
}

bool QuickBuilds::Contains(const assets::CarData& car) const {
    return std::ranges::contains(builds_, car, &assets::SavedRacer::car);
}

}
