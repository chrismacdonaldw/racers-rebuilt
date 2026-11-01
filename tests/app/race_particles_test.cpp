#include "app/race_particles.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../sim/physics/plane_world.hpp"
#include "../sim/race/checkpoint_ring.hpp"
#include "sim/physics/racer_controls.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kGridSize = 6;
constexpr Milliseconds kTick{12};
constexpr render::FloatMilliseconds kFrame{12.0F};
constexpr float kGrassEndX = 0.0F;
constexpr Transform kCamera{.position = {.x = -20.0F, .y = 5.0F}};

render::ParticleBankSetup Bank(std::string emitter, std::size_t slots) {
    return {.emitters = {{.name = std::move(emitter),
                          .emitRate = 50.0F,
                          .points = {{.z = 1.0F}},
                          .particleLife = Milliseconds{500},
                          .radius = 1000.0F,
                          .priority = 10}},
            .emitterLooks = {render::ParticleLook{}},
            .slotCount = slots};
}

std::vector<sim::RacerStart> Grid() {
    std::vector<sim::RacerStart> grid;
    grid.reserve(kGridSize);
    for (std::size_t slot = 0; slot < kGridSize; ++slot) {
        grid.push_back(
            {.position = {.x = -20.0F, .y = static_cast<float>(slot) * -10.0F, .z = 0.2F}});
    }
    return grid;
}

class GrassRace {
public:
    explicit GrassRace(std::string particle)
        : surfaces_{{.name = "grass", .particle = std::move(particle)}},
          world_({test::TestPlane{.surface = {.tableEntry = 0}, .maxX = kGrassEndX},
                  test::TestPlane{.minX = kGrassEndX}}),
          session_(Start()),
          particles_(Bank("grassemt", kCommonParticleSlots), Bank("snowkick", kTrackParticleSlots),
                     surfaces_, 0) {
        while (session_.Clock().Phase() != sim::RacePhase::Racing) {
            Tick();
        }
    }

    void Hold(std::size_t racer) { controls_.at(racer).isThrottleHeld = true; }

    void Tick() {
        session_.Tick(kTick, controls_);
        particles_.Update(session_, kFrame);
    }

    [[nodiscard]] std::size_t QuadCount() const {
        std::vector<render::QuadDraw> quads;
        particles_.AppendQuads(kCamera, quads);
        return quads.size();
    }

    [[nodiscard]] RaceParticles& Particles() { return particles_; }
    [[nodiscard]] sim::Racer& Player() { return session_.Racers()[0]; }

    [[nodiscard]] float RacerX(std::size_t racer) const {
        return session_.Racers()[racer].Body().motion.position.x;
    }

private:
    sim::RaceSession Start() {
        sim::RaceSetup setup{.racers = std::vector<sim::RacerEntry>(kGridSize)};
        setup.racers[0].isPlayer = true;
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            setup, {.checkpoints = &graph_, .grid = grid_, .world = &world_, .lowestTrackZ = -100});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    std::vector<assets::SurfaceEntry> surfaces_;
    sim::CheckpointGraph graph_ = test::Ring();
    test::PlaneWorld world_;
    std::vector<sim::RacerStart> grid_ = Grid();
    sim::RaceSession session_;
    RaceParticles particles_;
    std::vector<sim::RacerControls> controls_ = std::vector<sim::RacerControls>(kGridSize);
};

TEST_CASE("the player's wheels kick up grass while rolling and stop on the road") {
    GrassRace race("grassemt");
    CHECK(race.QuadCount() == 0);

    race.Hold(0);
    for (int tick = 0; tick < 20; ++tick) {
        race.Tick();
    }
    CHECK(race.QuadCount() >= static_cast<std::size_t>(sim::kWheelCount));

    while (race.RacerX(0) < kGrassEndX + 10.0F) {
        race.Tick();
    }
    CHECK(race.QuadCount() == 0);
}

TEST_CASE("a spun-out player kicks up nothing") {
    GrassRace race("grassemt");
    race.Hold(0);
    for (int tick = 0; tick < 20; ++tick) {
        race.Tick();
    }
    REQUIRE(race.QuadCount() > 0);

    race.Player().StartSpinOut();
    race.Tick();

    CHECK(race.QuadCount() == 0);
}

TEST_CASE("computer racers kick up nothing") {
    GrassRace race("grassemt");
    for (std::size_t racer = 1; racer < kGridSize; ++racer) {
        race.Hold(racer);
    }

    for (int tick = 0; tick < 50; ++tick) {
        race.Tick();
    }

    CHECK(race.RacerX(1) > Grid()[1].position.x + 1.0F);
    CHECK(race.QuadCount() == 0);
}

TEST_CASE("a wheel particle missing from the common file comes from the track's") {
    GrassRace race("snowkick");
    race.Hold(0);

    for (int tick = 0; tick < 20; ++tick) {
        race.Tick();
    }

    CHECK(race.QuadCount() > 0);
}

TEST_CASE("other effects start emitters by name in either file") {
    GrassRace race("grassemt");

    const std::optional<render::EmitterHandle> common =
        race.Particles().Bank(ParticleFile::Common).Start("grassemt", {});
    const std::optional<render::EmitterHandle> track =
        race.Particles().Bank(ParticleFile::Track).Start("snowkick", {});
    race.Tick();

    REQUIRE((common && track));
    CHECK(race.QuadCount() == 2);
    CHECK_FALSE(race.Particles().Bank(ParticleFile::Track).HasEmitter("grassemt"));
}
}
}
