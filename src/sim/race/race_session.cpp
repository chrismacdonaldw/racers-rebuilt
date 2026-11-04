#include "sim/race/race_session.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>

#include "core/assert.hpp"
#include "core/transform.hpp"
#include "sim/ai/ai_aim.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/contact_effects.hpp"
#include "sim/race/player_cheats.hpp"
#include "sim/race/session_parts.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kCatchUpDelay{15000};

}

Result<RaceSession> RaceSession::Create(const RaceSetup& setup, const RaceCourse& course) {
    RR_ASSERT(course.checkpoints != nullptr && course.world != nullptr);
    if (setup.firstSlot + setup.racers.size() > course.grid.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} racers from slot {} on a grid of {}", setup.racers.size(),
                                setup.firstSlot, course.grid.size()));
    }
    RaceCourse shifted = course;
    shifted.grid = course.grid.subspan(setup.firstSlot);
    return RaceSession(setup, shifted);
}

RaceSession::RaceSession(const RaceSetup& setup, const RaceCourse& course)
    : course_(course),
      respawn_(setup.respawn),
      cheats_(setup.cheats),
      modifiers_(setup.modifiers),
      playerCheats_(setup.playerCheats),
      headStartLeft_(setup.playerCheats.headStart),
      isDemo_(setup.isDemo),
      rules_(&RulesFor(setup.version)),
      lagsInput_(setup.lagsInput && rules_->allowsInputLag),
      lastControls_(setup.racers.size()),
      respawns_(setup.racers.size()),
      wasRespawned_(setup.racers.size()),
      positions_(setup.racers.size()),
      rivals_(setup.racers.size()),
      places_(setup.racers.size()),
      progress_(setup.racers.size(), setup.lapCount,
                {.usesZones = !EventSourcesFor(course).table.lapZones.empty(),
                 .keepsOneZone = rules_->keepsOneLapZone}),
      clock_(setup.isTestDrive),
      sounds_(setup.racers.size(), BrickCount(course), WeaponEventCount(setup, course)),
      random_(setup.seed),
      surfaces_(SurfacesFor(course)),
      triggers_(EventSourcesFor(course), setup.racers.size(), course.isMirrored, random_,
                OutsideEventCount(setup, course)),
      entities_(EntitySourcesFor(course)),
      hazards_(course.hazards, entities_, random_),
      resources_(EventSourcesFor(course).table, EntitySourcesFor(course), course.collision,
                 EventSourcesFor(course).sky),
      tints_(EventSourcesFor(course).table, setup.racers.size()),
      targets_(course.targets),
      triggerRacers_(setup.racers.size()),
      sphereWorlds_(SphereWorldsFor(course)),
      switchedWorlds_(setup.racers.size()),
      surfaceEvents_(setup.racers.size()),
      rubberBand_(PaceBonus(course.isMirrored, setup.circuit, *rules_)),
      skillLevels_(setup.racers.size()),
      catchesUp_(setup.aiSkill && setup.aiSkill->catchesUp),
      paced_(setup.racers.size()),
      routeSpeeds_(setup.racers.size(), 1.0F),
      bricks_(BricksFor(course, setup.cheats)),
      inventories_(setup.racers.size()),
      powerups_(setup.effects, UsesPerTick(setup, course), setup.cheats,
                setup.modifiers.onlyPowerup),
      weapons_(WeaponEventCount(setup, course)),
      reaches_(setup.racers.size()),
      pickupReaches_(setup.racers.size()),
      contactBodies_(setup.racers.size()),
      lines_(RacingLinesFor(setup, course)) {
    racers_.reserve(setup.racers.size());
    contactWorlds_.reserve(setup.racers.size());
    localBounds_.reserve(setup.racers.size());
    for (std::size_t i = 0; i < setup.racers.size(); ++i) {
        const RacerEntry& entry = setup.racers[i];
        Racer& racer = racers_.emplace_back(entry.chassis, entry.build, course.grid[i], *rules_);
        racer.HoldAtStart();
        racer.SetSuddenStopLength(entry.suddenStopLength);
        if (setup.cheats.Has(Cheat::IgnoreSurfaces)) {
            racer.IgnoreSurfaces();
        }
        racer.ScaleGravity(setup.modifiers.gravityScale);
        racer.ScaleSpeed(setup.modifiers.speedScale);
        if (entry.isPlayer) {
            GiveRacerCheats(racer, setup.playerCheats);
        }
        const std::span<const assets::RaceRoute> routes = RoutesFor(course, setup.firstSlot + i);
        drivers_.emplace_back();
        if (!entry.isPlayer && !routes.empty()) {
            const std::size_t variant = random_.Below(static_cast<std::uint32_t>(routes.size()));
            const assets::RaceRoute& route = routes[variant];
            if (setup.aiDriving == AiDriving::Physics) {
                std::optional<SkillProfile> skill;
                if (setup.aiSkill) {
                    skillLevels_[i] = LevelFor(*setup.aiSkill, setup.firstSlot + i, route.pace);
                    skill = ProfileFor(skillLevels_[i]);
                }
                drivers_.back().emplace(route, setup.aiPace, setup.aiCraft, skill,
                                        LineFor(setup.firstSlot + i, variant));
            } else {
                racer.FollowRoute(route);
            }
        }
        isPlayer_.push_back(entry.isPlayer);
        localBounds_.push_back(entry.localBounds);
        aiPowerups_.emplace_back(entry.habits);
        taunts_.emplace_back();
        positions_[i] = racer.Body().motion.position;
    }
}

