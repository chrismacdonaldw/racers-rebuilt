#include "audio/music_catalog.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "assets/folder_layer.hpp"
#include "temp_folder.hpp"

namespace rr::audio {
namespace {

void MountFiles(assets::Vfs& vfs, const std::filesystem::path& root,
                std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const std::filesystem::path path = root / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "x";
    }
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root);
    REQUIRE(layer.has_value());
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
}

TEST_CASE("the catalog lists the install's top-level tracks by lowercase name") {
    const test::TempFolder folder;
    assets::Vfs vfs;
    MountFiles(vfs, folder.Path(), {"CIRCUIT1.TUN", "menu.tun", "ENGINE.PCM", "MUSIC/EXTRA.TUN"});

    const Result<MusicCatalog> catalog = MusicCatalog::Scan(vfs);

    REQUIRE(catalog.has_value());
    CHECK(catalog->Ids() == std::vector<std::string>{"circuit1", "menu"});
    CHECK(catalog->Contains("CIRCUIT1"));
    CHECK_FALSE(catalog->Contains("extra"));
}

}
}
