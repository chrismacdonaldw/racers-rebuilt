#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/cutscene_timeline.hpp"

namespace rr::assets {
namespace {

struct TimelineCounts {
    std::string_view path;
    std::int32_t length = 0;
    std::size_t cameras = 0;
    std::size_t models = 0;
    std::size_t transforms = 0;
    std::size_t ambientLights = 0;
    std::size_t directionalLights = 0;

    friend bool operator==(const TimelineCounts&, const TimelineCounts&) = default;
};

constexpr std::array kTimelines{
    TimelineCounts{.path = "LEGAL/SPLASH1",
                   .length = 150,
                   .cameras = 0,
                   .models = 0,
                   .transforms = 3,
                   .ambientLights = 0,
                   .directionalLights = 0},
    TimelineCounts{.path = "CIRCUIT1/CIR1-01",
                   .length = 490,
                   .cameras = 1,
                   .models = 10,
                   .transforms = 13,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "CIRCUIT2/TEST",
                   .length = 480,
                   .cameras = 2,
                   .models = 15,
                   .transforms = 10,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "CIRCUIT3/CIR3-01",
                   .length = 410,
                   .cameras = 2,
                   .models = 44,
                   .transforms = 15,
                   .ambientLights = 13,
                   .directionalLights = 1},
    TimelineCounts{.path = "CIRCUIT4/CIR4-01",
                   .length = 400,
                   .cameras = 1,
                   .models = 13,
                   .transforms = 5,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "CIRCUIT5/CIR5-01",
                   .length = 380,
                   .cameras = 1,
                   .models = 11,
                   .transforms = 16,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "CIRCUIT6/TEST",
                   .length = 660,
                   .cameras = 2,
                   .models = 35,
                   .transforms = 7,
                   .ambientLights = 3,
                   .directionalLights = 1},
    TimelineCounts{.path = "CIRCUIT7/FR",
                   .length = 1480,
                   .cameras = 9,
                   .models = 32,
                   .transforms = 14,
                   .ambientLights = 21,
                   .directionalLights = 6},
    TimelineCounts{.path = "C_AWARD1/1STPLACE",
                   .length = 346,
                   .cameras = 1,
                   .models = 68,
                   .transforms = 9,
                   .ambientLights = 26,
                   .directionalLights = 4},
    TimelineCounts{.path = "C_AWARD2/TWO",
                   .length = 360,
                   .cameras = 2,
                   .models = 23,
                   .transforms = 8,
                   .ambientLights = 18,
                   .directionalLights = 1},
    TimelineCounts{.path = "C_AWARD3/3-01",
                   .length = 330,
                   .cameras = 1,
                   .models = 29,
                   .transforms = 23,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "C_AWARD4/LOSE",
                   .length = 240,
                   .cameras = 1,
                   .models = 6,
                   .transforms = 3,
                   .ambientLights = 12,
                   .directionalLights = 6},
    TimelineCounts{.path = "WINCAR/WINCAR",
                   .length = 440,
                   .cameras = 1,
                   .models = 11,
                   .transforms = 4,
                   .ambientLights = 4,
                   .directionalLights = 3},
    TimelineCounts{.path = "WINRRCAR/END",
                   .length = 1100,
                   .cameras = 3,
                   .models = 106,
                   .transforms = 9,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "WINVVCAR/VVWIN",
                   .length = 440,
                   .cameras = 1,
                   .models = 11,
                   .transforms = 5,
                   .ambientLights = 2,
                   .directionalLights = 1},
    TimelineCounts{.path = "CREDITS/CREDITS",
                   .length = 3510,
                   .cameras = 0,
                   .models = 0,
                   .transforms = 37,
                   .ambientLights = 0,
                   .directionalLights = 0},
};

constexpr std::size_t kCutsceneCount = 17;

template <typename Event>
std::size_t Count(const CutsceneTimelineAsset& timeline, Event CutsceneFrame::* events) {
    std::size_t count = 0;
    for (const CutsceneFrame& frame : timeline.frames) {
        count += (frame.*events).size();
    }
    return count;
}

TimelineCounts CountsOf(const CutsceneTimelineAsset& timeline, std::string_view path,
                        std::int32_t length) {
    return {.path = path,
            .length = length,
            .cameras = Count(timeline, &CutsceneFrame::cameras),
            .models = Count(timeline, &CutsceneFrame::models),
            .transforms = Count(timeline, &CutsceneFrame::transforms),
            .ambientLights = Count(timeline, &CutsceneFrame::ambientLights),
            .directionalLights = Count(timeline, &CutsceneFrame::directionalLights)};
}

CutsceneTimelineAsset ParseRetailTimeline(const JamArchive& archive, std::string_view path) {
    Result<CutsceneTimelineAsset> parsed =
        test::ReadRetailFile(archive, std::format("MENUDATA/{}.CDB", path))
            .and_then([](const FileData& data) { return ParseCutsceneTimeline(data.Bytes()); });
    if (!parsed) {
        FAIL(std::format("{}: {}", path, parsed.error().message));
    }
    return std::move(*parsed);
}

void CheckRetailTimeline(const JamArchive& archive, const TimelineCounts& expected) {
    const CutsceneTimelineAsset timeline = ParseRetailTimeline(archive, expected.path);
    REQUIRE(timeline.frames.size() == 1);
    CHECK(timeline.frames[0].framesPerSecond == 30);
    CHECK(CountsOf(timeline, expected.path, timeline.frames[0].length) == expected);
}

bool HasPreviewLengths(const CutsceneTimelineAsset& preview) {
    return std::ranges::all_of(preview.frames, [](const CutsceneFrame& frame) {
        return frame.length == 199 || frame.length == 200;
    });
}

TEST_CASE("every retail cutscene timeline parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    const test::Survey timelines = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "CDB"; },
        [](std::span<const std::byte> bytes) {
            return ParseCutsceneTimeline(bytes).transform([](const CutsceneTimelineAsset&) {});
        });
    test::CheckSurvey(timelines, kCutsceneCount);
}

TEST_CASE("retail timelines hold the events the original loads") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    for (const TimelineCounts& expected : kTimelines) {
        CheckRetailTimeline(*archive, expected);
    }
}

TEST_CASE("the track preview has a frame per preview name") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    const CutsceneTimelineAsset preview = ParseRetailTimeline(*archive, "SINGRACE/PST");

    CHECK(preview.worlds == std::vector<std::string>{"pst", "blended", "emitter"});
    CHECK(preview.frames.size() == 13);
    CHECK(std::ranges::contains(preview.frames, "adventr2", &CutsceneFrame::name));
    CHECK(HasPreviewLengths(preview));
}

TEST_CASE("the track preview's frames hold the events the original loads") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    const CutsceneTimelineAsset preview = ParseRetailTimeline(*archive, "SINGRACE/PST");

    CHECK(CountsOf(preview, "SINGRACE/PST", 0) == TimelineCounts{.path = "SINGRACE/PST",
                                                                 .cameras = 13,
                                                                 .models = 45,
                                                                 .transforms = 12,
                                                                 .ambientLights = 14,
                                                                 .directionalLights = 13});
}

}
}
