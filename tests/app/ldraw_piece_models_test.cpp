#include "app/ldraw_piece_models.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "assets/geometry_database.hpp"
#include "assets/piece_models.hpp"
#include "assets/vfs.hpp"
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
