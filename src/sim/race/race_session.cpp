#include "sim/race/race_session.hpp"

#include <cstdint>
#include <format>

#include "assets/game_path.hpp"
#include "core/assert.hpp"
#include "core/transform.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/contact_effects.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kMostWeaponEventsPerEffect = 2;
constexpr std::size_t kMostShotCastsPerEffect = 2;
constexpr std::size_t kEventsPerShotCast = 2;

BrickField BricksFor(const RaceCourse& course) {
    return course.bricks == nullptr ? BrickField() : BrickField(*course.bricks);
}

std::size_t BrickCount(const RaceCourse& course) {
    return course.bricks == nullptr ? 0
                                    : course.bricks->coloured.size() + course.bricks->white.size();
}

std::size_t EffectSlots(const RaceSetup& setup) {
    std::size_t slots = 0;
    for (std::size_t kind = 0; kind < kPowerupKindCount; ++kind) {
        slots += setup.effects.at(kind) ? SpecFor(static_cast<PowerupKind>(kind)).poolSize : 0;
    }
    return slots;
}

std::size_t WeaponEventCount(const RaceSetup& setup) {
    return (EffectSlots(setup) * kMostWeaponEventsPerEffect) + (setup.racers.size() * kMaxLaunches);
}

bool AreBricksAudible(const RaceClock& clock) {
    return clock.Phase() != RacePhase::Intro && clock.Phase() != RacePhase::Countdown;
}

const TrackEventSources& EventSourcesFor(const RaceCourse& course) {
    static const TrackEventSources kNone;
    return course.events == nullptr ? kNone : *course.events;
}

std::size_t OutsideEventCount(const RaceSetup& setup, const RaceCourse& course) {
    return (EffectSlots(setup) * kMostShotCastsPerEffect * kEventsPerShotCast) +
           (setup.racers.size() * SurfaceEvents::MostEventsPerRacer()) +
           EventResources::MostFiredPerTick(EventSourcesFor(course).table) +
           Hazards::MostEventsPerTick(course.hazards, setup.racers.size());
}

TrackSurfaces SurfacesFor(const RaceCourse& course) {
    return course.collision == nullptr ? TrackSurfaces()
                                       : TrackSurfaces(course.collision->surfaces);
}

std::optional<std::size_t> FindWorld(std::span<const NamedWorld> worlds, std::string_view name) {
    const std::string wanted = assets::ToUpperAscii(name);
    const auto found = std::ranges::find_if(worlds, [&](const NamedWorld& world) {
        return assets::ToUpperAscii(world.name) == wanted;
    });
    if (found == worlds.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - worlds.begin());
}

std::vector<std::optional<std::size_t>> SphereWorldsFor(const RaceCourse& course) {
    const std::vector<assets::TriggerSphere>& spheres = EventSourcesFor(course).racerSpheres;
    std::vector<std::optional<std::size_t>> worlds(spheres.size());
    for (std::size_t i = 0; course.collision != nullptr && i < spheres.size(); ++i) {
        if (spheres[i].collisionWorld) {
            worlds[i] = FindWorld(course.collision->worlds, *spheres[i].collisionWorld);
        }
    }
    return worlds;
}

const TrackEntitySources& EntitySourcesFor(const RaceCourse& course) {
    static const TrackEntitySources kNone;
    return course.entities == nullptr ? kNone : *course.entities;
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
      sounds_(setup.racers.size(), BrickCount(course), WeaponEventCount(setup)),
      random_(setup.seed),
      surfaces_(SurfacesFor(course)),
      triggers_(EventSourcesFor(course), setup.racers.size(), course.isMirrored, random_,
                OutsideEventCount(setup, course)),
      entities_(EntitySourcesFor(course)),
      hazards_(course.hazards, entities_, random_),
      resources_(EventSourcesFor(course).table, EntitySourcesFor(course), course.collision),
      triggerRacers_(setup.racers.size()),
      sphereWorlds_(SphereWorldsFor(course)),
      switchedWorlds_(setup.racers.size()),
      surfaceEvents_(setup.racers.size()),
      rubberBand_(course.isMirrored),
      paced_(setup.racers.size()),
      routeSpeeds_(setup.racers.size(), 1.0F),
      bricks_(BricksFor(course)),
      inventories_(setup.racers.size()),
      powerups_(setup.effects, setup.racers.size()),
      weapons_(WeaponEventCount(setup)),
      reaches_(setup.racers.size()),
      contactBodies_(setup.racers.size()) {
    racers_.reserve(setup.racers.size());
    contactWorlds_.reserve(setup.racers.size());
    localBounds_.reserve(setup.racers.size());
    for (std::size_t i = 0; i < setup.racers.size(); ++i) {
        const RacerEntry& entry = setup.racers[i];
        Racer& racer = racers_.emplace_back(entry.chassis, entry.build, course.grid[i]);
        racer.HoldAtStart();
        const std::span<const assets::RaceRoute> routes = RoutesFor(course, i);
        if (!entry.isPlayer && !routes.empty()) {
            racer.FollowRoute(routes[random_.Below(static_cast<std::uint32_t>(routes.size()))]);
        }
        isPlayer_.push_back(entry.isPlayer);
        localBounds_.push_back(entry.localBounds);
        aiPowerups_.emplace_back(entry.habits);
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
    for (TriggerWorlds& worlds : switchedWorlds_) {
        worlds.Tick(elapsed);
    }
    HazardContext hazards = HazardsContext(elapsed);
    hazards_.Tick(hazards);
    MeasureReaches();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        TickRacer(i, elapsed, clock_.Phase() == RacePhase::Intro ? RacerControls{} : controls[i]);
    }
    ResolveContacts();
    TickBricks(elapsed);
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
    const RaceClock before = clock_;
    clock_.Advance(elapsed, HasPlayerFinished());
    sounds_.HearClock(before, clock_, isPlayer_);
    if (before.Phase() == RacePhase::Countdown && clock_.Phase() == RacePhase::Racing) {
        for (Racer& racer : racers_) {
            racer.Go();
        }
        StartBoosts();
    }
    if (before.Phase() == RacePhase::Finished && clock_.Phase() == RacePhase::Results) {
        progress_.FinishRemaining();
    }
    for (Racer& racer : racers_) {
        racer.UpdateTimers(elapsed);
    }
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        sounds_.HearRacer(i, racers_[i], isPlayer_[i], clock_.IsRunning(), elapsed);
    }
    PaceRoutes(elapsed);
    hazards_.Hear(triggers_, hazards);
    resources_.Dispatch(triggers_, entities_);
}

