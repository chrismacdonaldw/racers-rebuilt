#pragma once

#include <cstddef>
#include <vector>

#include "core/duration.hpp"
#include "render/fx_system.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

/// Turbo flames, pickup bursts, warp sparkles and magnet pulls the original never drew.
class PowerupFx {
public:
    void Tick(const sim::RaceSession& session, render::FxSystem& fx, Milliseconds elapsed);

private:
    struct RacerEmissions {
        render::FxEmission flame;
        render::FxEmission swirl;
        render::FxEmission pull;
        render::FxEmission pulse;
    };

    void TickRacer(std::size_t slot, const sim::RaceSession& session, render::FxSystem& fx,
                   float elapsedMs);

    std::vector<RacerEmissions> racers_;
};

}
