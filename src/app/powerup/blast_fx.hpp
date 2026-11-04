#pragma once

#include <vector>

#include "render/fx_system.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

/// Bursts a fireball, a shockwave, embers and tumbling bricks as each blast or spike explodes.
class BlastFx {
public:
    void Tick(const sim::RaceSession& session, render::FxSystem& fx);

private:
    std::vector<sim::Explosion> blasts_;
    std::vector<sim::Explosion> spikes_;
};

}