bool RaceSession::WantsPowerup(std::size_t racer, Milliseconds elapsed,
                               const RacerControls& controls) {
    if (isPlayer_[racer]) {
        return controls.isUsePressed;
    }
    return aiPowerups_[racer].Tick(
        elapsed,
        {.racers = racers_,
         .racer = racer,
         .inventory = &inventories_[racer],
         .isCrowded = powerups_.IsCrowded(),
         .hasFinished = progress_.OfRacer(racer).finishPlace.has_value()},
        random_);
}

void RaceSession::StartBoosts() {
    PowerupContext effects = EffectContext();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (!isPlayer_[i] && aiPowerups_[i].RollsStartBoost(random_)) {
            powerups_.Fire({.racer = static_cast<std::uint8_t>(i),
                            .colour = assets::BrickColour::Green,
                            .level = 0},
                           effects);
        }
    }
}

void RaceSession::ResolveContacts() {
    contactWorlds_.clear();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        contactBodies_[i] = racers_[i].ForContacts(contactWorlds_.emplace_back(WorldsOf(i)));
    }
    contacts_.Resolve(contactBodies_);
    for (const RacerContact& contact : contacts_.Contacts()) {
        ApplyContactEffects(racers_[contact.first], racers_[contact.second], contact.point,
                            random_);
    }
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

void RaceSession::GivePowerup(std::size_t racer, assets::BrickColour colour, std::uint8_t level) {
    PowerupInventory& inventory = inventories_.at(racer);
    bricks_.Spend(inventory);
    inventory.Grant(colour, level);
}

PowerupContext RaceSession::EffectContext() {
    return {.racers = racers_,
            .world = course_.world,
            .random = &random_,
            .checkpoints = course_.checkpoints,
            .progress = &progress_,
            .reaches = reaches_,
            .powerups = &powerups_,
            .bricks = &bricks_,
            .inventories = inventories_,
            .explosions = &explosions_,
            .spikes = &spikes_,
            .weapons = &weapons_,
            .surfaces = &surfaces_,
            .trackEvents = &triggers_};
}

void RaceSession::TickBricks(Milliseconds elapsed) {
    bricks_.Tick(elapsed, reaches_, inventories_);
    sounds_.HearBricks(bricks_.Events(), AreBricksAudible(clock_));
}

void RaceSession::TickRacer(std::size_t racer, Milliseconds elapsed,
                            const RacerControls& controls) {
    Racer& car = racers_[racer];
    const Vector3 from = positions_[racer];
    car.RollCurse(elapsed, random_);
    car.Tick(elapsed, controls, WorldsOf(racer));
    surfaceEvents_.Hear(static_cast<std::uint8_t>(racer), car, surfaces_, triggers_);
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
    if (clock_.IsRunning() && WantsPowerup(racer, elapsed, controls)) {
        PowerupContext effects = EffectContext();
        if (isPlayer_[racer]) {
            effects.targets = course_.targets;
        }
        powerups_.Use(racer, inventories_[racer], bricks_, effects);
    }
}

HazardContext RaceSession::HazardsContext(Milliseconds elapsed) {
    return {.racers = racers_,
            .reaches = reaches_,
            .entities = &entities_,
            .events = &triggers_,
            .surfaces = &surfaces_,
            .models = &hazards_.Models(),
            .random = &random_,
            .elapsed = elapsed,
            .world = course_.world,
            .explosions = &explosions_};
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
