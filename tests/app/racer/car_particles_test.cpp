#include "app/racer/car_particles.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr sim::ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr Milliseconds kTick{12};
constexpr render::FloatMilliseconds kFrame{16.0F};
constexpr Transform kCamera{.position = {.x = -20.0F, .y = 5.0F}};

assets::EmitterAsset Emitter(std::string name, std::optional<Milliseconds> duration) {
    return {.name = std::move(name),
            .emitRate = 50.0F,
            .points = {{}},
            .particleLife = Milliseconds{10'000},
            .duration = duration,
            .radius = 1000.0F,
            .priority = 10};
}

render::ParticleBankSetup Emitters() {
    render::ParticleBankSetup setup{
        .emitters = {Emitter("dust", std::nullopt), Emitter("carsmke", std::nullopt),
                     Emitter("tiresmk", std::nullopt), Emitter("carland", Milliseconds{100}),
                     Emitter("carsprk", Milliseconds{100})},
        .slotCount = 10};
    setup.emitterLooks.assign(setup.emitters.size(), render::ParticleLook{});
    return setup;
}

Vector3 Centre(const render::QuadDraw& quad) {
    Vector3 sum;
    for (const Vector3& corner : quad.corners) {
        sum = sum + ToGame(corner);
    }
    return sum * 0.25F;
}

class Garage {
public:
    explicit Garage(float height = 0.2F) : racers_{Racer(height)} {}

    void Settle(int ticks) {
        for (int i = 0; i < ticks; ++i) {
            racers_[0].Tick(kTick, {}, ground_);
            particles_.Tick(racers_, bank_);
            bank_.Update(render::FloatMilliseconds{kTick});
        }
    }

    void Frames(int count, bool isRearKicking = false) {
        for (int i = 0; i < count; ++i) {
            particles_.Update(racers_[0], puffs_, std::nullopt, isRearKicking, {}, bank_);
            bank_.Update(kFrame);
        }
    }

    [[nodiscard]] std::vector<Vector3> Puffs() const {
        std::vector<render::QuadDraw> quads;
        bank_.AppendQuads(kCamera, quads);
        std::vector<Vector3> centres;
        centres.reserve(quads.size());
        for (const render::QuadDraw& quad : quads) {
            centres.push_back(Centre(quad));
        }
        return centres;
    }

    [[nodiscard]] float WheelTop() const {
        const auto& wheels = racers_[0].Body().motion.wheels;
        return std::ranges::max(wheels, {},
                                [](const sim::WheelProbe& wheel) { return wheel.position.z; })
            .position.z;
    }

    [[nodiscard]] sim::Racer& Car() { return racers_[0]; }
    [[nodiscard]] bool IsGrounded() const { return racers_[0].Body().motion.contactCount > 0; }
    void Tick() { particles_.Tick(racers_, bank_); }
    void Update() { bank_.Update(kFrame); }

private:
    static sim::Racer Racer(float height) { return {kChassis, {}, {.position = {.z = height}}}; }

    test::PlaneWorld ground_ = test::FlatGround();
    std::vector<sim::Racer> racers_;
    render::ParticleBank bank_{Emitters()};
    CarParticles particles_;
    CarParticles::Puffs puffs_;
};

std::size_t CountAbove(const std::vector<Vector3>& puffs, float z) {
    return static_cast<std::size_t>(
        std::ranges::count_if(puffs, [&](const Vector3& puff) { return puff.z > z; }));
}

TEST_CASE("a turbo start puffs ten dust clouds at the rear wheels and four exhaust clouds") {
    Garage garage;
    garage.Settle(5);
    REQUIRE(garage.Puffs().empty());

    garage.Car().StartTurbo(sim::TurboLevel::Strong);
    garage.Frames(100);

    const std::vector<Vector3> puffs = garage.Puffs();
    CHECK(puffs.size() == 14);
    CHECK(CountAbove(puffs, garage.WheelTop() + 1.0F) == 4);
}

TEST_CASE("a turbo start raises no dust while a rear wheel kicks up its surface") {
    Garage garage;
    garage.Settle(5);

    garage.Car().StartTurbo(sim::TurboLevel::Weak);
    garage.Frames(100, true);

    CHECK(garage.Puffs().size() == 4);
}

TEST_CASE("a car that falls for over 400 ms raises dust where it lands") {
    Garage settled;
    settled.Settle(20);
    CHECK(settled.Puffs().empty());

    Garage dropped(100.0F);
    dropped.Settle(1);
    for (int tick = 0; tick < 1000 && !dropped.IsGrounded(); ++tick) {
        dropped.Settle(1);
    }
    REQUIRE(dropped.IsGrounded());
    CHECK_FALSE(dropped.Puffs().empty());
}

TEST_CASE("a car that sparks shows them at the contact point") {
    Garage garage;
    garage.Settle(5);

    garage.Car().Spark({.x = 5.0F, .z = 1.0F});
    garage.Tick();
    garage.Update();

    const std::vector<Vector3> puffs = garage.Puffs();
    REQUIRE_FALSE(puffs.empty());
    CHECK(puffs[0].x == doctest::Approx(5.0F));
    CHECK(puffs[0].z == doctest::Approx(1.0F));
}

}
}
