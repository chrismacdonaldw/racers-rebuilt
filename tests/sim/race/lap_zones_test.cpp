#include "sim/race/lap_zones.hpp"

#include <doctest/doctest.h>

#include <array>
#include <span>

namespace rr::sim {
namespace {

using assets::LapZone;

int LapsAfter(LapZones& zones, std::span<const LapZone> path) {
    int laps = 0;
    for (const LapZone zone : path) {
        laps += zones.Enter(zone) ? 1 : 0;
    }
    return laps;
}

constexpr std::array kLap{LapZone::PostLine, LapZone::OpenTrack, LapZone::FinishLine};
constexpr std::array kWobble{LapZone::PostLine, LapZone::OpenTrack, LapZone::PostLine,
                             LapZone::OpenTrack, LapZone::FinishLine};

TEST_CASE("the first line crossing and each full lap after it count, in both versions") {
    for (const bool keepsOneZone : {false, true}) {
        LapZones zones(keepsOneZone);

        CHECK(zones.Enter(LapZone::FinishLine));
        CHECK(LapsAfter(zones, kLap) == 1);
        CHECK(LapsAfter(zones, kLap) == 1);
    }
}

TEST_CASE("touching the line again before the rest of the lap counts nothing") {
    for (const bool keepsOneZone : {false, true}) {
        LapZones zones(keepsOneZone);
        CHECK(zones.Enter(LapZone::FinishLine));

        CHECK_FALSE(zones.Enter(LapZone::FinishLine));
        CHECK(LapsAfter(zones, std::array{LapZone::PostLine, LapZone::FinishLine}) == 0);
    }
}

TEST_CASE("1999 loses a lap that wobbles between the post-line and open zones, 2001 keeps it") {
    LapZones original;
    LapZones rerelease(true);
    CHECK(original.Enter(LapZone::FinishLine));
    CHECK(rerelease.Enter(LapZone::FinishLine));

    CHECK(LapsAfter(original, kWobble) == 0);
    CHECK(LapsAfter(rerelease, kWobble) == 1);
}

TEST_CASE("1999 needs a fresh lap after backing over the start, 2001 doesn't") {
    LapZones original;
    LapZones rerelease(true);
    CHECK(LapsAfter(original,
                    std::array{LapZone::FinishLine, LapZone::PostLine, LapZone::OpenTrack}) == 1);
    CHECK(LapsAfter(rerelease,
                    std::array{LapZone::FinishLine, LapZone::PostLine, LapZone::OpenTrack}) == 1);

    original.Reverse();
    rerelease.Reverse();

    CHECK_FALSE(original.Enter(LapZone::FinishLine));
    CHECK(rerelease.Enter(LapZone::FinishLine));
}

TEST_CASE("in 2001, reaching the open zone straight from the line blocks the next count") {
    LapZones zones(true);
    CHECK(zones.Enter(LapZone::FinishLine));

    CHECK(LapsAfter(zones, std::array{LapZone::OpenTrack, LapZone::FinishLine}) == 0);
    CHECK(LapsAfter(zones, kLap) == 1);
}

}
}
