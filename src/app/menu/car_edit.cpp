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
constexpr std::string_view kQuickBuildFile = "MENUDATA/QBUILD.LRS";

}

Result<QuickBuilds> LoadQuickBuilds(const assets::Vfs& vfs) {
    return vfs.Read(kQuickBuildFile)
        .and_then([](const assets::FileData& data) { return assets::ParseRacerFile(data.Bytes()); })
        .transform(
            [](std::vector<assets::SavedRacer> builds) { return QuickBuilds(std::move(builds)); });
}

std::size_t PartSetOfChassis(const assets::PieceCatalog& catalog, std::string_view chassis) {
    const std::string wanted = assets::ToUpperAscii(chassis);
    const auto found = std::ranges::find_if(catalog.partSets, [&](const auto& set) {
        return assets::ToUpperAscii(set.chassisName) == wanted;
    });
    return found == catalog.partSets.end()
               ? 0
               : static_cast<std::size_t>(found - catalog.partSets.begin());
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
                                           const sim::BuiltCar& car,
                                           const assets::PieceCatalog& catalog) {
    std::array<bool, kPartSetCount> isAvailable{};
    std::fill_n(isAvailable.begin(), kStarterPartSets, true);
    for (const std::int32_t unlocked : partSets) {
        const auto set = kStarterPartSets + static_cast<std::size_t>(unlocked);
        if (unlocked >= 0 && set < kPartSetCount) {
            isAvailable.at(set) = true;
        }
    }
    for (const sim::CarPiece& piece : car.Pieces()) {
        const std::optional<std::size_t> set = PartSetOfType(catalog, piece.partType);
        if (set && *set < kPartSetCount) {
            isAvailable.at(*set) = true;
        }
    }
    std::vector<std::size_t> sets;
    for (std::size_t set = 0; set < kPartSetCount; ++set) {
        if (isAvailable.at(set) && set < catalog.partSets.size() &&
            !catalog.partSets[set].choices.empty()) {
            sets.push_back(set);
        }
    }
    return sets;
}

std::uint16_t PartTypeOf(const assets::PieceCatalog& catalog, std::size_t set) {
    return catalog.library.pieces.at(catalog.partSets.at(set).chassis).type;
}

std::optional<std::size_t> PartSetOfType(const assets::PieceCatalog& catalog,
                                         std::uint16_t partType) {
    if (partType == 0) {
        return std::nullopt;
    }
    for (std::size_t set = 0; set < catalog.partSets.size(); ++set) {
        if (PartTypeOf(catalog, set) == partType) {
            return set;
        }
    }
    return std::nullopt;
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