void RaceSession::Tick(Milliseconds elapsed, std::span<const RacerControls> controls) {
    RR_ASSERT(controls.size() == racers_.size());
    entities_.Tick(elapsed);
    sounds_.BeginTick();
    powerups_.BeginTick();
    weapons_.Clear();
    triggers_.BeginTick();
    resources_.BeginTick();
    PowerupContext hazardEffects = EffectContext();
    HazardContext hazards = HazardsContext(elapsed, hazardEffects);
    if (!rules_->racersStepFirst) {
        TickHazards(elapsed, hazards);
    }
    MeasureReaches();
    std::ranges::copy(positions_, rivals_.begin());
    const std::span<const RacerControls> stepped = lagsInput_ ? lastControls_ : controls;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        TickRacer(i, elapsed,
                  clock_.Phase() == RacePhase::Intro ? RacerControls{} : ControlsOf(i, stepped));
    }
    std::ranges::copy(controls, lastControls_.begin());
    ResolveContacts();
    TickBricks(elapsed);
    if (rules_->racersStepFirst) {
        TickHazards(elapsed, hazards);
    }
    MeasureReaches();
    hazards.bodies = contactBodies_;
    hazards_.Touch(hazards);
    TickTriggers(elapsed);
    SwitchOnTriggerWorlds();
    PowerupContext effects = EffectContext();
    powerups_.Tick(elapsed, effects);
    explosions_.Tick(elapsed, racers_, reaches_);
    spikes_.Tick(elapsed, racers_, reaches_);
    sounds_.HearWeapons(weapons_.Events());
    progress_.Rank(*course_.checkpoints, positions_);
    ReactToPlaces();
    ReleaseComputerRacers(elapsed);
    AdvanceClock(elapsed);
    for (Racer& racer : racers_) {
        racer.UpdateTimers(elapsed);
    }
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        sounds_.HearRacer(i, racers_[i], isPlayer_[i], clock_.IsRunning(), elapsed);
    }
    PaceRoutes(elapsed);
    hazards_.Hear(triggers_, hazards);
    resources_.Dispatch(triggers_, entities_);
    tints_.Apply(triggers_.Events());
    progress_.EnterZones(triggers_.Events(), EventSourcesFor(course_).table.lapZones);
    targets_.Hear(triggers_.Events());
    KeepWhitesFull();
}

void RaceSession::AdvanceClock(Milliseconds elapsed) {
    const RaceClock before = clock_;
    clock_.Advance(elapsed, HasPlayerFinished());
    sounds_.HearClock(before, clock_, isPlayer_);
    if (before.Phase() == RacePhase::Countdown && clock_.Phase() == RacePhase::Racing) {
        StartRacing();
    }
    if (before.Phase() == RacePhase::Finished && clock_.Phase() == RacePhase::Results) {
        progress_.FinishRemaining();
    }
}

