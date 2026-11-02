#include "app/cutscene/cutscene_timeline.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "assets/archive_survey.hpp"
#include "core/result.hpp"

namespace rr::app {
namespace {

using assets::CutsceneSpan;

constexpr CutsceneEventId Transform(std::size_t index) {
    return {.kind = CutsceneEventKind::Transform, .index = index};
}

constexpr CutsceneEventChange Begins(CutsceneEventId event) {
    return {.event = event, .begins = true};
}

constexpr CutsceneEventChange Ends(CutsceneEventId event) {
    return {.event = event, .begins = false};
}

assets::CutsceneTransform TransformEvent(std::string name, CutsceneSpan span) {
    return {.name = std::move(name), .span = span};
}

assets::CutsceneCamera CameraEvent(const std::string& camera, CutsceneSpan span) {
    return {.name = camera, .span = span, .camera = camera};
}

std::vector<CutsceneEventChange> Ticks(CutsceneTimeline& timeline, std::size_t count) {
    std::vector<CutsceneEventChange> changes;
    for (std::size_t tick = 0; tick < count; ++tick) {
        const std::span<const CutsceneEventChange> step = timeline.Advance(kSimTick);
        changes.insert(changes.end(), step.begin(), step.end());
    }
    return changes;
}

TEST_CASE("events begin and end on the frame clock, a begin first on a tie") {
    const assets::CutsceneFrame frame{
        .length = 100,
        .cameras = {CameraEvent("cam", {.start = 0, .duration = 50})},
        .transforms = {TransformEvent("a", {.start = 0, .duration = 10}),
                       TransformEvent("b", {.start = 10, .duration = 0})}};
    CutsceneTimeline timeline(frame);

    CHECK(Ticks(timeline, 1) == std::vector{Begins({.kind = CutsceneEventKind::Camera, .index = 0}),
                                            Begins(Transform(0))});
    CHECK(Ticks(timeline, 26).empty());
    CHECK(Ticks(timeline, 1) ==
          std::vector{Begins(Transform(1)), Ends(Transform(0)), Ends(Transform(1))});
    CHECK(timeline.IsActive({.kind = CutsceneEventKind::Camera, .index = 0}));
    CHECK_FALSE(timeline.IsActive(Transform(1)));
}

TEST_CASE("a frame stops at its length, leaving its events as they are") {
    const assets::CutsceneFrame frame{
        .length = 10, .transforms = {TransformEvent("a", {.start = 0, .duration = 20})}};
    CutsceneTimeline timeline(frame);

    static_cast<void>(Ticks(timeline, 27));
    CHECK_FALSE(timeline.IsComplete());
    static_cast<void>(Ticks(timeline, 1));
    CHECK(timeline.IsComplete());
    CHECK(timeline.IsActive(Transform(0)));
    CHECK(Ticks(timeline, 5).empty());
}

TEST_CASE("a looping frame ends its events and starts them again") {
    const assets::CutsceneFrame frame{
        .length = 10, .transforms = {TransformEvent("a", {.start = 0, .duration = 20})}};
    CutsceneTimeline timeline(frame, true);

    static_cast<void>(Ticks(timeline, 27));
    CHECK(Ticks(timeline, 1) == std::vector{Ends(Transform(0)), Begins(Transform(0))});
    CHECK_FALSE(timeline.IsComplete());
}

TEST_CASE("the view draws through the oldest camera and screens fade over the newest") {
    const assets::CutsceneFrame frame{
        .length = 100,
        .cameras = {CameraEvent("first", {.start = 0, .duration = 30}),
                    CameraEvent("second", {.start = 10, .duration = 30})}};
    CutsceneTimeline timeline(frame);
    CHECK_FALSE(timeline.DrawingCamera());

    static_cast<void>(Ticks(timeline, 42));
    CHECK(timeline.DrawingCamera() == 0);
    CHECK(timeline.NewestCamera() == 1);
    static_cast<void>(Ticks(timeline, 42));
    CHECK(timeline.DrawingCamera() == 1);
    CHECK(timeline.NewestCamera() == 1);
}

TEST_CASE("a blinking light switches when its time runs out, timed at 30 FPS frames") {
    const assets::CutsceneFrame frame{
        .length = 100,
        .ambientLights = {{.name = "amb",
                           .span = {.start = 0, .duration = 100},
                           .blink = assets::LightBlink{.framesOn = 2, .framesOff = 4}}}};
    CutsceneTimeline timeline(frame);

    static_cast<void>(Ticks(timeline, 5));
    CHECK(timeline.AmbientLight() == 0);
    static_cast<void>(Ticks(timeline, 1));
    CHECK_FALSE(timeline.AmbientLight());
    static_cast<void>(Ticks(timeline, 11));
    CHECK_FALSE(timeline.AmbientLight());
    static_cast<void>(Ticks(timeline, 1));
    CHECK(timeline.AmbientLight() == 0);
}

void CheckPlaysOnce(const assets::CutsceneFrame& frame) {
    CAPTURE(frame.name);
    CutsceneTimeline timeline(frame);
    std::vector<CutsceneEventChange> changes;
    const auto lengthMs = static_cast<std::size_t>(frame.length) * 1000 /
                          static_cast<std::size_t>(frame.framesPerSecond);
    for (std::size_t tick = 0; tick <= (lengthMs / kSimTick.count()) + 1; ++tick) {
        const std::span<const CutsceneEventChange> step = timeline.Advance(kSimTick);
        changes.insert(changes.end(), step.begin(), step.end());
    }
    CHECK(timeline.IsComplete());
    for (std::size_t first = 0; first < changes.size(); ++first) {
        const auto later = std::span(changes).subspan(first + 1);
        CHECK(std::ranges::count(later, changes[first]) == 0);
    }
}

TEST_CASE("every retail frame plays to its end, beginning and ending each event once at most") {
    const std::optional<assets::JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    for (const assets::IndexEntry& entry : test::FilesWithExtension(*archive, "CDB")) {
        CAPTURE(entry.Name());
        const Result<assets::CutsceneTimelineAsset> timeline =
            test::ParseEntry(*archive, entry, assets::ParseCutsceneTimeline);
        REQUIRE(timeline.has_value());
        for (const assets::CutsceneFrame& frame : timeline->frames) {
            CheckPlaysOnce(frame);
        }
    }
}

TEST_CASE("an eighth directional light drops the oldest") {
    assets::CutsceneFrame frame{.length = 100};
    for (int light = 0; light < 8; ++light) {
        frame.directionalLights.push_back({.span = {.start = 0, .duration = 10}});
    }
    CutsceneTimeline timeline(frame);

    static_cast<void>(Ticks(timeline, 1));

    CHECK(std::vector(timeline.DirectionalLights().begin(), timeline.DirectionalLights().end()) ==
          std::vector<std::size_t>{1, 2, 3, 4, 5, 6, 7});
}

}
}
