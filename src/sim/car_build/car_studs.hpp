#pragma once

#include <cstdint>
#include <vector>

#include "sim/car_build/built_car.hpp"

namespace rr::sim {

/// `height` is in plates. A nonzero `raisedBottom` picks the colour whose material has that index.
struct CarStud {
    int x = 0;
    int y = 0;
    int height = 0;
    std::uint8_t colour = 0;
    int raisedBottom = 0;

    friend bool operator==(const CarStud&, const CarStud&) = default;
};

/// Every occupied top, then each top a later piece covers without sitting flush on it.
[[nodiscard]] std::vector<CarStud> CarStuds(const BuiltCar& car);

}