RacerControls RaceSession::ControlsOf(std::size_t racer, std::span<const RacerControls> controls) {
    std::optional<RouteDriver>& driver = drivers_[racer];
    if (!driver) {
        return controls[racer];
    }
    const Racer& self = racers_[racer];
    const std::optional<Vector3> aim =
        driver->Craft().aims
            ? AimFor(inventories_[racer].Colour(), rivals_, racer, self.Body().motion)
            : std::nullopt;
    return driver->Drive(self, rivals_, racer, aim);
}

void RaceSession::StartRacing() {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (isPlayer_[i] || headStartLeft_ == Milliseconds{0}) {
            racers_[i].Go();
        }
    }
    progress_.StartZones();
    if (headStartLeft_ == Milliseconds{0}) {
        StartBoosts();
    }
}

void RaceSession::ReleaseComputerRacers(Milliseconds elapsed) {
    if (headStartLeft_ == Milliseconds{0} || !clock_.IsRunning()) {
        return;
    }
    headStartLeft_ = std::max(Milliseconds{0}, headStartLeft_ - elapsed);
    if (headStartLeft_ == Milliseconds{0}) {
        for (std::size_t i = 0; i < racers_.size(); ++i) {
            if (!isPlayer_[i]) {
                racers_[i].Go();
            }
        }
        StartBoosts();
    }
}

void RaceSession::ResolveContacts() {
    contactWorlds_.clear();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        contactBodies_[i] = racers_[i].ForContacts(contactWorlds_.emplace_back(WorldsOf(i)));
    }
    contacts_.Resolve(contactBodies_);
    for (const RacerContact& contact : contacts_.Contacts()) {
        ApplyContactEffects(racers_, contact, random_);
    }
}

std::shared_ptr<const RacingLine> RaceSession::LineFor(std::size_t slot,
                                                       std::size_t variant) const {
    if (slot >= lines_.size() || variant >= lines_[slot].size()) {
        return nullptr;
    }
    return lines_[slot][variant];
}

void RaceSession::PaceRoutes(Milliseconds elapsed) {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        paced_[i] = {.lapProgress = progress_.OfRacer(i).laps.Progress(*course_.checkpoints),
                     .isAiControlled = !isPlayer_[i]};
    }
    rubberBand_.Update(elapsed, paced_, routeSpeeds_);
    TuneSkills();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        racers_[i].SetRouteSpeed(routeSpeeds_[i] * modifiers_.speedScale);
    }
}

void RaceSession::TuneSkills() {
    if (!catchesUp_ || clock_.RaceTime() < kCatchUpDelay) {
        return;
    }
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        std::optional<RouteDriver>& driver = drivers_[i];
        if (driver.has_value() && skillLevels_[i] != 0) {
            driver->SetSkill(ProfileFor(skillLevels_[i] + CatchUpShift(paced_, i)));
        }
    }
}

void RaceSession::MeasureReaches() {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        reaches_[i] = {.centre = Apply(racers_[i].Pose(), localBounds_[i].centre),
                       .radius = localBounds_[i].radius};
    }
}

void RaceSession::TickTriggers(Milliseconds elapsed) {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        triggerRacers_[i] = {.reach = reaches_[i],
                             .isPlayer = isPlayer_[i],
                             .completedLaps = progress_.OfRacer(i).laps.CompletedLaps()};
    }
    triggers_.TestSpheres(triggerRacers_);
    resources_.Dispatch(triggers_, entities_);
    resources_.Update(elapsed, triggers_, entities_);
    triggers_.TickTimers(elapsed, random_);
    resources_.Dispatch(triggers_, entities_);
}

void RaceSession::TickHazards(Milliseconds elapsed, HazardContext& hazards) {
    for (TriggerWorlds& worlds : switchedWorlds_) {
        worlds.Tick(elapsed);
    }
    hazards_.Tick(hazards);
}

