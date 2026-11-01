#include "sim/track/event_resources.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{10};
constexpr std::size_t kSpareEvents = 16;

TrackEntitySources Door() {
    return {
        .skeletons = {{.nodes = {{.name = "door"}}}},
        .animations = {{.positionKeys = {{}, {}, {.z = 10.0F}, {.z = 10.0F}, {.z = 10.0F}, {}},
                        .keyFrames = {0, 0, 4, 0, 0, 4},
                        .tracks = {{.positions = {.firstValue = 0, .firstFrame = 0, .count = 1}},
                                   {.positions = {.firstValue = 1, .firstFrame = 1, .count = 2}},
                                   {.positions = {.firstValue = 3, .firstFrame = 3, .count = 1}},
                                   {.positions = {.firstValue = 4, .firstFrame = 4, .count = 2}}},
                        .parts = {{.name = "closed",
                                   .firstTrack = 0,
                                   .frameCount = 2,
                                   .loopFrames = 3,
                                   .framesPerSecond = 100},
                                  {.name = "opening",
                                   .firstTrack = 1,
                                   .frameCount = 5,
                                   .loopFrames = 6,
                                   .framesPerSecond = 100},
                                  {.name = "open",
                                   .firstTrack = 2,
                                   .frameCount = 2,
                                   .loopFrames = 3,
                                   .framesPerSecond = 100},
                                  {.name = "closing",
                                   .firstTrack = 3,
                                   .frameCount = 5,
                                   .loopFrames = 6,
                                   .framesPerSecond = 100}}}},
        .entities = {{.name = "door", .startPart = 0}},
    };
}

assets::EventTrigger On(assets::EventId event, assets::SubEvents subEvents = {}) {
    return {.event = event, .subEvents = subEvents};
}

class Rig {
public:
    explicit Rig(assets::EventTableAsset table, TrackEntitySources sources = Door())
        : table_(std::move(table)),
          sources_(std::move(sources)),
          events_(TrackEventSources{}, 0, false, random_,
                  EventResources::MostFiredPerTick(table_) + kSpareEvents),
          entities(sources_),
          resources(table_, sources_, nullptr) {}

    void Tick(std::initializer_list<TrackEvent> fired = {}) {
        entities.Tick(kTick);
        events_.BeginTick();
        resources.BeginTick();
        for (const TrackEvent& event : fired) {
            events_.Fire(event);
        }
        resources.Dispatch(events_, entities);
        resources.Update(kTick, events_, entities);
        resources.Dispatch(events_, entities);
        history.insert(history.end(), events_.Events().begin(), events_.Events().end());
    }

    void Run(Milliseconds time) {
        for (Milliseconds ran{0}; ran < time; ran += kTick) {
            Tick();
        }
    }

    [[nodiscard]] bool Saw(assets::EventId id, EventEdge edge) const {
        return std::ranges::contains(history, TrackEvent{.id = id, .edge = edge});
    }

    [[nodiscard]] std::optional<std::size_t> Part() const { return entities.CurrentPart(0); }
    [[nodiscard]] const assets::EventTableAsset& Table() const { return table_; }

private:
    assets::EventTableAsset table_;
    TrackEntitySources sources_;
    Random random_{1};
    TrackEvents events_;

public:
    TrackEntities entities;
    EventResources resources;
    std::vector<TrackEvent> history;
};

TrackEvent Start(assets::EventId id) {
    return {.id = id, .edge = EventEdge::Start};
}
TrackEvent End(assets::EventId id) {
    return {.id = id, .edge = EventEdge::End};
}

assets::PartAnimationResource DoorAnimation() {
    return {.trigger = On(5, {.start = 50, .active = 51, .end = 52}),
            .entity = assets::EntityRef{.name = "DOOR"},
            .activePart = 2,
            .idlePart = 0,
            .startPart = 1,
            .endPart = 3,
            .isLooping = true};
}

