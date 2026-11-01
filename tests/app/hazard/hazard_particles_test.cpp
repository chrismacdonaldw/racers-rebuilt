#include "app/hazard/hazard_particles.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../../sim/hazard/hazard_rig.hpp"
#include "../../sim/physics/plane_world.hpp"
#include "../../sim/race/checkpoint_ring.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};
constexpr render::FloatMilliseconds kFrame{12.0F};
constexpr Transform kCamera{.position = {.x = 5.0F, .y = 20.0F}};

render::ParticleBankSetup Bank(const std::vector<std::string>& names) {
    render::ParticleBankSetup setup{.slotCount = 4};
    for (const std::string& name : names) {
        setup.emitters.push_back({.name = name,
                                  .emitRate = 50.0F,
                                  .points = {{}},
                                  .particleLife = Milliseconds{500},
                                  .radius = 1000.0F,
                                  .priority = 10});
        setup.emitterLooks.emplace_back(render::ParticleLook{});
    }
    return setup;
}

class HazardRace {
public:
    explicit HazardRace(std::vector<assets::Hazard> hazards, sim::TrackEventSources events = {})
        : events_(std::move(events)), hazards_(std::move(hazards)), session_(Start()) {}

    void Tick() {
        session_.Tick(kTick, controls_);
        particles_.Tick(session_, kCamera, bank_, kTick);
        bank_.Update(kFrame);
    }

    [[nodiscard]] std::vector<render::QuadDraw> Quads() const {
        std::vector<render::QuadDraw> quads;
        bank_.AppendQuads(kCamera, quads);
        return quads;
    }

private:
    sim::RaceSession Start() {
        const sim::RaceSetup setup{.racers = {{.isPlayer = true}}};
        Result<sim::RaceSession> session = sim::RaceSession::Create(setup, {.checkpoints = &graph_,
                                                                            .grid = grid_,
                                                                            .world = &world_,
                                                                            .lowestTrackZ = -100,
                                                                            .events = &events_,
                                                                            .entities = &entities_,
                                                                            .hazards = hazards_});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    sim::CheckpointGraph graph_ = test::Ring();
    test::PlaneWorld world_{{test::TestPlane{}}};
    std::vector<sim::RacerStart> grid_{{.position = {.z = 0.2F}}};
    sim::TrackEntitySources entities_ = test::StillEntities({"dp_def", "ghostly"}, {});
    sim::TrackEventSources events_;
    std::vector<assets::Hazard> hazards_;
    sim::RaceSession session_;
    render::ParticleBank bank_{Bank({"snow", "smoke", "ghsttrl"})};
    HazardParticles particles_{0};
    std::vector<sim::RacerControls> controls_ = std::vector<sim::RacerControls>(1);
};

bool IsAheadOfTheCamera(const render::QuadDraw& quad) {
    const Vector3 centre =
        (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) * 0.25F;
    return std::abs(centre.z - (kCamera.position.z - 100.0F)) < 0.01F &&
           std::abs(centre.y - (kCamera.position.y - 40.0F)) < 0.01F &&
           std::abs(centre.x - kCamera.position.x) <= 100.0F;
}

}

TEST_CASE("snow falls 100 ahead of the camera, 40 below and up to 100 aside") {
    HazardRace race({{.kind = assets::HazardKind::Snowfall}});

    for (int tick = 0; tick < 5; ++tick) {
        race.Tick();
    }

    const std::vector<render::QuadDraw> quads = race.Quads();
    REQUIRE_FALSE(quads.empty());
    CHECK(std::ranges::all_of(quads, IsAheadOfTheCamera));
}

TEST_CASE("no snow falls while the player's car is tinted") {
    sim::TrackEventSources indoors;
    indoors.racerSpheres.push_back({.radius = 50.0F, .event = 4});
    indoors.table.colorTransforms.push_back(
        {.trigger = {.event = 4, .ignoresEnd = true}, .shifts = {1, 1, 1, 0}});
    HazardRace race({{.kind = assets::HazardKind::Snowfall}}, std::move(indoors));

    for (int tick = 0; tick < 5; ++tick) {
        race.Tick();
    }

    CHECK(race.Quads().empty());
}

TEST_CASE("the smoke vent and the ghost trail wait for their trigger") {
    HazardRace race({{.kind = assets::HazardKind::SmokeVent}, {.kind = assets::HazardKind::Ghost}});

    for (int tick = 0; tick < 5; ++tick) {
        race.Tick();
    }

    CHECK(race.Quads().empty());
}

}