void RaceSession::SwitchOnTriggerWorlds() {
    for (std::size_t sphere = 0; sphere < sphereWorlds_.size(); ++sphere) {
        const std::optional<std::size_t> world = sphereWorlds_[sphere];
        const std::uint8_t inside = triggers_.RacerSpheres().Inside(sphere);
        for (std::size_t racer = 0; world && racer < switchedWorlds_.size(); ++racer) {
            if ((inside >> racer & 1U) != 0) {
                switchedWorlds_[racer].SwitchOn(*world);
            }
        }
    }
}

RacerWorldView RaceSession::WorldsOf(std::size_t racer) const {
    return {*course_.world, course_.collision, switchedWorlds_[racer], surfaces_,
            resources_.MovedWorlds()};
}

void RaceSession::ReactToPlaces() {
    const std::span<const std::size_t> standings = progress_.Standings();
    for (std::size_t place = 0; place < standings.size(); ++place) {
        std::size_t& last = places_[standings[place]];
        if (clock_.IsRunning() && place != last) {
            racers_[standings[place]].React(place < last ? VoiceReaction::Positive
                                                         : VoiceReaction::Negative);
        }
        last = place;
    }
}

void RaceSession::TickRacer(std::size_t racer, Milliseconds elapsed,
                            const RacerControls& controls) {
    Racer& car = racers_[racer];
    const Vector3 from = positions_[racer];
    const bool hasSkipped = clock_.IsRunning() && UseButtonCheats(racer, controls);
    car.RollCurse(elapsed, random_);
    const RacerWorldView worlds = WorldsOf(racer);
    car.Tick(elapsed, controls, car.Route() ? worlds.WithoutBase() : worlds);
    if (progress_.OfRacer(racer).laps.CompletedLaps() < progress_.LapCount()) {
        car.WatchForSuddenStop(elapsed);
    }
    surfaceEvents_.Hear(static_cast<std::uint8_t>(racer), car, surfaces_, triggers_);
    const BodyMotion& motion = car.Body().motion;
    std::optional<RouteDriver>& driver = drivers_[racer];
    const bool hasFallen =
        !car.Route() && respawns_[racer].Update(respawn_, elapsed, IsGrounded(motion),
                                                motion.position.z, course_.lowestTrackZ);
    const bool hasStalled =
        driver && clock_.IsRunning() && !car.Boost().isHeldAtStart && driver->IsStalled(elapsed);
    wasRespawned_[racer] = hasFallen || hasStalled || hasSkipped;
    if (hasFallen || hasStalled) {
        car.Place(driver ? driver->Rejoin()
                         : RespawnPoint(*course_.checkpoints,
                                        progress_.OfRacer(racer).laps.LastCheckpoint(),
                                        course_.grid[racer], *course_.world));
    } else {
        const bool hadFinished = progress_.OfRacer(racer).finishPlace.has_value();
        progress_.Move(*course_.checkpoints, racer,
                       {.from = from,
                        .to = motion.position,
                        .facing = car.Body().facing,
                        .isAtFinish = car.Events().touches.HasReachedFinish()},
                       elapsed, clock_.IsRunning());
        if (!hadFinished && progress_.OfRacer(racer).finishPlace) {
            sounds_.HearFinish(racer);
        }
    }
    positions_[racer] = car.Body().motion.position;
    if (clock_.IsRunning() && WantsPowerup(racer, elapsed, controls)) {
        UsePowerup(racer);
    }
    if (clock_.IsRunning() && !isPlayer_[racer] &&
        taunts_[racer].Tick(elapsed, racers_, racer, random_)) {
        Taunt(racer);
    }
}

HazardContext RaceSession::HazardsContext(Milliseconds elapsed, PowerupContext& effects) {
    return {.racers = racers_,
            .reaches = reaches_,
            .entities = &entities_,
            .events = &triggers_,
            .surfaces = &surfaces_,
            .models = &hazards_.Models(),
            .random = &random_,
            .elapsed = elapsed,
            .world = course_.world,
            .explosions = &explosions_,
            .effects = &effects,
            .isMirrored = course_.isMirrored};
}

bool RaceSession::HasPlayerFinished() const {
    if (isDemo_ && !racers_.empty() && progress_.OfRacer(0).finishPlace) {
        return true;
    }
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (isPlayer_[i] && progress_.OfRacer(i).finishPlace) {
            return true;
        }
    }
    return false;
}

}
