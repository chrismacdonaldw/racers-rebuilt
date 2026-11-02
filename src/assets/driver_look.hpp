#pragma once

#include <cstdint>

namespace rr::assets {

/// Indices into the BODYPART.PCB part lists.
struct DriverLook {
    std::uint8_t face = 0;
    std::uint8_t hat = 0;
    std::uint8_t legs = 0;
    std::uint8_t torso = 0;
    std::uint8_t expression = 0;

    friend bool operator==(const DriverLook&, const DriverLook&) = default;
};

}
