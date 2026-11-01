#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"

namespace rr::sim {

inline constexpr assets::EventId kSmokeVentTrigger = 10;

struct SmokeVent {
    std::optional<std::size_t> entity;
};

struct Snowfall {};

}
