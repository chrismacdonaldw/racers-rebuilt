#include "app/menu/extra_pieces_lock.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>

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

}
}
