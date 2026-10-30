#include "sim/physics/start_boost.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <optional>

#include "plane_world.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {
namespace {

constexpr int kTickMs = 12;
constexpr RacerControls kThrottle{.isThrottleHeld = true};
constexpr RacerControls kBrake{.isBrakeHeld = true};

std::optional<TurboLevel> Hold(StartBoost& boost, const RacerControls& controls, int ticks) {
    std::optional<TurboLevel> granted;
    for (int i = 0; i < ticks; ++i) {
        if (const std::optional<TurboLevel> level = boost.Update(controls, kTickMs)) {
            granted = level;
        }
    }
    return granted;
}

std::optional<TurboLevel> PressAfterGo(int idleTicks) {
    StartBoost boost;
    boost.HoldAtStart();
    static_cast<void>(Hold(boost, {}, 10));
    REQUIRE_FALSE(boost.Go().has_value());
    static_cast<void>(Hold(boost, {}, idleTicks));
    return boost.Update(kThrottle, kTickMs);
}

TEST_CASE("throttle held for over 2 s before the start opens a 100 ms window") {
    StartBoost boost;
    boost.HoldAtStart();

    CHECK_FALSE(Hold(boost, kThrottle, 166).has_value());
    CHECK(boost.State().windowMs == 0);
    CHECK_FALSE(Hold(boost, kThrottle, 1).has_value());
    CHECK(boost.State().windowMs == 100);
    CHECK(boost.State().heldMs == 0);
}

TEST_CASE("going while the window is open gives a strong boost") {
    StartBoost boost;
    boost.HoldAtStart();
    static_cast<void>(Hold(boost, kThrottle, 170));

    CHECK(boost.Go() == TurboLevel::Strong);
    CHECK(boost.State().windowMs == 0);
}

TEST_CASE("going with throttle held and the window shut gives nothing") {
    StartBoost boost;
    boost.HoldAtStart();
    static_cast<void>(Hold(boost, kThrottle, 100));

    CHECK_FALSE(boost.Go().has_value());
    CHECK(boost.State().windowMs == 0);
}

TEST_CASE("pressing throttle in the window after the start boosts by how early it is") {
    CHECK(PressAfterGo(2) == TurboLevel::Strong);
    CHECK(PressAfterGo(4) == TurboLevel::Weak);
    CHECK_FALSE(PressAfterGo(9).has_value());
}

TEST_CASE("both pedals together neither open a window nor restart the hold") {
    StartBoost boost;
    boost.HoldAtStart();
    static_cast<void>(Hold(boost, kThrottle, 120));
    static_cast<void>(Hold(boost, {.isThrottleHeld = true, .isBrakeHeld = true}, 60));

    CHECK(boost.State().heldMs == 180 * 12);
    CHECK(boost.State().windowMs == 0);
}

TEST_CASE("braking restarts the hold") {
    StartBoost boost;
    boost.HoldAtStart();
    static_cast<void>(Hold(boost, kThrottle, 120));
    static_cast<void>(Hold(boost, kBrake, 1));

    CHECK_FALSE(Hold(boost, kThrottle, 120).has_value());
    CHECK(boost.State().windowMs == 0);
}

TEST_CASE("a racer held at the start stays put, then launches on a perfect start") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer({}, {}, {.position = {.z = 0.2F}});
    racer.HoldAtStart();
    for (int i = 0; i < 167; ++i) {
        racer.Tick(Milliseconds{kTickMs}, kThrottle, world);
    }

    CHECK(std::abs(racer.Body().motion.position.x) < 0.01F);
    racer.Go();

    const std::optional<Turbo>& active = racer.ActiveTurbo();
    REQUIRE(active.has_value());
    const Turbo& turbo = *active;
    CHECK(turbo.level == TurboLevel::Strong);
}

}
}
