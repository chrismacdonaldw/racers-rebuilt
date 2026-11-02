#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_screen.hpp"
#include "app/menu/race_setup_menu.hpp"
#include "assets/archive_survey.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

std::set<std::string> PreviewFrames(const assets::Vfs& vfs) {
    const Result<assets::CutsceneTimelineAsset> timeline =
        vfs.Read("MENUDATA/SINGRACE/PST.CDB").and_then([](const assets::FileData& data) {
            return assets::ParseCutsceneTimeline(data.Bytes());
        });
    REQUIRE(timeline.has_value());
    std::set<std::string> names;
    for (const assets::CutsceneFrame& frame : timeline->frames) {
        names.insert(assets::ToUpperAscii(frame.name));
    }
    return names;
}

std::vector<std::string> UnknownPreviews(const std::vector<CourseChoice>& courses,
                                         const std::set<std::string>& frames) {
    std::vector<std::string> unknown;
    for (const CourseChoice& course : courses) {
        for (const std::optional<std::string>& preview : course.previews) {
            if (preview && !frames.contains(assets::ToUpperAscii(*preview))) {
                unknown.push_back(*preview);
            }
        }
    }
    return unknown;
}

TEST_CASE("every retail single race previews one frame of the track preview cutscene") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<CourseChoice> tracks =
        LoadTrackChoices(*vfs).value_or(std::vector<CourseChoice>{});

    CHECK(UnknownPreviews(tracks, PreviewFrames(*vfs)).empty());
    CHECK(std::ranges::all_of(
        tracks, [](const CourseChoice& track) { return track.previews.size() == 1; }));
}

TEST_CASE("every retail circuit previews its races' frames in circuit order") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<CourseChoice> circuits =
        LoadCircuitChoices(*vfs, {}).value_or(std::vector<CourseChoice>{});
    REQUIRE(circuits.size() == 7);

    CHECK(UnknownPreviews(circuits, PreviewFrames(*vfs)).empty());
    CHECK(circuits.front().previews ==
          std::vector<std::optional<std::string>>{"pirate1", "castle2", "space1", "advntr1"});
    CHECK(std::ranges::count(circuits.back().previews, std::optional<std::string>{"Final"}) == 1);
}

TEST_CASE("the race pick screens place the track preview where the original does") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string_view layout : {"SINGRACE", "CIRCRACE"}) {
        CAPTURE(layout);
        const Result<CutsceneScreenPlacement> placed =
            ReadCutsceneScreen(*vfs, {.menu = "singrace", .layout = layout});
        const CutsceneScreenPlacement placement = placed.value_or(CutsceneScreenPlacement{});
        const render::Rect& rect = placement.rect;
        CHECK(placement.folder + "/" + placement.cutscene == "MENUDATA/SINGRACE/PST");
        CHECK(std::array{rect.x, rect.y, rect.width, rect.height} ==
              std::array{260.0F, 135.0F, 320.0F, 256.0F});
    }
}

}
}
