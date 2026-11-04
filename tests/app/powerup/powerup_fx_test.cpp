#include "app/powerup/powerup_fx.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "../../sim/race/checkpoint_ring.hpp"
#include "app/race_fx.hpp"
#include "app/track_space.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/powerup/green_effects.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kGridSize = 6;
constexpr Milliseconds kTick{12};
constexpr render::FloatMilliseconds kFrame{12.0F};
constexpr render::TextureId kGlow{3};
constexpr render::TextureId kStar{6};
constexpr render::FxAssets kAssets{.smoke = render::TextureId{1},
                                   .spark = render::TextureId{2},
                                   .glow = kGlow,
                                   .ring = render::TextureId{4},
                                   .bubble = render::TextureId{5},
                                   .star = kStar};

std::vector<sim::RacerStart> Grid() {
    std::vector<sim::RacerStart> grid;
    grid.reserve(kGridSize);
    for (std::size_t slot = 0; slot < kGridSize; ++slot) {
        grid.push_back(
            {.position = {.x = -20.0F, .y = static_cast<float>(slot) * -10.0F, .z = 0.2F}});
    }
    return grid;
}

class FxRace {
public:
    FxRace() : session_(Start()) {
        while (session_.Clock().Phase() != sim::RacePhase::Racing) {
            Tick();
        }
    }

    void Use(std::uint8_t level) {
        session_.GivePowerup(0, assets::BrickColour::Green, level);
        controls_[0].isUsePressed = true;
        Tick();
        controls_[0].isUsePressed = false;
    }

    void Tick() {
        session_.Tick(kTick, controls_);
        powerups_.Tick(session_, fx_, kTick);
        fx_.Update(kFrame);
    }

    void Run(int ticks) {
        for (int tick = 0; tick < ticks; ++tick) {
            Tick();
        }
    }

    [[nodiscard]] std::vector<Vector3> Drawn(render::TextureId texture) const {
        std::vector<render::QuadDraw> quads;
        fx_.AppendQuads({}, quads);
        std::vector<Vector3> centres;
        for (const render::QuadDraw& quad : quads) {
            if (quad.texture == texture) {
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
            return powerups.Slots()[slot].kind == kind;
        });
    }

    [[nodiscard]] const sim::Racer& Player() const { return session_.Racers()[0]; }

private:
    sim::RaceSession Start() {
        sim::RaceSetup setup{.racers = std::vector<sim::RacerEntry>(kGridSize)};
        setup.racers[0].isPlayer = true;
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
    render::FxSystem fx_{RaceFxEffects(), kAssets};
    PowerupFx powerups_;
    std::vector<sim::RacerControls> controls_ = std::vector<sim::RacerControls>(kGridSize);
};

bool IsAllNear(const std::vector<Vector3>& points, const Vector3& at, float distance) {
    return std::ranges::all_of(points,
                               [&](const Vector3& point) { return Length(point - at) < distance; });
}

bool IsAnyNear(const std::vector<Vector3>& points, const Vector3& at, float distance) {
    return std::ranges::any_of(points,
                               [&](const Vector3& point) { return Length(point - at) < distance; });
}

bool RunUntilLanded(FxRace& race) {
    for (int tick = 0; tick < 1000; ++tick) {
        race.Tick();
        if (race.Player().Events().hasWarpLanded) {
            return true;
        }
    }
    return false;
}

TEST_CASE("a turbo blasts flame behind the car until its boost ends") {
    FxRace race;
    race.Use(1);
    race.Run(50);

    const std::vector<Vector3> flame = race.Drawn(kGlow);
    REQUIRE(!flame.empty());
    CHECK(std::ranges::all_of(
        flame, [&](const Vector3& at) { return at.x < race.Player().Pose().position.x; }));

    while (race.IsRunning(sim::PowerupKind::Turbo)) {
        race.Tick();
    }
    race.Run(30);
    CHECK(race.Drawn(kGlow).empty());
}

TEST_CASE("a warp swirls in around the car as it starts and bursts where it lands") {
    FxRace race;
    race.Use(3);
    REQUIRE(race.IsRunning(sim::PowerupKind::Warp));
    race.Run(5);

    const Vector3 start = race.Player().Body().motion.position;
    const std::vector<Vector3> swirl = race.Drawn(kStar);
    REQUIRE(!swirl.empty());
    CHECK(IsAllNear(swirl, start, 12.0F));

    REQUIRE(RunUntilLanded(race));
    const Vector3 landing = race.Player().Body().motion.position;
    CHECK(Length(landing - start) > 30.0F);
    CHECK(IsAnyNear(race.Drawn(kStar), landing, 6.0F));
}

}
}
