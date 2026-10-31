#include "sim/race/race_session.hpp"

#include <cstdint>
#include <format>

#include "core/assert.hpp"
#include "sim/physics/body_state.hpp"

namespace rr::sim {
namespace {

BrickField BricksFor(const RaceCourse& course) {
    return course.bricks == nullptr ? BrickField() : BrickField(*course.bricks);
}

std::size_t BrickCount(const RaceCourse& course) {
    return course.bricks == nullptr ? 0
                                    : course.bricks->coloured.size() + course.bricks->white.size();
}

std::span<const assets::RaceRoute> RoutesFor(const RaceCourse& course, std::size_t slot) {
    if (course.routes == nullptr || slot >= course.routes->size()) {
        return {};
    }
    return (*course.routes)[slot];
}

}

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
      wasRespawned_(setup.racers.size()),
      positions_(setup.racers.size()),
      progress_(setup.racers.size(), setup.lapCount),
      sounds_(setup.racers.size(), BrickCount(course)),
      random_(setup.seed),
      rubberBand_(course.isMirrored),
      paced_(setup.racers.size()),
      routeSpeeds_(setup.racers.size(), 1.0F),
      bricks_(BricksFor(course)),
      inventories_(setup.racers.size()),
      reaches_(setup.racers.size()) {
    racers_.reserve(setup.racers.size());
    for (std::size_t i = 0; i < setup.racers.size(); ++i) {
        const RacerEntry& entry = setup.racers[i];
        Racer& racer = racers_.emplace_back(entry.chassis, entry.build, course.grid[i]);
        racer.HoldAtStart();
        const std::span<const assets::RaceRoute> routes = RoutesFor(course, i);
        if (!entry.isPlayer && !routes.empty()) {
            racer.FollowRoute(routes[random_.Below(static_cast<std::uint32_t>(routes.size()))]);
        }
        isPlayer_.push_back(entry.isPlayer);
        positions_[i] = racer.Body().motion.position;
    }
}

void RaceSession::Tick(Milliseconds elapsed, std::span<const RacerControls> controls) {
    RR_ASSERT(controls.size() == racers_.size());
    sounds_.BeginTick();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        TickRacer(i, elapsed, clock_.Phase() == RacePhase::Intro ? RacerControls{} : controls[i]);
    }
    TickBricks(elapsed);
    progress_.Rank(*course_.checkpoints, positions_);
    const RaceClock before = clock_;
    clock_.Advance(elapsed, HasPlayerFinished());
    sounds_.HearClock(before, clock_, isPlayer_);
    if (before.Phase() == RacePhase::Countdown && clock_.Phase() == RacePhase::Racing) {
        for (Racer& racer : racers_) {
            racer.Go();
        }
    }
    if (before.Phase() == RacePhase::Finished && clock_.Phase() == RacePhase::Results) {
        progress_.FinishRemaining();
    }
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        sounds_.HearRacer(i, racers_[i], isPlayer_[i], clock_.IsRunning(), elapsed);
    }
    PaceRoutes(elapsed);
}

void RaceSession::PaceRoutes(Milliseconds elapsed) {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        paced_[i] = {.lapProgress = progress_.OfRacer(i).laps.Progress(*course_.checkpoints),
                     .isAiControlled = !isPlayer_[i]};
    }
    rubberBand_.Update(elapsed, paced_, routeSpeeds_);
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        racers_[i].SetRouteSpeed(routeSpeeds_[i]);
    }
}

void RaceSession::TickBricks(Milliseconds elapsed) {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        reaches_[i] = {.centre = racers_[i].Body().motion.position,
                       .radius = racers_[i].BoundsRadius()};
    }
    bricks_.Tick(elapsed, reaches_, inventories_);
    sounds_.HearBricks(bricks_.Events());
}

void RaceSession::TickRacer(std::size_t racer, Milliseconds elapsed,
                            const RacerControls& controls) {
    Racer& car = racers_[racer];
    const Vector3 from = car.Body().motion.position;
    car.Tick(elapsed, controls, *course_.world);
    const BodyMotion& motion = car.Body().motion;
    wasRespawned_[racer] =
        !car.Route() && respawns_[racer].Update(respawn_, elapsed, IsGrounded(motion),
                                                motion.position.z, course_.lowestTrackZ);
    if (wasRespawned_[racer]) {
        car.Place(RespawnPoint(*course_.checkpoints, progress_.OfRacer(racer).laps.LastCheckpoint(),
                               course_.grid[racer], *course_.world));
    } else {
        const bool hadFinished = progress_.OfRacer(racer).finishPlace.has_value();
        progress_.Move(*course_.checkpoints, racer,
                       {.from = from, .to = motion.position, .facing = car.Body().facing}, elapsed,
                       clock_.IsRunning());
        if (!hadFinished && progress_.OfRacer(racer).finishPlace) {
            sounds_.HearFinish(racer);
        }
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
