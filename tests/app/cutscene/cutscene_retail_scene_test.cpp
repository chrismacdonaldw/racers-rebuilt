#include <doctest/doctest.h>

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "assets/archive_survey.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"

namespace rr::app {
namespace {

struct RetailCutscene {
    std::string_view folder;
    std::string_view file;
};

constexpr std::array kCutscenes{
    RetailCutscene{.folder = "LEGAL", .file = "SPLASH1"},
    RetailCutscene{.folder = "CIRCUIT1", .file = "CIR1-01"},
    RetailCutscene{.folder = "CIRCUIT2", .file = "TEST"},
    RetailCutscene{.folder = "CIRCUIT3", .file = "CIR3-01"},
    RetailCutscene{.folder = "CIRCUIT4", .file = "CIR4-01"},
    RetailCutscene{.folder = "CIRCUIT5", .file = "CIR5-01"},
    RetailCutscene{.folder = "CIRCUIT6", .file = "TEST"},
    RetailCutscene{.folder = "CIRCUIT7", .file = "FR"},
    RetailCutscene{.folder = "C_AWARD1", .file = "1STPLACE"},
    RetailCutscene{.folder = "C_AWARD2", .file = "TWO"},
    RetailCutscene{.folder = "C_AWARD3", .file = "3-01"},
    RetailCutscene{.folder = "C_AWARD4", .file = "LOSE"},
    RetailCutscene{.folder = "WINCAR", .file = "WINCAR"},
    RetailCutscene{.folder = "WINRRCAR", .file = "END"},
    RetailCutscene{.folder = "WINVVCAR", .file = "VVWIN"},
    RetailCutscene{.folder = "CREDITS", .file = "CREDITS"},
    RetailCutscene{.folder = "SINGRACE", .file = "PST"},
};

void CheckCutscene(const assets::Vfs& vfs, assets::TrackLoader& loader,
                   const RetailCutscene& cutscene) {
    CAPTURE(cutscene.file);
    const std::string folder = std::format("MENUDATA/{}", cutscene.folder);
    const Result<assets::CutsceneTimelineAsset> timeline =
        vfs.Read(std::format("{}/{}.CDB", folder, cutscene.file))
            .and_then([](const assets::FileData& data) {
                return assets::ParseCutsceneTimeline(data.Bytes());
            });
    REQUIRE(timeline.has_value());
    const Result<assets::TrackAssets> assets = loader.LoadWorlds(folder, timeline->worlds);
    REQUIRE_MESSAGE(assets.has_value(), assets.error().message);
    Result<sim::TrackEntitySources> sources = sim::LoadWorldEntities(vfs, folder, timeline->worlds);
    REQUIRE_MESSAGE(sources.has_value(), sources.error().message);
    std::vector<const assets::WorldAsset*> worlds;
    for (const auto& world : assets->worlds) {
        worlds.push_back(world.get());
    }
    for (const assets::CutsceneFrame& frame : timeline->frames) {
        const Result<CutsceneBindings> bindings = BindCutscene(frame, worlds, {}, *sources);
        REQUIRE_MESSAGE(bindings.has_value(), bindings.error().message);
    }
}

TEST_CASE("every retail cutscene binds to its worlds") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    for (const RetailCutscene& cutscene : kCutscenes) {
        CheckCutscene(*vfs, loader, cutscene);
    }
}

}
}
