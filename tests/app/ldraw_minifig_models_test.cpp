#include "app/ldraw_minifig_models.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "app/ldraw_piece_models.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/archive_survey.hpp"
#include "assets/folder_layer.hpp"
#include "assets/geometry_database.hpp"
#include "assets/part_library.hpp"
#include "mods/hook_registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kRaceHeads = "MENUDATA/PARTDB/GAMEPART/ICB_CHAR.GCB";
constexpr std::string_view kRaceBody = "MENUDATA/PARTDB/GAMEPART/LEG_BOX.GDB";

class MinifigFixture {
public:
    explicit MinifigFixture(assets::Vfs vfs) : vfs_(std::move(vfs)) {
        settings_.Declare(kLdrawPiecesSetting);
        mods::ResolveThroughHooks(vfs_, hooks_);
        models_.Attach(hooks_, settings_);
    }

    void Turn(bool isEnabled) { settings_.Set(kLdrawPiecesSetting, isEnabled); }

    [[nodiscard]] assets::Vfs& Vfs() { return vfs_; }

    [[nodiscard]] std::size_t FaceTriangles() const {
        const Result<assets::PartLibrary> heads = LoadHeadParts(vfs_, kRaceHeads);
        REQUIRE(heads.has_value());
        std::size_t count = 0;
        for (const assets::LibraryPart& part : heads.value_or(assets::PartLibrary{}).parts) {
            for (const assets::PartGroup& group : part.groups) {
                count += group.material == "face" ? group.triangles.size() : 0;
            }
        }
        return count;
    }

    [[nodiscard]] std::size_t BodyTriangles() const {
        const Result<assets::GeometryAsset> body = vfs_.Read(kRaceBody).and_then(
            [](const assets::FileData& file) { return assets::ParseGeometry(file.Bytes()); });
        REQUIRE(body.has_value());
        return body.value_or(assets::GeometryAsset{}).indices.size() / 3;
    }

private:
    assets::Vfs vfs_;
    settings::Registry settings_;
    mods::HookRegistry hooks_;
    LdrawMinifigModels models_{RR_LDRAW_DIR};
};

TEST_CASE("only driver bodies with a figure and head libraries are minifig files") {
    LdrawMinifigModels models(RR_LDRAW_DIR);
    const assets::Vfs vfs;

    CHECK_FALSE(models.ModelFile(vfs, "MENUDATA/PARTDB/MENUPART/HEADS.GDB").has_value());
    CHECK_FALSE(models.ModelFile(vfs, "MENUDATA/PIECEDB/MODELS/L300100.GDB").has_value());
    CHECK_FALSE(models.ModelFile(vfs, kRaceHeads).has_value());
}

TEST_CASE("with the setting on, the race driver's body and heads are LDraw") {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    MinifigFixture fixture(std::move(*vfs));
    fixture.Turn(false);
    const std::size_t originalFaces = fixture.FaceTriangles();
    const std::size_t originalBody = fixture.BodyTriangles();

    fixture.Turn(true);

    CHECK(fixture.FaceTriangles() > originalFaces * 4);
    CHECK(fixture.BodyTriangles() > originalBody * 4);
}

TEST_CASE("a mod's head library wins over the LDraw heads") {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<assets::FileData> original = vfs->Read(kRaceHeads);
    REQUIRE(original.has_value());
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "menudata/partdb/gamepart/icb_char.gcb";
    std::filesystem::create_directories(file.parent_path());
    const std::span<const std::byte> bytes = original->Bytes();
    std::ofstream(file, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder.Path());
    REQUIRE(layer.has_value());
    vfs->Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    MinifigFixture fixture(std::move(*vfs));
    fixture.Turn(false);
    const std::size_t modFaces = fixture.FaceTriangles();

    fixture.Turn(true);

    CHECK(fixture.FaceTriangles() == modFaces);
}

}
}
