#pragma once

#include <cstdint>
#include <string>

namespace rr::assets {

/// Indices into the merged BODYPART.PCB part lists. A part's name, when set, wins over its index,
/// so the part stays put when mods change the lists.
struct DriverLook {
    std::uint8_t face = 0;
    std::uint8_t hat = 0;
    std::uint8_t legs = 0;
    std::uint8_t torso = 0;
    std::uint8_t expression = 0;
    std::string faceName;
    std::string hatName;
    std::string legsName;
    std::string torsoName;

    friend bool operator==(const DriverLook&, const DriverLook&) = default;
};

}
