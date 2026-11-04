#include "app/menu/racer_lock.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "assets/ldraw_extra_pieces.hpp"

namespace rr::app {
namespace {

assets::CarData CarOf(std::uint16_t chassis, std::uint16_t brick) {
    assets::CarData car{};
    car[1] = std::byte{2};
    car[2] = static_cast<std::byte>(chassis >> 8U);
    car[3] = static_cast<std::byte>(chassis & 0xFFU);
    car[10] = static_cast<std::byte>(brick >> 8U);
    car[11] = static_cast<std::byte>(brick & 0xFFU);
    return car;
}

TEST_CASE("a car with an extra piece locks only while extra pieces are off") {
    const assets::CarData extra = CarOf(17, assets::kFirstExtraPieceType + 3);

    CHECK(IsLockedCar(extra, false));
    CHECK_FALSE(IsLockedCar(extra, true));
}

TEST_CASE("a car of original pieces never locks") {
    CHECK_FALSE(IsLockedCar(CarOf(17, 0x900), false));
    CHECK_FALSE(IsLockedCar(assets::CarData{}, false));
}

TEST_CASE("a driver wearing a part the catalogue lacks locks its racer") {
    RacerLocks locks(assets::DriverPartCatalog{.hats = {{.name = "nohat"}}});
    const settings::Racer worn{.look = {.hatName = "NOHAT"}};
    const settings::Racer missing{.look = {.hatName = "lava-helmet"}};
    settings::Racer both = missing;
    both.car = CarOf(17, assets::kFirstExtraPieceType);

    CHECK(locks.Of(worn) == RacerLock::None);
    CHECK(locks.Of(missing) == RacerLock::MissingParts);
    CHECK(locks.Of(both) == RacerLock::ExtraPieces);
    locks.AllowExtraPieces(true);
    CHECK(locks.Of(both) == RacerLock::MissingParts);
}

TEST_CASE("without a parts catalogue no driver locks") {
    const RacerLocks locks(std::nullopt);

    CHECK(locks.Of({.look = {.hatName = "lava-helmet"}}) == RacerLock::None);
}

TEST_CASE("a car with a piece no mounted mod gives locks its racer") {
    const std::vector<std::uint16_t> types{17, 0x900, assets::kFirstExtraPieceType};
    const RacerLocks locks(std::nullopt, types);

    CHECK(locks.Of({.car = CarOf(17, 0x900)}) == RacerLock::None);
    CHECK(locks.Of({.car = CarOf(17, assets::kFirstModPieceType + 5)}) == RacerLock::MissingPieces);
    CHECK(locks.Of({.car = CarOf(17, assets::kFirstExtraPieceType)}) == RacerLock::ExtraPieces);
}

}
}