TEST_CASE("a part animation plays its start part, then loops its active part") {
    Rig rig({.partAnimations = {DoorAnimation()}});

    rig.Tick({Start(5)});
    rig.Run(Milliseconds{100});

    CHECK(rig.Part() == 2);
    CHECK(rig.resources.PartAnimations()[0] == ResourcePhase::Active);
    CHECK(rig.history == std::vector{Start(5), Start(50), Start(51), End(50)});
    CHECK(rig.entities.NodeWorld(0, 0).position.z == doctest::Approx(10.0F));
}

TEST_CASE("an ended part animation re-enters its active phase, then plays its end part") {
    Rig rig({.partAnimations = {DoorAnimation()}});
    rig.Tick({Start(5)});
    rig.Run(Milliseconds{100});
    rig.history.clear();

    rig.Tick({End(5)});
    rig.Run(Milliseconds{100});

    CHECK(rig.Part() == 0);
    CHECK(rig.resources.PartAnimations()[0] == ResourcePhase::Idle);
    CHECK(rig.history == std::vector{End(5), Start(51), Start(52), End(51), End(52)});
    CHECK(rig.entities.NodeWorld(0, 0).position.z == doctest::Approx(0.0F));
}

TEST_CASE("a part animation without a start part plays a one-shot active part once") {
    Rig rig({.partAnimations = {{.trigger = On(6),
                                 .entity = assets::EntityRef{.name = "door"},
                                 .activePart = 1,
                                 .idlePart = 0}}});

    rig.Tick({Start(6)});
    rig.Run(Milliseconds{30});
    const std::optional<std::size_t> playing = rig.Part();
    rig.Run(Milliseconds{100});

    CHECK(playing == 1);
    CHECK(rig.Part() == 0);
    CHECK(rig.resources.PartAnimations()[0] == ResourcePhase::Idle);
}

TEST_CASE("a timer runs its active id for its duration and ignores starts and ends meanwhile") {
    Rig rig({.timers = {{.trigger = On(7, {.active = 70}), .duration = Milliseconds{40}}}});

    rig.Tick({Start(7)});
    rig.Tick({Start(7), End(7)});
    rig.Tick();
    const bool endedEarly = rig.Saw(70, EventEdge::End);
    rig.Tick();

    CHECK_FALSE(endedEarly);
    CHECK(rig.history == std::vector{Start(7), Start(70), Start(7), End(7), End(70)});
    CHECK(rig.resources.Timers()[0].phase == ResourcePhase::Ending);
}

TEST_CASE("a timer doesn't run out while its hold event runs") {
    Rig rig(
        {.timers = {
             {.trigger = On(8, {.active = 80}), .duration = Milliseconds{10}, .holdEvent = 9}}});

    rig.Tick({Start(9), Start(8)});
    rig.Run(Milliseconds{100});
    const bool ranOut = rig.Saw(80, EventEdge::End);
    rig.Tick({End(9)});

    CHECK_FALSE(ranOut);
    CHECK(rig.Saw(80, EventEdge::End));
}

TEST_CASE("a timer that starts on the end of its event starts when the event ends") {
    assets::TimerResource timer{.trigger = On(7, {.active = 70}), .duration = Milliseconds{100}};
    timer.trigger.startsOnEnd = true;
    Rig rig({.timers = {timer}});

    rig.Tick({Start(7)});
    const bool startedOnStart = rig.Saw(70, EventEdge::Start);
    rig.Tick({End(7)});

    CHECK_FALSE(startedOnStart);
    CHECK(rig.Saw(70, EventEdge::Start));
}

TEST_CASE("a timer that ends its own event stops firing once the tick's budget is spent") {
    Rig rig({.timers = {{.trigger = On(7, {.end = 7}), .duration = Milliseconds{10}}}});

    rig.Tick({Start(7)});
    rig.Tick({End(7)});

    CHECK(rig.history.size() == 2 + EventResources::MostFiredPerTick(rig.Table()));
    CHECK(rig.resources.Timers()[0].phase == ResourcePhase::Ending);
}

