#include "app/cutscene/award_driver.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_materials.hpp"
#include "app/cutscene/cutscene_racer.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/archive_survey.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/driver_model.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"
#include "assets/skeleton_database.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {
namespace {

using Model = assets::AssetCache<assets::GeometryAsset>::Handle;

Model ModelWith(std::vector<std::string> materials) {
    return std::make_shared<const assets::GeometryAsset>(
        assets::GeometryAsset{.materials = std::move(materials)});
}

assets::TrackAssets Stage() {
    assets::WorldAsset world{.models = {"guy1", "swap", "pleg"}};
    for (const std::string_view name : {"Guy1", "swap", "pleg"}) {
        world.animatedEntities.push_back(
            {.name = std::string(name),
             .lods = {{.model = static_cast<std::int32_t>(world.animatedEntities.size())}}});
    }
    assets::TrackAssets stage{
        .worlds = {std::make_shared<const assets::WorldAsset>(std::move(world))},
        .models = {{"GUY1", ModelWith({"dflt"})},
                   {"SWAP", ModelWith({"legs", "hand"})},
                   {"PLEG", ModelWith({"peg"})}}};
    stage.materials.emplace("CR_DFLT", assets::MaterialAsset{.name = "stage"});
    return stage;
}

TEST_CASE("a prebuilt driver's look takes its table's part indices") {
    const assets::DriverLook look = DriverLookOf(
        {.hatIndex = 1, .faceIndex = 2, .torsoIndex = 3, .legsIndex = 10, .expressionIndex = 4});

    CHECK(look == assets::DriverLook{.face = 2, .hat = 1, .legs = 10, .torso = 3, .expression = 4});
}

TEST_CASE("the award driver takes guy1's place and lends swap and pleg its legs") {
    assets::TrackAssets stage = Stage();
    const AwardDriver driver{
        .face = "cr_",
        .minifig = {.materials = {"cr_dflt", "cr_legs", "cr_chst"}},
        .looks = {.materials = {{"CR_DFLT", assets::MaterialAsset{.name = "driver"}}}}};

    DressAwardDriver(driver, stage);

    CHECK(stage.models.at("GUY1")->materials == driver.minifig.materials);
    CHECK(stage.models.at("SWAP")->materials == std::vector<std::string>{"cr_legs", "hand"});
    CHECK(stage.models.at("PLEG")->materials == std::vector<std::string>{"cr_legs"});
    CHECK(stage.materials.at("CR_DFLT").name == "driver");
}

struct AwardStage {
    std::string_view folder;
    std::string_view file;
};

constexpr std::array kAwardStages{
    AwardStage{.folder = "C_AWARD1", .file = "1STPLACE"},
    AwardStage{.folder = "C_AWARD2", .file = "TWO"},
    AwardStage{.folder = "C_AWARD3", .file = "3-01"},
    AwardStage{.folder = "C_AWARD4", .file = "LOSE"},
    AwardStage{.folder = "WINCAR", .file = "WINCAR"},
    AwardStage{.folder = "WINRRCAR", .file = "END"},
    AwardStage{.folder = "CIRCUIT7", .file = "FR"},
};

std::vector<std::optional<std::size_t>> Parents(const assets::SkeletonAsset& skeleton) {
    std::vector<std::optional<std::size_t>> parents;
    parents.reserve(skeleton.nodes.size());
    for (const assets::SkeletonNode& node : skeleton.nodes) {
        parents.push_back(node.parent);
    }
    return parents;
}
std::vector<std::optional<std::size_t>> BodyRig(const assets::Vfs& vfs,
                                                const assets::DriverLook& look) {
    const Result<assets::SkeletonAsset> body =
        LoadDriverParts(vfs)
            .and_then([&](const assets::DriverPartCatalog& parts) {
                return assets::DriverBodyName(parts, look, assets::DriverDetail::Menu);
            })
            .and_then([&](const std::string& name) {
                return vfs.Read(
                    std::format("MENUDATA/PARTDB/MENUPART/{}.SDB", assets::ToUpperAscii(name)));
            })
            .and_then(
                [](const assets::FileData& data) { return assets::ParseSkeleton(data.Bytes()); });
    REQUIRE(body.has_value());
    return Parents(*body);
}

std::string DressedStage(const assets::Vfs& vfs, assets::TrackLoader& loader,
                         const AwardDriver& driver,
                         std::span<const std::optional<std::size_t>> body,
                         const AwardStage& stage) {
    const std::string folder = std::format("MENUDATA/{}", stage.folder);
    const Result<assets::CutsceneTimelineAsset> timeline =
        vfs.Read(std::format("{}/{}.CDB", folder, stage.file))
            .and_then([](const assets::FileData& data) {
                return assets::ParseCutsceneTimeline(data.Bytes());
            });
    Result<assets::TrackAssets> staged =
        timeline.and_then([&](const assets::CutsceneTimelineAsset& loaded) {
            return loader.LoadWorlds(folder, loaded.worlds);
        });
    const Result<sim::TrackEntitySources> sources =
        staged.and_then([&](const assets::TrackAssets&) {
            return sim::LoadWorldEntities(vfs, folder, timeline->worlds);
        });
    REQUIRE(sources.has_value());
    DressAwardDriver(driver, *staged);
    std::vector<const assets::WorldAsset*> worlds;
    worlds.reserve(staged->worlds.size());
    for (const auto& world : staged->worlds) {
        worlds.push_back(world.get());
    }
    const assets::CutsceneFrame& frame = timeline->frames.front();
    Result<WorldAnimations> animations = LoadWorldAnimations(vfs, folder, worlds);
    REQUIRE(animations.has_value());
    ShowDriverFaces(timeline->frames, driver.face, *animations);
    assets::NameMap<render::Material> materials;
    for (const auto& entry : staged->materials) {
        materials.emplace(entry.first, render::Material{});
    }
    const Result<CutsceneMaterials> tracks =
        BindCutscene(frame, worlds, {}, *sources).and_then([&](const CutsceneBindings& bound) {
            return CutsceneMaterials::Create(frame, bound.models, *animations, materials);
        });
    const auto guy1 = std::ranges::find(sources->entities, "guy1", &sim::TrackEntity::name);
    REQUIRE(guy1 != sources->entities.end());
    const bool isMenuRig = std::ranges::equal(Parents(sources->skeletons.at(guy1->skeleton)), body);
    return std::format("{} faces {}, {}", stage.file, tracks ? "shown" : tracks.error().message,
                       isMenuRig ? "menu rig" : "another rig");
}

TEST_CASE("every retail award stage dresses guy1 as the driver on the menu minifig's rig") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const assets::DriverLook look{.face = 1, .hat = 1, .legs = 1, .torso = 1};
    Result<AwardDriver> driver = LoadAwardDriver(*vfs, loader, look);
    REQUIRE_MESSAGE(driver.has_value(), driver.error().message);
    const std::vector<std::optional<std::size_t>> body = BodyRig(*vfs, look);
    const std::uint16_t lastJoint =
        std::ranges::max(driver->minifig.vertices, {}, &assets::GeometryVertex::joint).joint;
    CHECK(lastJoint < body.size());
    std::vector<std::string> dressed;
    dressed.reserve(kAwardStages.size());
    for (const AwardStage& stage : kAwardStages) {
        dressed.push_back(DressedStage(*vfs, loader, *driver, body, stage));
    }
    std::vector<std::string> expected;
    expected.reserve(kAwardStages.size());
    for (const AwardStage& stage : kAwardStages) {
        expected.push_back(std::format("{} faces shown, menu rig", stage.file));
    }
    CHECK(dressed == expected);
}

}
}
