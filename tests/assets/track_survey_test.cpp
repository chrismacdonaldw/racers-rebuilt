#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "assets/jam_archive.hpp"
#include "assets/track_loader.hpp"
#include "game_files.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kRaceFolderCount = 13;

std::vector<std::string> RaceFolders(const Vfs& vfs) {
    std::vector<std::string> folders = ListRaceTracks(vfs);
    for (std::string& folder : folders) {
        folder.insert(0, "GAMEDATA/");
    }
    return folders;
}

void CheckTrackLoads(const Vfs& vfs, LoaderPool& pool, const std::string& folder) {
    TrackLoader loader(vfs, pool);
    const auto start = std::chrono::steady_clock::now();
    const Result<TrackAssets> track = loader.Load(folder);
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
    if (!track) {
        FAIL_CHECK(track.error().message);
        return;
    }
    CHECK(track->missingTextures.empty());
    MESSAGE(std::format("{}: {} worlds, {} models, {} materials, {} textures in {:.0f} ms", folder,
                        track->worlds.size(), track->models.size(), track->materials.size(),
                        track->textures.size(), elapsed.count()));
}

TEST_CASE("every retail race track loads") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    Result<JamArchive> archive = JamArchive::Open(*gameFolder / "LEGO.JAM");
    REQUIRE(archive.has_value());
    Vfs vfs;
    vfs.Mount(std::make_unique<JamArchive>(std::move(*archive)));
    LoaderPool pool(LoaderPool::DefaultWorkerCount());

    const std::vector<std::string> folders = RaceFolders(vfs);
    CHECK(folders.size() == kRaceFolderCount);
    for (const std::string& folder : folders) {
        CheckTrackLoads(vfs, pool, folder);
    }
}

}
}
