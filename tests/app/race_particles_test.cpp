#include "app/race_particles.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "../sim/physics/plane_world.hpp"
#include "../sim/race/checkpoint_ring.hpp"
#include "app/race_fx.hpp"
#include "app/track_space.hpp"
#include "sim/physics/racer_controls.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kGridSize = 6;
constexpr Milliseconds kTick{12};
constexpr render::FloatMilliseconds kFrame{12.0F};
constexpr float kGrassEndX = 0.0F;
constexpr Transform kCamera{.position = {.x = -20.0F, .y = 5.0F}};

render::ParticleBankSetup Bank(const std::vector<std::string>& names, std::size_t slots) {
    render::ParticleBankSetup setup{.slotCount = slots};
    for (const std::string& name : names) {
        setup.emitters.push_back({.name = name,
                                  .emitRate = 50.0F,
                                  .points = {{.z = 1.0F}},
                                  .particleLife = Milliseconds{500},
                                  .radius = 1000.0F,
                                  .priority = 10});
        setup.emitterLooks.emplace_back(render::ParticleLook{});
    }
    return setup;
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
    explicit GrassRace(std::string particle, std::size_t players = 1)
        : players_(players),
          surfaces_{{.name = "grass", .particle = std::move(particle)}},
          world_({test::TestPlane{.surface = {.tableEntry = 0}, .maxX = kGrassEndX},
                  test::TestPlane{.minX = kGrassEndX}}),
          session_(Start()),
          particles_(Bank({"grassemt", "tiresmk"}, kCommonParticleSlots),
                     Bank({"snowkick"}, kTrackParticleSlots),
                     std::make_unique<render::FxSystem>(RaceFxEffects(), render::kWhiteTexture),
                     surfaces_, 0) {
        while (session_.Clock().Phase() != sim::RacePhase::Racing) {
            Tick();
        }
    }

    void Hold(std::size_t racer) { controls_.at(racer).isThrottleHeld = true; }
    void Skid(std::optional<SkidWheels> wheels) { skidding_.front() = wheels; }

    void Tick() {
        session_.Tick(kTick, controls_);
        particles_.Update(session_, std::span(skidding_).first(players_), kFrame);
    }

    [[nodiscard]] std::vector<render::QuadDraw> Quads() const {
        std::vector<render::QuadDraw> quads;
        particles_.AppendQuads(kCamera, quads);
        return quads;
    }

    [[nodiscard]] std::size_t QuadCount() const { return Quads().size(); }

    [[nodiscard]] RaceParticles& Particles() { return particles_; }
    [[nodiscard]] sim::Racer& Player() { return session_.Racers()[0]; }

    [[nodiscard]] float RacerX(std::size_t racer) const {
        return session_.Racers()[racer].Body().motion.position.x;
    }

private:
    sim::RaceSession Start() {
        sim::RaceSetup setup{.racers = std::vector<sim::RacerEntry>(kGridSize)};
        for (std::size_t player = 0; player < players_; ++player) {
            setup.racers[player].isPlayer = true;
        }
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            setup, {.checkpoints = &graph_, .grid = grid_, .world = &world_, .lowestTrackZ = -100});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    std::size_t players_;
    std::vector<assets::SurfaceEntry> surfaces_;
    sim::CheckpointGraph graph_ = test::Ring();
    test::PlaneWorld world_;
    std::vector<sim::RacerStart> grid_ = Grid();
    sim::RaceSession session_;
    RaceParticles particles_;
    std::vector<sim::RacerControls> controls_ = std::vector<sim::RacerControls>(kGridSize);
    std::array<std::optional<SkidWheels>, 2> skidding_{};
};

Vector3 Centre(const render::QuadDraw& quad) {
    Vector3 sum;
    for (const Vector3& corner : quad.corners) {
        sum = sum + ToGame(corner);
    }
    return sum * 0.25F;
}

std::vector<float> SideOffsets(const std::vector<render::QuadDraw>& quads) {
    std::vector<float> offsets;
    offsets.reserve(quads.size());
    for (const render::QuadDraw& quad : quads) {
        offsets.push_back(Centre(quad).y);
    }
    return offsets;
}

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

TEST_CASE("in split screen the second player's wheels kick up grass too") {
    GrassRace race("grassemt", 2);
    race.Hold(1);

    for (int tick = 0; tick < 20; ++tick) {
        race.Tick();
    }

    CHECK(race.RacerX(0) == doctest::Approx(Grid()[0].position.x).epsilon(0.01));
    CHECK(race.QuadCount() >= static_cast<std::size_t>(sim::kWheelCount));
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

TEST_CASE("a skidding player's tyres smoke from either marked wheel until the skid ends") {
    GrassRace race("grassemt");
    race.Skid(SkidWheels{false, false, true, true});

    for (int tick = 0; tick < 30; ++tick) {
        race.Tick();
    }
    const std::vector<float> sides = SideOffsets(race.Quads());
    REQUIRE_FALSE(sides.empty());
    const auto [right, left] = std::ranges::minmax(sides);
    CHECK(right < -2.0F);
    CHECK(left > 0.0F);

    race.Skid(std::nullopt);
    for (int tick = 0; tick < 50; ++tick) {
        race.Tick();
    }
    CHECK(race.QuadCount() == 0);
}

TEST_CASE("with better particles a skid draws the new smoke in place of the original's") {
    GrassRace race("grassemt");
    race.Particles().SetBetter(true);
    race.Skid(SkidWheels{false, false, true, true});

    for (int tick = 0; tick < 30; ++tick) {
        race.Tick();
    }

    const std::vector<render::QuadDraw> quads = race.Quads();
    REQUIRE(quads.size() > 10);
    CHECK(std::ranges::all_of(quads, [](const render::QuadDraw& quad) {
        return quad.uv.width == doctest::Approx(0.5F);
    }));

    race.Particles().SetBetter(false);
    const std::vector<render::QuadDraw> original = race.Quads();
    REQUIRE_FALSE(original.empty());
    CHECK(original.front().uv.width > 0.9F);
}
}
}
