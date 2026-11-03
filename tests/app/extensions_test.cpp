#include "app/extensions.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "assets/folder_layer.hpp"
#include "assets/jam_builder.hpp"
#include "settings/registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kManifest = R"(id = "paint"
name = "Paint"
version = "1.0"
)";
constexpr std::string_view kFile = "GAMEDATA/A.TXT";

void WriteText(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string ReadText(const assets::Vfs& vfs) {
    const Result<assets::FileData> data = vfs.Read(kFile);
    return data ? std::string(test::AsText(data->Bytes())) : std::string();
}

TEST_CASE("remounting swaps in the mods that the settings enable now") {
    const test::TempFolder folder;
    WriteText(folder.Path() / "game" / kFile, "game");
    WriteText(folder.Path() / "mods" / "paint" / "mod.toml", kManifest);
    WriteText(folder.Path() / "mods" / "paint" / "files" / kFile, "mod");
    Extensions extensions = LoadExtensions(folder.Path() / "mods", folder.Path() / "overrides");
    settings::Registry registry;
    DeclareExtensionSettings(extensions, registry);
    Result<assets::FolderLayer> game = assets::FolderLayer::Scan(folder.Path() / "game");
    REQUIRE(game.has_value());
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*game)));
    MountExtensions(extensions, vfs, registry);
    const std::string modded = ReadText(vfs);

    REQUIRE(registry.Assign("mods.paint.enabled", settings::SettingValue(false)).has_value());
    RemountExtensions(extensions, vfs, registry);

    CHECK(modded == "mod");
    CHECK(ReadText(vfs) == "game");
    CHECK(vfs.LayerCount() == 1);
}

}
}
