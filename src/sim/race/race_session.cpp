#include "sim/race/race_session.hpp"

#include <format>

#include "core/assert.hpp"
#include "sim/physics/body_state.hpp"

namespace rr::sim {

Result<RaceSession> RaceSession::Create(const RaceSetup& setup, const RaceCourse& course) {
    RR_ASSERT(course.checkpoints != nullptr && course.world != nullptr);
    if (setup.racers.size() > course.grid.size()) {
        return Fail(ErrorCode::InvalidData, std::format("{} racers on a grid of {}",
                                                        setup.racers.size(), course.grid.size()));
    }
    return RaceSession(setup, course);
}

RaceSession::RaceSession(const RaceSetup& setup, const RaceCourse& course)
    : course_(course),
      respawn_(setup.respawn),
      respawns_(setup.racers.size()),
      positions_(setup.racers.size()),
      progress_(setup.racers.size(), setup.lapCount),
      random_(setup.seed) {
    racers_.reserve(setup.racers.size());
    for (std::size_t i = 0; i < setup.racers.size(); ++i) {
        const RacerEntry& entry = setup.racers[i];
        racers_.emplace_back(entry.chassis, entry.build, course.grid[i]).HoldAtStart();
        isPlayer_.push_back(entry.isPlayer);
        positions_[i] = course.grid[i].position;
    }
}

void RaceSession::Tick(Milliseconds elapsed, std::span<const RacerControls> controls) {
    RR_ASSERT(controls.size() == racers_.size());
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        TickRacer(i, elapsed, clock_.Phase() == RacePhase::Intro ? RacerControls{} : controls[i]);
    }
    progress_.Rank(*course_.checkpoints, positions_);
    const RacePhase before = clock_.Phase();
    clock_.Advance(elapsed, HasPlayerFinished());
    if (before == RacePhase::Countdown && clock_.Phase() == RacePhase::Racing) {
        for (Racer& racer : racers_) {
            racer.Go();
        }
    }
    if (before == RacePhase::Finished && clock_.Phase() == RacePhase::Results) {
        progress_.FinishRemaining();
    }
}

void RaceSession::TickRacer(std::size_t racer, Milliseconds elapsed,
                            const RacerControls& controls) {
    Racer& car = racers_[racer];
    const Vector3 from = car.Body().motion.position;
    car.Tick(elapsed, controls, *course_.world);
    const BodyMotion& motion = car.Body().motion;
    if (respawns_[racer].Update(respawn_, elapsed, IsGrounded(motion), motion.position.z,
                                course_.lowestTrackZ)) {
        car.Place(RespawnPoint(*course_.checkpoints, progress_.OfRacer(racer).laps.LastCheckpoint(),
                               course_.grid[racer], *course_.world));
    } else {
        progress_.Move(*course_.checkpoints, racer,
                       {.from = from, .to = motion.position, .facing = car.Body().facing}, elapsed,
                       clock_.IsRunning());
    }
    positions_[racer] = car.Body().motion.position;
}

bool RaceSession::HasPlayerFinished() const {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (isPlayer_[i] && progress_.OfRacer(i).finishPlace) {
            return true;
        }
    }
    return false;
}

}
