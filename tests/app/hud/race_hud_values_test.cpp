#include "app/hud/race_hud_values.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>

#include "../../sim/time_trial/ghost_run_rig.hpp"
#include "ring_race.hpp"

namespace rr::app {
namespace {

using test::RingRace;

std::optional<HudBanner> BannerText(const HudValues& values) {
    return values.banner.transform([](const BannerView& banner) { return banner.text; });
}

float BannerScale(const HudValues& values) {
    return values.banner.transform([](const BannerView& banner) { return banner.scale; })
        .value_or(0.0F);
}

TEST_CASE("the countdown shows 3, 2 and 1, each shrinking from 1.8 over its second") {
    RingRace race(1, 3);
    race.clock.Advance(Milliseconds{2001}, false);

    race.clock.Advance(Milliseconds{500}, false);
    const HudValues three = ReadHudValues(race.clock, race.progress, 0);
    race.clock.Advance(Milliseconds{750}, false);
    const HudValues two = ReadHudValues(race.clock, race.progress, 0);
    race.clock.Advance(Milliseconds{1000}, false);
    const HudValues one = ReadHudValues(race.clock, race.progress, 0);

    CHECK(BannerText(three) == HudBanner::Three);
    CHECK(BannerScale(three) == doctest::Approx(0.9F));
    CHECK(BannerText(two) == HudBanner::Two);
    CHECK(BannerScale(two) == doctest::Approx(1.35F));
    CHECK(BannerText(one) == HudBanner::One);
    CHECK(BannerScale(one) == doctest::Approx(1.35F));
}

TEST_CASE("go holds for a second, shrinks over the next, then clears") {
    RingRace race(1, 3);
    race.StartRacing();
    const HudValues held = ReadHudValues(race.clock, race.progress, 0);

    race.clock.Advance(Milliseconds{1500}, false);
    const HudValues shrinking = ReadHudValues(race.clock, race.progress, 0);
    race.clock.Advance(Milliseconds{500}, false);

    CHECK(BannerText(held) == HudBanner::Go);
    CHECK(BannerScale(held) == doctest::Approx(1.8F));
    CHECK(BannerScale(shrinking) == doctest::Approx(0.9F));
    CHECK_FALSE(ReadHudValues(race.clock, race.progress, 0).banner.has_value());
}

TEST_CASE("before the race the HUD reads lap 1 at no time and no best lap") {
    const RingRace race(2, 3);

    const HudValues values = ReadHudValues(race.clock, race.progress, 1);

    CHECK(values.lap == 1);
    CHECK(values.lapTime == Milliseconds{0});
    CHECK_FALSE(values.bestLap.has_value());
    CHECK_FALSE(values.banner.has_value());
}

TEST_CASE("for 3 s after a lap, the last lap's time flashes in 250 ms beats") {
    RingRace race(1, 3);
    race.StartRacing();
    race.Step(0, -15.0F, -5.0F);
    race.DriveLap(0);
    race.DriveLap(0);
    const HudValues shown = ReadHudValues(race.clock, race.progress, 0);

    race.Wait(0, Milliseconds{300});
    const HudValues hidden = ReadHudValues(race.clock, race.progress, 0);
    race.Wait(0, Milliseconds{2300});
    const HudValues current = ReadHudValues(race.clock, race.progress, 0);

    CHECK(shown.lap == 2);
    CHECK(shown.lapTime == Milliseconds{600});
    CHECK_FALSE(hidden.lapTime.has_value());
    CHECK(current.lapTime == Milliseconds{3100});
}

TEST_CASE("the best lap is the fastest completed lap") {
    RingRace race(1, 3);
    race.StartRacing();
    race.DriveLap(0);
    race.Wait(0, Milliseconds{1000});
    race.DriveLap(0);
    race.DriveLap(0);

    const HudValues values = ReadHudValues(race.clock, race.progress, 0);

    CHECK(values.lap == 3);
    CHECK(values.bestLap == Milliseconds{500});
}

TEST_CASE("the place follows the standings, then the finishing place") {
    RingRace race(2, 1);
    race.StartRacing();
    race.Step(1, -5.0F, 5.0F);
    const std::array<Vector3, 2> positions{Vector3{.x = -8.0F}, Vector3{.x = 5.0F}};
    race.progress.Rank(race.graph, positions);
    const int behind = ReadHudValues(race.clock, race.progress, 0).place;

    race.DriveLap(0);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{12});

    CHECK(behind == 2);
    CHECK(ReadHudValues(race.clock, race.progress, 0).place == 1);
}

TEST_CASE("finishing shows the banner until the results replace it") {
    RingRace race(1, 1);
    race.StartRacing();
    race.DriveLap(0);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{12});
    race.clock.Advance(Milliseconds{12}, true);
    REQUIRE(race.clock.Phase() == sim::RacePhase::Finished);
    const HudValues finished = ReadHudValues(race.clock, race.progress, 0);

    race.clock.Advance(Milliseconds{300}, false);
    const HudValues flashing = ReadHudValues(race.clock, race.progress, 0);
    race.clock.Advance(kResultsDelay, false);

    CHECK(BannerText(finished) == HudBanner::Finish);
    CHECK(finished.lap == 1);
    CHECK(finished.lapTime == Milliseconds{500});
    CHECK_FALSE(flashing.lapTime.has_value());
    CHECK_FALSE(ReadHudValues(race.clock, race.progress, 0).banner.has_value());
    CHECK(ShowsResults(race.clock));
}

TEST_CASE("the gap to the leader shows for 2 s after a lap") {
    RingRace race(2, 3);
    race.StartRacing();
    race.DriveLap(1);
    race.DriveLap(1);
    const std::array<Vector3, 2> positions{Vector3{.x = -5.0F}, Vector3{.x = 5.0F}};
    race.progress.Rank(race.graph, positions);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{1000});
    race.DriveLap(0);
    const HudValues shown = ReadHudValues(race.clock, race.progress, 0);

    race.Wait(0, Milliseconds{1500});

    CHECK(shown.lapGap == Milliseconds{1000});
    CHECK_FALSE(ReadHudValues(race.clock, race.progress, 0).lapGap.has_value());
}

TEST_CASE("a time trial shows its total and the time lost to the record ghost") {
    RingRace race(1, 3);
    race.StartRacing();
    const sim::TimeTrialGhosts ghosts(test::StraightGhost(4, test::Laps(400, 400, 400)));
    race.DriveLap(0);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{100});

    const HudValues values = ReadHudValues(race.clock, race.progress, 0, &ghosts);

    CHECK(values.total == Milliseconds{1100});
    CHECK(values.lapGap == Milliseconds{100});
    CHECK_FALSE(ReadHudValues(race.clock, race.progress, 0).total.has_value());
}

}
}
