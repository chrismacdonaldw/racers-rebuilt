#include "sim/physics/contact_effects.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "sim/ai/straight_route.hpp"
#include "sim/physics/plane_world.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {
namespace {

constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr Vector3 kBeside{.y = 5.0F, .z = 0.2F};
constexpr Vector3 kAhead{.x = 5.0F, .z = 0.2F};
constexpr Milliseconds kShove{750};
constexpr Milliseconds kTick{12};

Racer RacerAt(const Vector3& position) {
    return {kNeutralChassis, {}, {.position = position}};
}

Racer ShieldedAt(const Vector3& position, std::uint8_t level) {
    Racer racer = RacerAt(position);
    racer.RaiseShield(level);
    return racer;
}

Vector3 PointBetween(const Racer& first, const Racer& second) {
    return second.CentreOfMass() -
           ((second.Body().motion.position - first.Body().motion.position) * 0.5F);
}

void Touch(Racer& first, Racer& second) {
    Random random(3);
    const RacerContact contact{.first = 0, .second = 1, .point = PointBetween(first, second)};
    std::vector<Racer> racers{std::move(first), std::move(second)};
    ApplyContactEffects(racers, contact, random);
    first = std::move(racers[0]);
    second = std::move(racers[1]);
}

Racer RouteRacerAt(const Vector3& position) {
    Racer racer = RacerAt({});
    racer.FollowRoute(test::StraightRoute(position));
    return racer;
}

Racer MovingRacerAt(const Vector3& position) {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerAt(position);
    racer.Go();
    for (int ms = 0; ms < 600; ms += 12) {
        racer.Tick(kTick, {.isThrottleHeld = true}, world);
    }
    return racer;
}

bool IsShoved(const Racer& racer) {
    return racer.Body().externalForces[0].has_value();
}

bool IsLeftAlone(const Racer& racer) {
    return !IsShoved(racer) && !racer.IsSpinning();
}

int SpinMs(const Racer& racer) {
    return racer.IsSpinning() ? racer.Body().impulses.yawMs : 0;
}

TEST_CASE("a level 0 shield does nothing to the racer it touches") {
    Racer shielded = ShieldedAt({}, 0);
    Racer other = RacerAt(kBeside);

    Touch(shielded, other);

    CHECK(IsLeftAlone(other));
}

TEST_CASE("a level 1 shield shoves a racer it meets side on for 750 ms") {
    Racer shielded = ShieldedAt({}, 1);
    Racer other = RacerAt(kBeside);

    Touch(shielded, other);

    const Vector3 force = other.Body().externalForces[0].value_or(Vector3{});
    CHECK(force.x == doctest::Approx(0.0F));
    CHECK(force.y > 0.0F);
    CHECK(other.Body().steering.isSteering);
    CHECK(other.Body().steering.gain == 2.5F);
    CHECK(other.Status().shoveMs == 750);
    CHECK_FALSE(IsShoved(shielded));

    other.UpdateTimers(kShove - Milliseconds{12});
    CHECK(IsShoved(other));
    other.UpdateTimers(Milliseconds{12});
    CHECK_FALSE(IsShoved(other));
    CHECK_FALSE(other.Status().shoveForce.has_value());
}

TEST_CASE("a shove's steering holds through the drive commands until the shove ends") {
    const test::PlaneWorld world = test::FlatGround();
    Racer shielded = ShieldedAt({}, 1);
    Racer other = RacerAt(kBeside);
    Touch(shielded, other);

    other.Tick(kTick, {}, world);
    other.UpdateTimers(kTick);
    CHECK(other.Body().steering.isSteering);
    CHECK(other.Body().steering.gain == 2.5F);

    other.UpdateTimers(kShove);
    other.Tick(kTick, {}, world);
    CHECK_FALSE(other.Body().steering.isSteering);
}

TEST_CASE("a level 1 shield doesn't shove a racer it meets nose to tail") {
    Racer shielded = ShieldedAt({}, 1);
    Racer other = RacerAt(kAhead);

    Touch(shielded, other);

    CHECK_FALSE(IsShoved(other));
}

TEST_CASE("a second shove waits for the first to end") {
    Racer shielded = ShieldedAt({}, 1);
    Racer other = RacerAt(kBeside);
    Touch(shielded, other);

    Touch(shielded, other);

    CHECK(IsShoved(other));
    CHECK_FALSE(other.Body().externalForces[1].has_value());
}

TEST_CASE("a level 2 shield spins the other racer 1 turn and level 3 spins it 2") {
    Racer weaker = ShieldedAt({}, 2);
    Racer stronger = ShieldedAt({}, 3);
    Racer spunOnce = RacerAt(kBeside);
    Racer spunTwice = RacerAt(kBeside);

    Touch(weaker, spunOnce);
    Touch(stronger, spunTwice);

    CHECK(SpinMs(spunOnce) == 698);
    CHECK(SpinMs(spunTwice) == 1396);
    CHECK_FALSE(weaker.IsSpinning());
    CHECK_FALSE(stronger.IsSpinning());
}

TEST_CASE("the second racer's shield acts on the first") {
    Racer first = RacerAt({});
    Racer shielded = ShieldedAt(kBeside, 3);

    Touch(first, shielded);

    CHECK(SpinMs(first) == 1396);
    CHECK_FALSE(shielded.IsSpinning());
}

TEST_CASE("two shielded racers leave each other alone") {
    for (std::uint8_t level = 1; level <= 3; ++level) {
        CAPTURE(level);
        Racer first = ShieldedAt({}, level);
        Racer second = ShieldedAt(kBeside, level);

        Touch(first, second);

        CHECK(IsLeftAlone(first));
        CHECK(IsLeftAlone(second));
    }
}

TEST_CASE("a level 1 shield pushes a racer on a route") {
    Racer shielded = ShieldedAt({}, 1);
    Racer other = RacerAt({});
    other.FollowRoute(test::StraightRoute(kBeside));

    Touch(shielded, other);

    CHECK(other.Route().transform([](const AiRacer& route) { return route.Effects().isPushed; }) ==
          std::optional(true));
}

TEST_CASE("a curse passes to an unshielded racer with the time it has left") {
    Racer cursed = RacerAt({});
    cursed.AttachCurse(Milliseconds{10'000});
    cursed.UpdateTimers(Milliseconds{4000});
    Racer other = RacerAt(kBeside);

    Touch(other, cursed);

    CHECK_FALSE(cursed.Status().isCursed);
    CHECK(other.Status().isCursed);
    CHECK(other.Status().curseMs == 6000);
    CHECK(other.Status().curseSource.giver == std::optional<std::uint8_t>(1));
}

TEST_CASE("a shield blocks a curse passed by contact") {
    for (std::uint8_t level = 0; level <= 3; ++level) {
        CAPTURE(level);
        Racer cursed = RacerAt({});
        cursed.AttachCurse(Milliseconds{10'000});
        Racer shielded = ShieldedAt(kBeside, level);

        Touch(cursed, shielded);

        CHECK(cursed.Status().isCursed);
        CHECK_FALSE(shielded.Status().isCursed);
    }
}

TEST_CASE("a curse doesn't pass to a racer that is already cursed") {
    Racer first = RacerAt({});
    first.AttachCurse(Milliseconds{10'000});
    Racer second = RacerAt(kBeside);
    second.AttachCurse(Milliseconds{2000});

    Touch(first, second);

    CHECK(first.Status().curseMs == 10'000);
    CHECK(second.Status().curseMs == 2000);
}

TEST_CASE("two computer racers that touch shrink their boxes to nothing over 250 ms") {
    Racer first = RouteRacerAt({});
    Racer second = RouteRacerAt(kBeside);

    Touch(first, second);
    second.UpdateTimers(kTick);
    CHECK(second.Status().contact.boxMs == 738);
    CHECK(second.Status().contact.boxScale == doctest::Approx(0.952F));

    for (int tick = 0; tick < 20; ++tick) {
        Touch(first, second);
        first.UpdateTimers(kTick);
    }
    CHECK(first.Status().contact.boxScale == doctest::Approx(0.04F));
    Touch(first, second);
    first.UpdateTimers(kTick);
    CHECK(first.Status().contact.boxScale == 0.0F);
}

TEST_CASE("a box grows back to full size the tick after it stops touching") {
    Racer first = RouteRacerAt({});
    Racer second = RouteRacerAt(kBeside);
    Touch(first, second);
    first.UpdateTimers(kTick);

    first.UpdateTimers(kTick);

    CHECK(first.Status().contact.boxScale == 1.0F);
    CHECK(first.Status().contact.boxMs == 0);
    CHECK_FALSE(first.Status().contact.isTouching);
}

TEST_CASE("a player's box never shrinks") {
    Racer player = RacerAt({});
    Racer computer = RouteRacerAt(kBeside);

    Touch(computer, player);
    player.UpdateTimers(kTick);
    computer.UpdateTimers(kTick);

    CHECK(player.Status().contact.boxScale == 1.0F);
    CHECK(computer.Status().contact.boxScale == 1.0F);
}

TEST_CASE("a faster first racer scrapes, then both wait 250 ms to scrape again") {
    Racer faster = MovingRacerAt({});
    Racer slower = RacerAt(kBeside);

    Touch(faster, slower);
    REQUIRE(faster.Events().scrapeSound.has_value());
    CHECK(faster.Events().sparkPoint == PointBetween(faster, slower));
    CHECK(faster.Status().contact.scrapeCooldownMs == 250);
    CHECK(slower.Status().contact.scrapeCooldownMs == 250);
    CHECK(faster.Status().contact.isTouching);
    CHECK(slower.Status().contact.isTouching);

    slower.UpdateTimers(Milliseconds{240});
    CHECK_FALSE(slower.IsScrapeReady());
    slower.UpdateTimers(Milliseconds{10});
    CHECK(slower.IsScrapeReady());
}

TEST_CASE("a faster first racer sparks on every touch, even while its scrape waits") {
    Racer faster = MovingRacerAt({});
    Racer slower = RacerAt(kBeside);
    Touch(faster, slower);
    faster.Tick(kTick, {.isThrottleHeld = true}, test::FlatGround());
    REQUIRE_FALSE(faster.Events().sparkPoint.has_value());

    Touch(faster, slower);

    CHECK_FALSE(faster.Events().scrapeSound.has_value());
    CHECK(faster.Events().sparkPoint.has_value());
    CHECK_FALSE(slower.Events().sparkPoint.has_value());
}

TEST_CASE("a slower first racer doesn't scrape") {
    Racer slower = RacerAt({});
    Racer faster = MovingRacerAt(kBeside);

    Touch(slower, faster);

    CHECK_FALSE(slower.Events().scrapeSound.has_value());
    CHECK_FALSE(slower.Events().sparkPoint.has_value());
    CHECK(slower.IsScrapeReady());
}

}
}
