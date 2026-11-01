#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/hazard_list.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/physics/plane_world.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"

namespace rr::test {

inline constexpr std::size_t kRigNodes = 4;
inline constexpr float kRigReach = 4.0F;
inline constexpr Milliseconds kRigTick{12};

/// One entity per name, standing at `at` with every node at its origin, on a 100-frame loop.
inline sim::TrackEntitySources StillEntities(const std::vector<std::string>& names,
                                             const Vector3& at) {
    sim::TrackEntitySources sources{
        .skeletons = {{.nodes = {{.name = "root"},
                                 {.name = "one", .parent = 0},
                                 {.name = "two", .parent = 1},
                                 {.name = "three", .parent = 2}}}},
        .animations = {
            {.tracks = std::vector<assets::AnimationTrack>(kRigNodes),
             .parts = {
                 {.name = "a0", .frameCount = 100, .loopFrames = 101, .framesPerSecond = 30}}}}};
    for (const std::string& name : names) {
        sources.entities.push_back({.name = name, .startPart = 0, .placement = {.position = at}});
    }
    return sources;
}

inline Result<sim::RaceSession> PlayerSession(const sim::RaceTrack& track,
                                              const sim::CollisionQuery& world) {
    const sim::RaceSetup setup{.racers = {{.isPlayer = true}}};
    return sim::RaceSession::Create(setup, {.checkpoints = &track.checkpoints,
                                            .grid = track.grid,
                                            .world = &world,
                                            .lowestTrackZ = track.lowestTrackZ,
                                            .events = &track.events,
                                            .collision = &track.collision,
                                            .entities = &track.entities,
                                            .hazards = track.hazards});
}

inline std::optional<sim::RaceTrack> RetailRaceTrack(std::string_view name) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    Result<sim::RaceTrack> track = sim::LoadRaceTrack(*vfs, name);
    REQUIRE(track.has_value());
    return std::move(*track);
}

/// Racers on flat ground with hazards that run only when told to.
class HazardRig {
public:
    HazardRig(std::vector<assets::Hazard> hazards, sim::TrackEntitySources sources,
              std::size_t racerCount = 1)
        : sources_(std::move(sources)),
          hazardList_(std::move(hazards)),
          entities_(sources_),
          hazards_(hazardList_, entities_, random_),
          events_({}, racerCount, false, random_,
                  sim::Hazards::MostEventsPerTick(hazardList_, racerCount)),
          heard_({}, racerCount, false, random_, 1),
          reaches_(racerCount) {
        for (std::size_t i = 0; i < racerCount; ++i) {
            racers_.emplace_back(
                sim::ChassisSpec{}, sim::CarBuild{},
                sim::RacerStart{.position = {.y = static_cast<float>(i) * 50.0F, .z = 0.2F}});
        }
        MeasureReaches();
    }

    HazardRig(const HazardRig&) = delete;
    HazardRig(HazardRig&&) = delete;
    HazardRig& operator=(const HazardRig&) = delete;
    HazardRig& operator=(HazardRig&&) = delete;
    ~HazardRig() = default;

    void Hear(assets::EventId id, sim::EventEdge edge,
              std::optional<Vector3> position = std::nullopt) {
        Hear({.id = id, .edge = edge, .position = position});
    }

    void Hear(const sim::TrackEvent& event) {
        sim::HazardContext context = Context();
        heard_.BeginTick();
        heard_.Fire(event);
        hazards_.Hear(heard_, context);
    }

    void UseSurfaces(sim::TrackSurfaces surfaces) { surfaces_ = std::move(surfaces); }

    void Seek(std::size_t entity, float frame) { entities_.Seek(entity, frame); }

    void Tick() {
        events_.BeginTick();
        sim::HazardContext context = Context();
        hazards_.Tick(context);
    }

    void Run(Milliseconds time) {
        for (Milliseconds run{0}; run < time; run += kRigTick) {
            entities_.Tick(kRigTick);
            Tick();
        }
    }

    void Touch() {
        std::vector<sim::ContactBody> bodies;
        bodies.reserve(racers_.size());
        for (sim::Racer& racer : racers_) {
            bodies.push_back(racer.ForContacts(world_));
        }
        sim::HazardContext context = Context();
        context.bodies = bodies;
        hazards_.Touch(context);
    }

    void Drive(int ticks) {
        for (int tick = 0; tick < ticks; ++tick) {
            for (sim::Racer& racer : racers_) {
                racer.Tick(kRigTick, {.isThrottleHeld = true}, world_);
            }
        }
        MeasureReaches();
    }

    void Place(std::size_t racer, const Vector3& position) {
        racers_[racer].Place({.position = position});
        MeasureReaches();
    }

    [[nodiscard]] std::size_t CountStarts(assets::EventId id) const {
        std::size_t count = 0;
        for (const sim::TrackEvent& event : events_.Events()) {
            count += event.id == id && event.edge == sim::EventEdge::Start ? 1 : 0;
        }
        return count;
    }

    [[nodiscard]] std::span<const sim::TrackEvent> Events() const { return events_.Events(); }
    [[nodiscard]] const sim::Hazards& Hazards() const { return hazards_; }
    [[nodiscard]] const sim::TrackSurfaces& Surfaces() const { return surfaces_; }
    [[nodiscard]] const sim::TrackEntities& Entities() const { return entities_; }
    [[nodiscard]] const sim::Explosions& Blasts() const { return explosions_; }
    [[nodiscard]] sim::Racer& Car(std::size_t racer = 0) { return racers_[racer]; }

private:
    sim::HazardContext Context() {
        return {.racers = racers_,
                .reaches = reaches_,
                .entities = &entities_,
                .events = &events_,
                .surfaces = &surfaces_,
                .models = &hazards_.Models(),
                .random = &random_,
                .elapsed = kRigTick,
                .world = &world_,
                .explosions = &explosions_};
    }

    void MeasureReaches() {
        for (std::size_t i = 0; i < racers_.size(); ++i) {
            reaches_[i] = {.centre = racers_[i].Pose().position, .radius = kRigReach};
        }
    }

    sim::TrackEntitySources sources_;
    std::vector<assets::Hazard> hazardList_;
    sim::TrackEntities entities_;
    sim::Random random_{1};
    sim::Hazards hazards_;
    sim::TrackSurfaces surfaces_;
    sim::Explosions explosions_;
    sim::TrackEvents events_;
    sim::TrackEvents heard_;
    PlaneWorld world_{{{}}};
    std::vector<sim::Racer> racers_;
    std::vector<BoundingSphere> reaches_;
};

}
