#include "app/powerup/powerup_particles.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "../../sim/race/checkpoint_ring.hpp"
#include "app/track_space.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/yellow_effects.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kGridSize = 6;
constexpr Milliseconds kTick{12};
constexpr render::FloatMilliseconds kFrame{12.0F};
constexpr std::array<std::string_view, 5> kEmitters{"cannsmk", "trbsmke", "lghthit", "oilbub",
                                                    "dynsprk"};
constexpr Vector3 kVictimStart{.x = 0.0F, .z = 0.2F};

render::TextureId TextureOf(std::string_view emitter) {
    return render::TextureId{static_cast<std::uint32_t>(
        100 + (std::ranges::find(kEmitters, emitter) - kEmitters.begin()))};
}

render::ParticleBankSetup Emitters() {
    render::ParticleBankSetup setup{.slotCount = kEmitters.size()};
    for (const std::string_view name : kEmitters) {
        setup.emitters.push_back({.name = std::string(name),
                                  .emitRate = 50.0F,
                                  .points = {{}},
                                  .particleLife = Milliseconds{500},
                                  .radius = 100'000.0F,
                                  .priority = 10});
        setup.emitterLooks.emplace_back(render::ParticleLook{.texture = TextureOf(name)});
    }
    return setup;
}

std::vector<sim::RacerStart> Grid() {
    std::vector<sim::RacerStart> grid{{.position = {.x = -20.0F, .z = 0.2F}},
                                      {.position = kVictimStart}};
    for (std::size_t slot = grid.size(); slot < kGridSize; ++slot) {
        grid.push_back(
            {.position = {.x = -20.0F, .y = static_cast<float>(slot) * -10.0F, .z = 0.2F}});
    }
    return grid;
}

class PowerupRace {
public:
    PowerupRace() : session_(Start()), bank_(Emitters()) {
        while (session_.Clock().Phase() != sim::RacePhase::Racing) {
            Tick();
        }
    }

    void Use(assets::BrickColour colour, std::uint8_t level) {
        session_.GivePowerup(0, colour, level);
        controls_[0].isUsePressed = true;
        Tick();
        controls_[0].isUsePressed = false;
    }

    void Tick() {
        session_.Tick(kTick, controls_);
        particles_.Tick(session_, bank_, kTick);
        bank_.Update(kFrame);
    }

    void Run(int ticks) {
        for (int tick = 0; tick < ticks; ++tick) {
            Tick();
        }
    }

    [[nodiscard]] std::vector<Vector3> Particles(std::string_view emitter) const {
        std::vector<render::QuadDraw> quads;
        bank_.AppendQuads({}, quads);
        std::vector<Vector3> centres;
        for (const render::QuadDraw& quad : quads) {
            if (quad.texture == TextureOf(emitter)) {
                centres.push_back(
                    ToGame((quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) *
                           0.25F));
            }
        }
        return centres;
    }

    [[nodiscard]] bool IsRunning(sim::PowerupKind kind) const {
        const sim::PowerupSystem& powerups = session_.Powerups();
        return std::ranges::any_of(powerups.Active(), [&](std::uint16_t slot) {
            return powerups.Slots()[slot].kind == kind && powerups.Slots()[slot].use.racer == 0;
        });
    }

    [[nodiscard]] Transform Player() const { return session_.Racers()[0].Pose(); }

private:
    sim::RaceSession Start() {
        sim::RaceSetup setup{.racers = std::vector<sim::RacerEntry>(kGridSize)};
        setup.racers[0].isPlayer = true;
        sim::AddRedEffects(setup.effects);
        sim::AddYellowEffects(setup.effects);
        sim::AddGreenEffects(setup.effects);
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            setup, {.checkpoints = &graph_, .grid = grid_, .world = &world_, .lowestTrackZ = -100});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    sim::CheckpointGraph graph_ = test::Ring();
    test::PlaneWorld world_{{test::TestPlane{}}};
    std::vector<sim::RacerStart> grid_ = Grid();
    sim::RaceSession session_;
    render::ParticleBank bank_;
    PowerupParticles particles_;
    std::vector<sim::RacerControls> controls_ = std::vector<sim::RacerControls>(kGridSize);
};

TEST_CASE("a fired cannonball puffs three times above its owner") {
    PowerupRace race;
    race.Use(assets::BrickColour::Red, 0);
    REQUIRE(race.IsRunning(sim::PowerupKind::Cannonball));
    const float carZ = race.Player().position.z;

    std::size_t most = 0;
    for (int tick = 0; tick < 40; ++tick) {
        race.Tick();
        const std::vector<Vector3> puffs = race.Particles("cannsmk");
        most = std::max(most, puffs.size());
        if (!puffs.empty()) {
            CHECK(puffs.front().z > carZ + 3.0F);
        }
    }

    CHECK(most == 3);
}

TEST_CASE("a level 2 turbo smokes behind the car until it ends, and weaker ones never do") {
    PowerupRace weak;
    weak.Use(assets::BrickColour::Green, 1);
    weak.Run(100);
    CHECK(weak.Particles("trbsmke").empty());

    PowerupRace race;
    race.Use(assets::BrickColour::Green, 2);
    race.Run(60);
    const std::vector<Vector3> smoke = race.Particles("trbsmke");
    REQUIRE(!smoke.empty());
    CHECK(smoke.back().x < race.Player().position.x);

    while (race.IsRunning(sim::PowerupKind::Turbo)) {
        race.Tick();
    }
    CHECK(race.Particles("trbsmke").empty());
}

TEST_CASE("an armed oil slick bubbles around itself") {
    PowerupRace race;
    race.Use(assets::BrickColour::Yellow, 0);
    const Vector3 dropped = race.Player().position;

    race.Run(100);

    const std::vector<Vector3> bubbles = race.Particles("oilbub");
    REQUIRE(!bubbles.empty());
    CHECK(std::abs(bubbles.front().y - dropped.y) <= 6.0F);
}

TEST_CASE("thrown dynamite sparks from its fuse") {
    PowerupRace race;
    race.Use(assets::BrickColour::Yellow, 1);

    race.Run(10);

    CHECK(!race.Particles("dynsprk").empty());
}

TEST_CASE("lightning sparks where it strikes a racer") {
    PowerupRace race;
    race.Use(assets::BrickColour::Red, 2);

    bool hasSparked = false;
    for (int tick = 0; tick < 150 && !hasSparked; ++tick) {
        race.Tick();
        const std::vector<Vector3> sparks = race.Particles("lghthit");
        hasSparked = !sparks.empty();
        if (hasSparked) {
            CHECK(sparks.front().x > race.Player().position.x + 5.0F);
        }
    }

    CHECK(hasSparked);
}

}
}
