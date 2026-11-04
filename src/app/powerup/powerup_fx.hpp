#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "core/duration.hpp"
#include "render/fx_system.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

/// Turbo flames, pickup bursts, warp sparkles and magnet pulls the original never drew.
class PowerupFx {
public:
    void Tick(const sim::RaceSession& session, render::FxSystem& fx, Milliseconds elapsed);

private:
    struct Effects {
        std::optional<render::FxEffectId> flame;
        std::optional<render::FxEffectId> hover;
        std::optional<render::FxEffectId> swirl;
        std::optional<render::FxEffectId> warpOut;
        std::optional<render::FxEffectId> warpIn;
        std::optional<render::FxEffectId> pull;
        std::optional<render::FxEffectId> pulse;
        std::optional<render::FxEffectId> white;
        std::array<std::optional<render::FxEffectId>, sim::kPowerupColourCount> pickups;
    };

    struct RacerEmissions {
        render::FxEmission flame;
        render::FxEmission swirl;
        render::FxEmission pull;
        render::FxEmission pulse;
    };

    [[nodiscard]] static Effects EffectsOf(const render::FxSystem& fx);
    void TickRacer(std::size_t slot, const sim::RaceSession& session, const Effects& effects,
                   render::FxSystem& fx, float elapsedMs);
    static void BurstPickups(const sim::BrickField& bricks, const Effects& effects,
                             render::FxSystem& fx);

    std::optional<Effects> effects_;
    std::vector<RacerEmissions> racers_;
};

}
