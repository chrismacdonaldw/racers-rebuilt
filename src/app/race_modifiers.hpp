#pragma once

#include "sim/race/modifiers.hpp"

namespace rr::app {

struct RaceModifiers {
    sim::Modifiers rules;
    bool mirrorsTracks = false;
    bool squeaksVoices = false;
    bool rushesRocketRacer = false;

    friend bool operator==(const RaceModifiers&, const RaceModifiers&) = default;
};

}
