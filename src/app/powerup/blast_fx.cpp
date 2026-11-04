#include "app/powerup/blast_fx.hpp"

#include <cstddef>
#include <optional>
#include <span>

#include "app/race_fx.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

bool HasStarted(const sim::Explosion& now, const sim::Explosion& before) {
    return now.phase == sim::ExplosionPhase::Exploding &&
           (before.phase != sim::ExplosionPhase::Exploding || now.remaining > before.remaining);
}

void BurstNew(const sim::Explosions& explosions, std::vector<sim::Explosion>& seen,
              render::FxSystem& fx, render::FxEffectId blast) {
    const std::span<const sim::Explosion> all = explosions.All();
    seen.resize(all.size());
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (HasStarted(all[i], seen[i])) {
            fx.Burst(blast, {.position = ToEngine(all[i].position)});
        }
        seen[i] = all[i];
    }
}

}

void BlastFx::Tick(const sim::RaceSession& session, render::FxSystem& fx) {
    const std::optional<render::FxEffectId> blast = fx.EffectFor(kBlastEffect);
    if (!blast) {
        return;
    }
    BurstNew(session.Blasts(), blasts_, fx, *blast);
    BurstNew(session.Spikes(), spikes_, fx, *blast);
}

}
