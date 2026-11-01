#include "sim/track/track_timers.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};

struct Edge {
    std::int64_t timeMs = 0;
    EventEdge edge = EventEdge::Start;

    friend bool operator==(const Edge&, const Edge&) = default;
};

std::vector<Edge> Run(TrackTimers& timers, Random& random, Milliseconds duration) {
    std::vector<Edge> edges;
    TrackEventQueue queue(timers.Count());
    for (Milliseconds time = kTick; time <= duration; time += kTick) {
        queue.Clear();
        timers.Tick(kTick, random, queue);
        for (const TrackEvent& event : queue.Events()) {
            edges.push_back({.timeMs = time.count(), .edge = event.edge});
        }
    }
    return edges;
}

TEST_CASE("a delayed timer starts one on phase after its delay and then repeats") {
    const std::array timers{assets::TrackTimer{.on = {.length = Milliseconds{1666}},
                                               .off = {.length = Milliseconds{3334}},
                                               .event = 12,
                                               .startDelay = Milliseconds{3600}}};
    Random random(1);
    TrackTimers running(timers, random);

    CHECK(Run(running, random, Milliseconds{12000}) ==
          std::vector<Edge>{{.timeMs = 5268, .edge = EventEdge::Start},
                            {.timeMs = 6936, .edge = EventEdge::End},
                            {.timeMs = 10272, .edge = EventEdge::Start},
                            {.timeMs = 11940, .edge = EventEdge::End}});
}

TEST_CASE("a timer without a delay starts after its off phase") {
    const std::array timers{assets::TrackTimer{
        .on = {.length = Milliseconds{100}}, .off = {.length = Milliseconds{300}}, .event = 2}};
    Random random(1);
    TrackTimers running(timers, random);

    const std::vector<Edge> edges = Run(running, random, Milliseconds{820});

    CHECK(edges == std::vector<Edge>{{.timeMs = 300, .edge = EventEdge::Start},
                                     {.timeMs = 408, .edge = EventEdge::End},
                                     {.timeMs = 708, .edge = EventEdge::Start},
                                     {.timeMs = 816, .edge = EventEdge::End}});
    CHECK_FALSE(running.IsOn(0));
}

TEST_CASE("a random phase draws below its maximum the original's way") {
    Random random(7);
    Milliseconds longestSmall{0};
    Milliseconds longestLarge{0};
    bool areLargeSevens = true;
    for (int i = 0; i < 10000; ++i) {
        longestSmall = std::max(longestSmall, DrawPhaseLength(Milliseconds{300}, random));
        const Milliseconds large = DrawPhaseLength(Milliseconds{8000}, random);
        longestLarge = std::max(longestLarge, large);
        areLargeSevens = areLargeSevens && large.count() % 7 == 0;
    }
    CHECK(longestSmall == Milliseconds{299});
    CHECK(longestLarge == Milliseconds{7161});
    CHECK(areLargeSevens);
    CHECK(DrawPhaseLength(Milliseconds{0}, random) == Milliseconds{0});
}

TEST_CASE("random timers stay within their phases") {
    const std::array timers{
        assets::TrackTimer{.on = {.length = Milliseconds{1200}, .isRandom = true},
                           .off = {.length = Milliseconds{8000}, .isRandom = true},
                           .event = 50}};
    Random random(3);
    TrackTimers running(timers, random);

    const std::vector<Edge> edges = Run(running, random, Milliseconds{60000});

    REQUIRE(edges.size() > 4);
    std::int64_t last = 0;
    for (const Edge& edge : edges) {
        const std::int64_t gap = edge.timeMs - last;
        CHECK(gap <= (edge.edge == EventEdge::Start ? 7161 + 12 : 1199 + 12));
        last = edge.timeMs;
    }
}

TEST_CASE("a timer without an event runs but fires nothing") {
    const std::array timers{assets::TrackTimer{.on = {.length = Milliseconds{24}},
                                               .off = {.length = Milliseconds{24}}}};
    Random random(1);
    TrackTimers running(timers, random);

    CHECK(Run(running, random, Milliseconds{240}).empty());
    CHECK(running.PhaseTime(0).has_value());
}

}
}
