#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/award_driver.hpp"
#include "app/cutscene/cutscene_screen.hpp"
#include "app/menu/race_setup_menu.hpp"
#include "assets/archive_survey.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"
#include "assets/racer_catalog.hpp"
#include "sim/track/track_entity_sources.hpp"

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

std::vector<std::string> UnknownPreviews(const assets::Vfs& vfs,
                                         const std::vector<CourseChoice>& courses) {
    const std::set<std::string> frames = PreviewFrames(vfs);
    const Result<assets::RacerCatalog> drivers = assets::RacerCatalog::Load(vfs);
    REQUIRE(drivers.has_value());
    std::vector<std::string> unknown;
    for (const CourseChoice& course : courses) {
        for (const std::optional<RacePreview>& preview : course.previews) {
            if (preview && !frames.contains(assets::ToUpperAscii(preview->frame))) {
                unknown.push_back(preview->frame);
            }
            if (preview && !drivers->Find(preview->driver)) {
                unknown.push_back(preview->driver);
            }
        }
    }
    return unknown;
}

std::vector<std::string> FramesOf(const CourseChoice& course) {
    std::vector<std::string> frames;
    frames.reserve(course.previews.size());
    for (const std::optional<RacePreview>& preview : course.previews) {
        frames.push_back(preview ? preview->frame : std::string());
    }
    return frames;
}

TEST_CASE("every retail single race previews one frame and mascot of the track preview") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<CourseChoice> tracks =
        LoadTrackChoices(*vfs).value_or(std::vector<CourseChoice>{});

    CHECK(UnknownPreviews(*vfs, tracks).empty());
    CHECK(std::ranges::all_of(
        tracks, [](const CourseChoice& track) { return track.previews.size() == 1; }));
}

TEST_CASE("every retail time trial previews Veronica Voltage") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<CourseChoice> trials =
        LoadTrialChoices(*vfs, {}).value_or(std::vector<CourseChoice>{});
    REQUIRE_FALSE(trials.empty());

    CHECK(UnknownPreviews(*vfs, trials).empty());
    CHECK(std::ranges::all_of(trials, [](const CourseChoice& trial) {
        return trial.previews.front().value_or(RacePreview{}).driver == "VV";
    }));
}

TEST_CASE("every retail circuit previews its races' frames in circuit order") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<CourseChoice> circuits =
        LoadCircuitChoices(*vfs, {}).value_or(std::vector<CourseChoice>{});
    REQUIRE(circuits.size() == 7);

    CHECK(UnknownPreviews(*vfs, circuits).empty());
    CHECK(FramesOf(circuits.front()) ==
          std::vector<std::string>{"pirate1", "castle2", "space1", "advntr1"});
    CHECK(std::ranges::count(FramesOf(circuits.back()), "Final") == 1);
}

TEST_CASE("every retail preview driver loads as a menu minifig") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    std::set<std::string> drivers;
    for (const CourseChoice& course : LoadCircuitChoices(*vfs, {}).value_or({})) {
        for (const std::optional<RacePreview>& preview : course.previews) {
            drivers.insert(preview.value_or(RacePreview{}).driver);
        }
    }
    drivers.insert("VV");
    drivers.erase("");
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    std::vector<std::string> failed;
    for (const std::string& driver : drivers) {
        const Result<AwardDriver> loaded =
            AwardLook(*vfs, {.driver = driver}).and_then([&](const assets::DriverLook& look) {
                return LoadAwardDriver(*vfs, loader, look);
            });
        if (!loaded) {
            failed.push_back(std::format("{}: {}", driver, loaded.error().message));
        }
    }
    CHECK(drivers.size() > 4);
    CHECK(failed.empty());
}

std::size_t PreviewDriverJoints(const assets::Vfs& vfs) {
    const Result<assets::CutsceneTimelineAsset> timeline =
        vfs.Read("MENUDATA/SINGRACE/PST.CDB").and_then([](const assets::FileData& data) {
            return assets::ParseCutsceneTimeline(data.Bytes());
        });
    REQUIRE(timeline.has_value());
    const Result<sim::TrackEntitySources> sources =
        sim::LoadWorldEntities(vfs, "MENUDATA/SINGRACE", timeline->worlds);
    REQUIRE(sources.has_value());
    const auto guy1 = std::ranges::find(sources->entities, "guy1", &sim::TrackEntity::name);
    REQUIRE(guy1 != sources->entities.end());
    return sources->skeletons.at(guy1->skeleton).nodes.size();
}

TEST_CASE("the track preview's guy1 has a joint for every joint of a menu minifig") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<AwardDriver> driver = LoadAwardDriver(*vfs, loader, {});
    REQUIRE(driver.has_value());
    const std::uint16_t lastJoint =
        std::ranges::max(driver->minifig.vertices, {}, &assets::GeometryVertex::joint).joint;

    CHECK(lastJoint < PreviewDriverJoints(*vfs));
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
