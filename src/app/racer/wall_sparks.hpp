#pragma once

#include <span>
#include <vector>

#include "core/duration.hpp"
#include "render/fx_system.hpp"
#include "sim/physics/racer.hpp"

namespace rr::app {

/// Throws sparks where a moving car scrapes a wall, a few at a time while the scrape lasts.
class WallSparks {
public:
    void Tick(std::span<const sim::Racer> racers, render::FxSystem& fx, Milliseconds elapsed);

private:
    std::vector<Milliseconds> cooldowns_;
};

}
