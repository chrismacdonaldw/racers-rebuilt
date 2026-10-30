#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "sim/ai/route_choice.hpp"
#include "sim/ai/rubber_band.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_progress.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/random.hpp"
#include "sim/race/respawn.hpp"

namespace rr::sim {

/// What a session reads from its track. Everything it points to must outlive the session.
struct RaceCourse {
    const CheckpointGraph* checkpoints = nullptr;
    std::span<const RacerStart> grid;
    const CollisionQuery* world = nullptr;
    float lowestTrackZ = 0.0F;
    const GridRoutes* routes = nullptr;
    bool isMirrored = false;
};

class RaceSession {
public:
    /// Fails when there are more racers than grid slots.
    [[nodiscard]] static Result<RaceSession> Create(const RaceSetup& setup,
                                                    const RaceCourse& course);

    /// `controls` has one entry per racer. The intro ignores them and the countdown only steers.
    void Tick(Milliseconds elapsed, std::span<const RacerControls> controls);

    [[nodiscard]] const RaceClock& Clock() const { return clock_; }
    [[nodiscard]] std::span<const Racer> Racers() const { return racers_; }
    [[nodiscard]] const RaceProgress& Progress() const { return progress_; }
    [[nodiscard]] Random& Rng() { return random_; }

    /// Whether the last `Tick` respawned `racer`.
    [[nodiscard]] bool WasRespawned(std::size_t racer) const { return wasRespawned_.at(racer); }

private:
    RaceSession(const RaceSetup& setup, const RaceCourse& course);

    void TickRacer(std::size_t racer, Milliseconds elapsed, const RacerControls& controls);
    void PaceRoutes(Milliseconds elapsed);
    [[nodiscard]] bool HasPlayerFinished() const;

    RaceCourse course_;
    RespawnRule respawn_;
    std::vector<bool> isPlayer_;
    std::vector<Racer> racers_;
    std::vector<RespawnWatch> respawns_;
    std::vector<bool> wasRespawned_;
    std::vector<Vector3> positions_;
    RaceProgress progress_;
    RaceClock clock_;
    Random random_;
    RubberBand rubberBand_;
    std::vector<PacedRacer> paced_;
    std::vector<float> routeSpeeds_;
};

}
