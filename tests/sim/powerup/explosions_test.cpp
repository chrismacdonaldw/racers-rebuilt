#include "sim/powerup/explosions.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

#include "core/bounding_sphere.hpp"
#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using test::kChassis;
using test::kTick;

class Blasts {
public:
    explicit Blasts(const std::vector<Vector3>& positions) {
        for (const Vector3& position : positions) {
            racers_.emplace_back(kChassis, CarBuild{}, RacerStart{.position = position});
            reaches_.push_back({.centre = position, .radius = 3.0F});
        }
    }

    void Tick(int ticks = 1) {
        for (int tick = 0; tick < ticks; ++tick) {
            explosions_.Tick(kTick, racers_, reaches_);
            for (Racer& racer : racers_) {
                racer.UpdateTimers(kTick);
            }
        }
    }

    Explosions& Pool() { return explosions_; }
    Racer& Car(std::size_t racer) { return racers_[racer]; }

private:
    std::vector<Racer> racers_;
    std::vector<BoundingSphere> reaches_;
    Explosions explosions_;
};

TEST_CASE("an explosion grows to its blast radius over the 1,000 ms flash") {
    Blasts blasts({});
    blasts.Pool().Spawn({}, false, std::nullopt);
    const Explosion& explosion = blasts.Pool().All()[0];

    blasts.Tick();
    CHECK(blasts.Pool().BlastRadius(explosion) ==
          doctest::Approx(0.1F + (4.9F * 0.0737F)).epsilon(0.01));
    blasts.Tick(82);
    CHECK(explosion.phase == ExplosionPhase::Exploding);
    CHECK(blasts.Pool().BlastRadius(explosion) == doctest::Approx(5.0F).epsilon(0.01));
    blasts.Tick();
    CHECK(explosion.phase == ExplosionPhase::Idle);
}

TEST_CASE("a scar fades for 5,000 ms after the flash") {
    Blasts blasts({});
    blasts.Pool().Spawn({}, true, std::nullopt);
    const Explosion& explosion = blasts.Pool().All()[0];

    blasts.Tick(84);
    CHECK(explosion.phase == ExplosionPhase::ScarFading);
    blasts.Tick(415);
    CHECK(explosion.phase == ExplosionPhase::ScarFading);
    blasts.Tick();
    CHECK(explosion.phase == ExplosionPhase::Idle);
}

TEST_CASE("the blast launches unshielded racers it reaches, but not its owner") {
    Blasts blasts({{}, {.x = 2.0F}, {.y = 2.0F}, {.x = 50.0F}});
    blasts.Car(2).RaiseShield(0);
    blasts.Pool().Spawn({}, false, 0);

    blasts.Tick();

    const float lift = 200.0F / kChassis.baseMass;
    CHECK(blasts.Car(0).Body().motion.velocity.z == doctest::Approx(0.0F));
    CHECK(blasts.Car(1).Body().motion.velocity.z == doctest::Approx(lift));
    CHECK(blasts.Car(2).Body().motion.velocity.z == doctest::Approx(0.0F));
    CHECK(blasts.Car(3).Body().motion.velocity.z == doctest::Approx(0.0F));
}

TEST_CASE("the blast fades over the second half of the flash") {
    Blasts blasts({{.x = 3.0F}});
    blasts.Pool().Spawn({}, false, std::nullopt);
    blasts.Tick(61);

    blasts.Tick();

    const float remaining = 1000.0F - (62.0F * 12.0F);
    const float lift = 200.0F * (2.0F * remaining / 1000.0F) / kChassis.baseMass;
    CHECK(blasts.Car(0).Body().motion.velocity.z == doctest::Approx(lift));
}

TEST_CASE("a seventh explosion takes back the one nearest its end") {
    Blasts blasts({});
    for (std::size_t i = 0; i < kExplosionCount; ++i) {
        blasts.Pool().Spawn({.x = static_cast<float>(i)}, false, std::nullopt);
        blasts.Tick();
    }

    blasts.Pool().Spawn({.x = 100.0F}, false, std::nullopt);

    CHECK(blasts.Pool().All()[0].position.x == 100.0F);
    CHECK(blasts.Pool().All()[0].remaining == Milliseconds{1000});
}

TEST_CASE("an explosion outside the world box doesn't spawn") {
    Blasts blasts({});
    blasts.Pool().Spawn({.z = -2049.0F}, false, std::nullopt);

    CHECK(blasts.Pool().All()[0].phase == ExplosionPhase::Idle);
}

}
}
