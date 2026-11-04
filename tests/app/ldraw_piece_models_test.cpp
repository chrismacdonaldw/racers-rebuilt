#include "app/ldraw_piece_models.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/gltf_models.hpp"
#include "assets/folder_layer.hpp"
#include "assets/geometry_database.hpp"
#include "assets/piece_models.hpp"
#include "assets/vfs.hpp"
#include "gltf_files.hpp"
#include "mods/hook_registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

class LdrawFixture {
public:
    LdrawFixture() {
        models_.DeclareSettings(settings_);
        mods::ResolveThroughHooks(vfs_, hooks_);
        models_.Attach(hooks_, settings_);
    }

    void Turn(bool isEnabled) { settings_.Set(kLdrawPiecesSetting, isEnabled); }

    void Mount(const std::filesystem::path& folder) {
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder);
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    }

    [[nodiscard]] Result<assets::GeometryAsset> Read(std::string_view path) const {
        return vfs_.Read(path).and_then(
            [](const assets::FileData& file) { return assets::ParseGeometry(file.Bytes()); });
    }

private:
    LdrawPieceModels models_{RR_LDRAW_DIR};
    settings::Registry settings_;
    mods::HookRegistry hooks_;
    assets::Vfs vfs_;
};

TEST_CASE_FIXTURE(LdrawFixture, "a mapped piece's model is its LDraw part on its cells") {
    Turn(true);

    const Result<assets::GeometryAsset> brick = Read(assets::PieceModelPath("l300100"));

    REQUIRE(brick.has_value());
    CHECK(brick->materials == std::vector<std::string>{std::string(assets::kPieceColourMaterial)});
    const auto [low, high] =
        std::ranges::minmax(brick->vertices, {},
                            [](const assets::GeometryVertex& vertex) { return vertex.position.z; });
    CHECK(low.position.z == doctest::Approx(0.0F));
    CHECK(high.position.z == doctest::Approx(1.4F));
    CHECK(Read(assets::PieceModelPath("L300100")).has_value());
}

TEST_CASE_FIXTURE(LdrawFixture,
                  "with the setting off, or for other pieces and paths, nothing is made") {
    Turn(false);
    CHECK_FALSE(Read(assets::PieceModelPath("L300100")).has_value());

    Turn(true);
    CHECK_FALSE(Read(assets::PieceModelPath("SLAMP")).has_value());
    CHECK_FALSE(Read("MENUDATA/PIECEDB/L300100.GDB").has_value());
}

TEST_CASE_FIXTURE(LdrawFixture, "an extra piece's model is made whatever the setting says") {
    Turn(false);

    const Result<assets::GeometryAsset> plate = Read(assets::PieceModelPath("x3024"));

    REQUIRE(plate.has_value());
    const auto [low, high] =
        std::ranges::minmax(plate->vertices, {},
                            [](const assets::GeometryVertex& vertex) { return vertex.position.z; });
    CHECK(low.position.z == doctest::Approx(0.0F));
    CHECK(high.position.z == doctest::Approx(0.6F));
}

TEST_CASE("a mod's glb piece model wins over the LDraw part") {
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "menudata/piecedb/models/l300100.glb";
    std::filesystem::create_directories(file.parent_path());
    const std::vector<std::byte> glb = test::BuildGlb({.material = "piece"});
    std::ofstream(file, std::ios::binary)
        .write(reinterpret_cast<const char*>(glb.data()), std::ssize(glb));
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder.Path());
    REQUIRE(layer.has_value());
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    settings::Registry settings;
    mods::HookRegistry hooks;
    GltfModels gltf;
    LdrawPieceModels ldraw(RR_LDRAW_DIR);
    gltf.DeclareSettings(settings);
    ldraw.DeclareSettings(settings);
    settings.Set(kLdrawPiecesSetting, true);
    mods::ResolveThroughHooks(vfs, hooks);
    gltf.Attach(hooks, settings);
    ldraw.Attach(hooks, settings);

    const Result<assets::GeometryAsset> model =
        vfs.Read(assets::PieceModelPath("L300100")).and_then([](const assets::FileData& data) {
            return assets::ParseGeometry(data.Bytes());
        });

    REQUIRE(model.has_value());
    CHECK(model->vertices.size() == 3);
    CHECK(model->materials == std::vector<std::string>{"piece"});
}

TEST_CASE_FIXTURE(LdrawFixture, "a model file in a layer wins over the LDraw part") {
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "menudata/piecedb/models/l300100.gdb";
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << "not a model";
    Mount(folder.Path());
    Turn(true);

    CHECK_FALSE(Read(assets::PieceModelPath("L300100")).has_value());
}

}
}