TEST_CASE("model distances hide their model at rest, or while active with the flag") {
    Rig rig(
        {.modelDistances = {{.trigger = On(10)}, {.trigger = On(10), .hidesWhileActive = true}}});
    const std::vector<bool> resting{rig.resources.IsModelHidden(0), rig.resources.IsModelHidden(1)};

    rig.Tick({Start(10)});

    CHECK(resting == std::vector{true, false});
    CHECK_FALSE(rig.resources.IsModelHidden(0));
    CHECK(rig.resources.IsModelHidden(1));
}

TEST_CASE("an event link ends the forced resources on their next update") {
    Rig rig({.links = {{.event = 12, .forced = 10}}, .modelDistances = {{.trigger = On(10)}}});
    rig.Tick({Start(10)});

    rig.Tick({Start(12)});

    CHECK(rig.resources.IsModelHidden(0));
    CHECK(rig.resources.ModelDistances()[0] == ResourcePhase::Idle);
}

TEST_CASE("racer-path events reach none of the position-path resources") {
    Rig rig({.modelDistances = {{.trigger = On(10)}}});

    rig.Tick({{.id = 10, .edge = EventEdge::Start, .racer = 0}});

    CHECK(rig.resources.IsModelHidden(0));
}

struct RetailRig {
    explicit RetailRig(const RaceTrack& track)
        : events(track.events, 1, false, random,
                 EventResources::MostFiredPerTick(track.events.table) + kSpareEvents),
          entities(track.entities),
          resources(track.events.table, track.entities, &track.collision) {}

    void Tick(std::initializer_list<TrackEvent> fired = {}) {
        entities.Tick(kTick);
        events.BeginTick();
        resources.BeginTick();
        for (const TrackEvent& event : fired) {
            events.Fire(event);
        }
        resources.Dispatch(events, entities);
        resources.Update(kTick, events, entities);
        resources.Dispatch(events, entities);
    }

    void Run(Milliseconds time) {
        for (Milliseconds ran{0}; ran < time; ran += kTick) {
            Tick();
        }
    }

    Random random{1};
    TrackEvents events;
    TrackEntities entities;
    EventResources resources;
};

std::optional<RaceTrack> RetailTrack(std::string_view name) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    Result<RaceTrack> track = LoadRaceTrack(*vfs, name);
    REQUIRE(track.has_value());
    return std::move(*track);
}

std::size_t WorldNamed(const TrackCollision& collision, std::string_view name) {
    const auto world = std::ranges::find(collision.worlds, name, &NamedWorld::name);
    REQUIRE(world != collision.worlds.end());
    return static_cast<std::size_t>(world - collision.worlds.begin());
}

TEST_CASE("the RACEC3R0 timer raises the door for 3 seconds, then lowers it") {
    const std::optional<RaceTrack> track = RetailTrack("RACEC3R0");
    if (!track) {
        return;
    }
    RetailRig rig(*track);
    const std::size_t door = rig.entities.Find("door").value_or(0);

    rig.Tick({Start(1)});
    rig.Run(Milliseconds{1500});
    const std::optional<std::size_t> raised = rig.entities.CurrentPart(door);
    rig.Run(Milliseconds{3000});

    CHECK(raised == 2);
    CHECK(rig.entities.CurrentPart(door) == 0);
    CHECK(rig.resources.Timers()[0].phase == ResourcePhase::Ending);
}

TEST_CASE("the RACEC0R2 shortcut volume follows its sliding door while event 2 runs") {
    const std::optional<RaceTrack> track = RetailTrack("RACEC0R2");
    if (!track) {
        return;
    }
    RetailRig rig(*track);
    const std::size_t volume = WorldNamed(track->collision, "shtctcol");
    const Vector3 rest =
        track->collision.worlds[volume].world.WorldPlacement().value_or(Placement{}).position;

    rig.Tick({Start(2), Start(1)});
    const std::optional<Placement> started = rig.resources.MovedWorlds()[volume];
    rig.Run(Milliseconds{1500});
    const std::optional<Placement> slid = rig.resources.MovedWorlds()[volume];

    REQUIRE(started.has_value());
    REQUIRE(slid.has_value());
    CHECK(Length(started.value_or(Placement{}).position - rest) < 0.01F);
    CHECK(Length(slid.value_or(Placement{}).position - rest) > 1.0F);
}

}
}
