#include "app/gltf_models.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "assets/geometry_database.hpp"
#include "gltf_files.hpp"
#include "mods/hook_registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

void WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
}

class GltfFixture {
public:
    GltfFixture() {
        WriteBytes(folder_.Path() / "gamedata" / "kart.glb", test::BuildGlb({}));
        WriteBytes(folder_.Path() / "gamedata" / "broken.glb", std::vector(8, std::byte{0x42}));
        WriteBytes(folder_.Path() / "gamedata" / "broken.gdb", std::vector(4, std::byte{0x07}));
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder_.Path());
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
        models_.DeclareSettings(settings_);
        mods::ResolveThroughHooks(vfs_, hooks_);
        models_.Attach(hooks_, settings_);
    }

    void Turn(bool isEnabled) { settings_.Set(kGltfModels, isEnabled); }

    [[nodiscard]] Result<assets::FileData> Read(std::string_view path) const {
        return vfs_.Read(path);
    }

private:
    test::TempFolder folder_;
    assets::Vfs vfs_;
    mods::HookRegistry hooks_;
    settings::Registry settings_;
    GltfModels models_;
};

TEST_CASE("only gdb models have a glb sibling") {
    CHECK(GlbSibling("GAMEDATA/COMMON/KART.GDB") == "GAMEDATA/COMMON/KART.GLB");
    CHECK_FALSE(GlbSibling("GAMEDATA/COMMON/KART.MDB").has_value());
}

TEST_CASE_FIXTURE(GltfFixture, "a glb is built into the gdb it shares a name with") {
    const Result<assets::GeometryAsset> kart =
        Read("GAMEDATA/KART.GDB").and_then([](const assets::FileData& file) {
            return assets::ParseGeometry(file.Bytes());
        });

    REQUIRE(kart.has_value());
    CHECK(kart->vertices.size() == 3);
    CHECK(kart->materials == std::vector<std::string>{"STONE"});
}

TEST_CASE_FIXTURE(GltfFixture, "a glb that can't be imported leaves the game model") {
    const Result<assets::FileData> broken = Read("GAMEDATA/BROKEN.GDB");

    REQUIRE(broken.has_value());
    CHECK(broken->Bytes().size() == 4);
}

TEST_CASE_FIXTURE(GltfFixture, "turning glTF models off stops building them") {
    Turn(false);

    CHECK_FALSE(Read("GAMEDATA/KART.GDB").has_value());

    Turn(true);

    CHECK(Read("GAMEDATA/KART.GDB").has_value());
}

}
}
