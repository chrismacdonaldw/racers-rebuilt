#include "app/menu/menu_champion.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string_view>
#include <utility>

#include "app/cutscene/award_driver.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/animation_database.hpp"
#include "assets/archive_survey.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/world_database.hpp"
#include "sim/animation/part_clock.hpp"

namespace rr::app {
namespace {

assets::AnimationAsset LoadPoses(const assets::Vfs& vfs) {
    Result<assets::AnimationAsset> poses =
        vfs.Read("MENUDATA/LEGOMAN.ADB").and_then([](const assets::FileData& data) {
            return assets::ParseAnimations(data.Bytes());
        });
    REQUIRE(poses.has_value());
    REQUIRE(poses->parts.size() == 3);
    CHECK(poses->parts[2].frameCount > 208);
    return std::move(*poses);
}

void CheckPosed(const assets::Vfs& vfs, const assets::RacerCatalog& catalog,
                const assets::DriverPartCatalog& parts, const assets::AnimationAsset& poses,
                std::string_view champion) {
    CAPTURE(champion);
    const Result<assets::SkeletonAsset> rig =
        catalog.Find(champion)
            .and_then([](const assets::RacerFiles& racer) { return DriverLookOf(racer.driver); })
            .and_then([&](const assets::DriverLook& look) {
                return LoadMenuMinifigSkeleton(vfs, parts, look);
            });
    REQUIRE_MESSAGE(rig.has_value(), rig.error().message);
    CHECK(sim::CheckPartTracks(poses, rig->nodes.size()).has_value());
}

}

TEST_CASE("the main menu shows the host of the last circuit unlocked in order") {
    CHECK(MenuChampionCode({}) == "CR");
    CHECK(MenuChampionCode({"c1"}) == "KK");
    CHECK(MenuChampionCode({"c2"}) == "CR");
    CHECK(MenuChampionCode({"c1", "c2", "c4"}) == "BB");
    CHECK(MenuChampionCode({"c1", "c2", "c3", "c4"}) == "BVB");
    CHECK(MenuChampionCode({"c1", "c2", "c3", "c4", "c5"}) == "GM");
    CHECK(MenuChampionCode({"c1", "c2", "c3", "c4", "c5", "c6"}) == "RR");
}

TEST_CASE("LEGOMAN poses every main menu champion through the glass tap") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const assets::AnimationAsset poses = LoadPoses(*vfs);
    const Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(*vfs);
    const Result<assets::DriverPartCatalog> parts = LoadDriverParts(*vfs);
    REQUIRE(catalog.has_value());
    REQUIRE(parts.has_value());
    for (const std::string_view champion : {"CR", "KK", "BB", "JT", "BVB", "GM", "RR"}) {
        CheckPosed(*vfs, *catalog, *parts, poses, champion);
    }
}

TEST_CASE("the main menu platform's world has a camera") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<assets::WorldAsset> world =
        vfs->Read("MENUDATA/TT.WDB").and_then([](const assets::FileData& data) {
            return assets::ParseWorld(data.Bytes());
        });
    REQUIRE(world.has_value());
    REQUIRE_FALSE(world->cameras.empty());
    CHECK(world->cameras.front().name == "Camera01");
}

}
