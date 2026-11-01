#include "sim/track/track_events.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "assets/archive_survey.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr float kStepLength = 2.0F;
constexpr float kRacerReach = 6.0F;

struct Fired {
    std::vector<assets::EventId> racerStarts;
    std::vector<assets::EventId> racerEnds;
    std::vector<assets::EventId> environmentStarts;
};

void Record(const TrackEvents& events, Fired& fired) {
    for (const TrackEvent& event : events.Events()) {
        if (event.racer) {
            (event.edge == EventEdge::Start ? fired.racerStarts : fired.racerEnds)
                .push_back(event.id);
        } else if (event.edge == EventEdge::Start && event.position) {
            fired.environmentStarts.push_back(event.id);
        }
    }
}

Fired DriveRoute(const RaceTrack& track, const assets::RaceRoute& route) {
    Random random(1);
    TrackEvents events(track.events, 1, false, random);
    Fired fired;
    Vector3 position = route.startPosition;
    for (const assets::RoutePoint& point : route.points) {
        const int steps =
            std::max(1, static_cast<int>(std::ceil(Length(point.offset) / kStepLength)));
        for (int step = 1; step <= steps; ++step) {
            const Vector3 at =
                position + (point.offset * (static_cast<float>(step) / static_cast<float>(steps)));
            const std::array racers{
                TriggerRacer{.reach = {.centre = at, .radius = kRacerReach}, .isPlayer = true}};
            events.BeginTick();
            events.Tick(kTick, racers, random);
            Record(events, fired);
        }
        position += point.offset;
    }
    return fired;
}

const assets::RaceRoute& FirstRoute(const RaceTrack& track) {
    const auto routes = std::ranges::find_if(
        track.routes, [](const std::vector<assets::RaceRoute>& slot) { return !slot.empty(); });
    REQUIRE(routes != track.routes.end());
    return routes->front();
}

TEST_CASE("driving a lap of RACEC0R0 fires its racer and environment spheres in order") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<RaceTrack> track = LoadRaceTrack(*vfs, "RACEC0R0");
    REQUIRE(track.has_value());

    const Fired fired = DriveRoute(*track, FirstRoute(*track));

    const std::vector<assets::EventId> lap{1033, 102, 53, 50, 100};
    const std::vector<assets::EventId> ambience{6, 13, 2, 11, 7, 9, 5, 14, 17, 18, 4, 3};
    CHECK(std::ranges::starts_with(fired.racerStarts, lap));
    CHECK(std::ranges::starts_with(fired.environmentStarts, ambience));
    CHECK(fired.racerEnds.size() + 1 == fired.racerStarts.size());
}

}
}
