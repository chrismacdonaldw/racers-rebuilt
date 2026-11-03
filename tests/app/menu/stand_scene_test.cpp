#include "app/menu/stand_scene.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/archive_survey.hpp"
#include "assets/loader_pool.hpp"
#include "assets/original_save.hpp"
#include "core/quaternion.hpp"
#include "sim/animation/part_clock.hpp"

namespace rr::app {
namespace {

constexpr std::array kStandScreens{std::string_view("GARAGE"), std::string_view("PICKRCR")};

StandScene LoadScene(const assets::Vfs& vfs, std::string_view screen) {
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(vfs, pool);
    const Result<MenuFiles> files = ReadMenuFiles(vfs, screen);
    REQUIRE(files.has_value());
    Result<StandScene> scene = LoadStandScene(vfs, loader, *files, screen);
    REQUIRE_MESSAGE(scene.has_value(), scene.error().message);
    return std::move(*scene);
}

Vector3 CarForward(const StandScene& scene, float frame) {
    const sim::EntityClock clock = sim::PartClock{.part = 0, .frame = frame};
    return Rotate(StandCarPlacement(scene.sources, scene.turntable, clock).rotation, {.x = 1.0F});
}

void CheckScene(const assets::Vfs& vfs, std::string_view screen) {
    CAPTURE(screen);
    const StandScene scene = LoadScene(vfs, screen);
    CHECK(scene.camera.verticalFovDegrees == doctest::Approx(31.7687F));
    CHECK(scene.sources.entities.at(scene.turntable).name == "crsdow");
    CHECK(scene.driver == scene.sources.entities.size() - 1);
    CHECK(std::set(scene.idles.begin(), scene.idles.end()).size() == 7);
    CHECK(scene.cheers.size() == 3);
}

void CheckRacerFile(const assets::Vfs& vfs, const assets::DriverPartCatalog& parts,
                    const assets::AnimationAsset& poses, std::string_view file) {
    CAPTURE(file);
    const Result<std::vector<assets::SavedRacer>> racers = vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseRacerFile(data.Bytes()); });
    REQUIRE(racers.has_value());
    for (std::size_t racer = 0; racer < racers->size(); ++racer) {
        CAPTURE(racer);
        const Result<assets::SkeletonAsset> rig =
            LoadMenuMinifigSkeleton(vfs, parts, racers->at(racer).look);
        REQUIRE_MESSAGE(rig.has_value(), rig.error().message);
        CHECK(sim::CheckPartTracks(poses, rig->nodes.size()).has_value());
    }
}

}

TEST_CASE("the garage and racer pick stand loads RS_SET with its camera and the racer's poses") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string_view screen : kStandScreens) {
        CheckScene(*vfs, screen);
    }
}

TEST_CASE("the stand's car turns once round with the turntable") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const StandScene scene = LoadScene(*vfs, "GARAGE");
    const Vector3 start = CarForward(scene, 0.0F);
    const Vector3 half = CarForward(scene, 100.0F);
    const Vector3 end = CarForward(scene, 200.0F);

    CHECK(start.z == doctest::Approx(0.0F));
    CHECK(Dot(start, half) == doctest::Approx(-1.0F).epsilon(0.001));
    CHECK(Dot(start, end) == doctest::Approx(1.0F).epsilon(0.001));
}

TEST_CASE("RSANIM poses the menu rig of every prebuilt racer") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const StandScene scene = LoadScene(*vfs, "PICKRCR");
    const assets::AnimationAsset& poses =
        scene.sources.animations.at(scene.sources.entities.at(scene.driver).animation);
    const Result<assets::DriverPartCatalog> parts = LoadDriverParts(*vfs);
    REQUIRE(parts.has_value());
    for (const std::string_view file : {"MENUDATA/DEFAULT.LRS", "MENUDATA/QBUILD.LRS"}) {
        CheckRacerFile(*vfs, *parts, poses, file);
    }
}

}
