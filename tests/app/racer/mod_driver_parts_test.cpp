#include "app/racer/mod_driver_parts.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/racer/racer_models.hpp"
#include "assets/archive_survey.hpp"
#include "assets/driver_model.hpp"
#include "assets/folder_layer.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "gltf_files.hpp"
#include "image_files.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kLavaParts =
    "[[hat]]\nname = \"lava-helmet\"\nmodel = \"lava/helmet.glb\"\n"
    "[[face]]\nname = \"lv_\"\ndefault = \"lava/face.png\"\nhappy = \"lava/happy.png\"\n"
    "[[torso]]\nname = \"lava-suit\"\ntexture = \"lava/suit.png\"\nhook = true\n"
    "[[legs]]\nname = \"lava-legs\"\ntexture = \"lava/legs.png\"\n"
    "[[colour]]\nname = \"lava_red\"\nrgb = [200, 60, 20]\n";

ModDriverParts Parsed(std::string_view text) {
    Result<ModDriverParts> parts = ParseModDriverParts(text, "PARTS.TOML");
    REQUIRE(parts.has_value());
    return std::move(*parts);
}

class ModFolders {
public:
    void Add(std::string_view parts) {
        root_ = folder_.Path() / std::to_string(count_++);
        Write("PARTS.TOML", parts);
        Mount();
    }

    void AddLava() {
        root_ = folder_.Path() / std::to_string(count_++);
        Write("PARTS.TOML", kLavaParts);
        const std::vector<std::byte> helmet = test::BuildGlb({.material = "face"});
        Write("lava/helmet.glb",
              std::string_view(reinterpret_cast<const char*>(helmet.data()), helmet.size()));
        for (const std::string_view image : {"face", "happy", "suit", "legs"}) {
            test::WritePng(PartPath(std::format("lava/{}.png", image)),
                           test::SolidImage(4, 4, std::byte{0x80}));
        }
        Mount();
    }

    bool MountRetail() {
        std::optional<assets::Vfs> retail = test::OpenRetailVfs();
        if (retail) {
            vfs_ = std::move(*retail);
        }
        return retail.has_value();
    }

    [[nodiscard]] assets::Vfs& Vfs() { return vfs_; }

private:
    [[nodiscard]] std::filesystem::path PartPath(std::string_view file) const {
        return root_ / "MENUDATA/PARTDB" / file;
    }

    void Write(std::string_view file, std::string_view bytes) const {
        const std::filesystem::path path = PartPath(file);
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << bytes;
    }

    void Mount() {
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root_);
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    }

    test::TempFolder folder_;
    std::filesystem::path root_;
    assets::Vfs vfs_;
    int count_ = 0;
};

}

TEST_CASE("a parts file reads hats, faces, torsos, legs and colours") {
    const ModDriverParts parts = Parsed(kLavaParts);

    REQUIRE(parts.hats.size() == 1);
    CHECK(parts.hats[0].model == "lava/helmet.glb");
    REQUIRE(parts.faces.size() == 1);
    CHECK(parts.faces[0].images[0] == "lava/face.png");
    CHECK(parts.faces[0].images[3] == "lava/happy.png");
    CHECK(parts.faces[0].images[1].empty());
    REQUIRE(parts.torsos.size() == 1);
    CHECK(parts.torsos[0].variant == 1);
    REQUIRE(parts.legs.size() == 1);
    CHECK(parts.legs[0].variant == 0);
    REQUIRE(parts.colours.size() == 1);
    CHECK(parts.colours[0].rgba == Rgba8{200, 60, 20, 255});
}

TEST_CASE("a broken part is skipped, and unknown keys leave the part alone") {
    const ModDriverParts parts = Parsed(
        "[[hat]]\nmodel = \"a.glb\"\n[[hat]]\nname = \"b\"\nmodel = \"b.glb\"\nset = \"x\"\n"
        "[[face]]\nname = \"c_\"\n[[face]]\nname = \"d_\"\ndefault = \"d.png\"\nsad = 3\n"
        "[[torso]]\nname = \"e\"\ntexture = \"e.png\"\nhook = \"yes\"\n"
        "[[colour]]\nname = \"f\"\nrgb = [1, 2, 300]\n[[colour]]\nname = \"g\"\nrgb = [1]\n"
        "[[shoe]]\nname = \"h\"\nhat = 3\n");

    REQUIRE(parts.hats.size() == 1);
    CHECK(parts.hats[0].name == "b");
    CHECK(parts.faces.empty());
    CHECK(parts.torsos.empty());
    CHECK(parts.colours.empty());
}

TEST_CASE("a parts file that isn't TOML is an error") {
    CHECK_FALSE(ParseModDriverParts("[[hat]\n", "PARTS.TOML").has_value());
}

TEST_CASE("parts files merge by name, a later mod's part replacing an earlier one") {
    ModFolders mods;
    mods.Add(
        "[[torso]]\nname = \"suit\"\ntexture = \"a.png\"\n[[legs]]\nname = \"x\"\n"
        "texture = \"x.png\"\n");
    mods.Add("[[torso\n");
    mods.Add(
        "[[torso]]\nname = \"SUIT\"\ntexture = \"b.png\"\n[[torso]]\nname = \"coat\"\n"
        "texture = \"c.png\"\n");

    const ModDriverParts parts = LoadModDriverParts(mods.Vfs());

    REQUIRE(parts.torsos.size() == 2);
    CHECK(parts.torsos[0].texture == "b.png");
    CHECK(parts.torsos[1].name == "coat");
    CHECK(parts.legs.size() == 1);
}

TEST_CASE("mod parts become catalogue entries, each hat with its own head part") {
    const assets::DriverPartCatalog catalog = ModPartCatalog(Parsed(kLavaParts));

    REQUIRE(catalog.hats.size() == 1);
    CHECK(catalog.hats[0].name == "lava-helmet");
    CHECK(catalog.headParts == std::vector<std::string>{"lava-helmet"});
    REQUIRE(catalog.torsos.parts.size() == 1);
    CHECK(catalog.torsos.parts[0].variant == 1);
    CHECK(catalog.faces.parts.at(0).unlock == 0);
}

TEST_CASE("a hat's model becomes a head part, and one that doesn't load is skipped") {
    ModFolders mods;
    mods.AddLava();
    ModDriverParts parts = Parsed(kLavaParts);
    parts.hats.push_back({.name = "gone", .model = "gone.glb"});

    const assets::PartLibrary heads = ModHeadParts(mods.Vfs(), parts);

    REQUIRE(heads.parts.size() == 1);
    CHECK(heads.parts[0].name == "lava-helmet");
    const Result<assets::GeometryAsset> helmet = assets::BuildPartGeometry(heads, "LAVA-HELMET", 2);
    REQUIRE(helmet.has_value());
    CHECK(helmet->materials == std::vector<std::string>{"face"});
    CHECK(helmet->vertices.size() == 3);
}

namespace {

assets::DriverPartCatalog Catalog(const assets::Vfs& vfs) {
    Result<assets::DriverPartCatalog> parts = LoadDriverParts(vfs);
    REQUIRE(parts.has_value());
    return std::move(*parts);
}

assets::TrackAssets Looks(const assets::Vfs& vfs) {
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(vfs, pool);
    Result<assets::TrackAssets> looks = LoadDriverLooks(vfs, loader, Catalog(vfs));
    REQUIRE(looks.has_value());
    return std::move(*looks);
}

}

TEST_CASE("a mod's parts dress a game driver in menus and races") {
    ModFolders mods;
    if (!mods.MountRetail()) {
        return;
    }
    mods.AddLava();
    const assets::Vfs& vfs = mods.Vfs();
    const assets::DriverPartCatalog parts = Catalog(vfs);
    const assets::DriverLook look = assets::FitLook(parts, {.faceName = "lv_",
                                                            .hatName = "lava-helmet",
                                                            .legsName = "lava-legs",
                                                            .torsoName = "lava-suit"});

    CHECK_FALSE(assets::HasMissingParts(parts, look));
    CHECK(parts.torsos.parts.at(look.torso).variant == 1);
    CHECK(BuildMinifig(vfs, parts, look, assets::DriverDetail::Menu).has_value());
    CHECK(BuildMinifig(vfs, parts, look, assets::DriverDetail::Race).has_value());
}

TEST_CASE("a mod's faces, torsos, legs and colours load with the game's driver looks") {
    ModFolders mods;
    if (!mods.MountRetail()) {
        return;
    }
    mods.AddLava();

    const assets::TrackAssets looks = Looks(mods.Vfs());

    const std::vector<std::string_view> added{"LV_DFLT", "LV_HAPPY", "LV_SUPRZ", "LAVA-SUIT",
                                              "LAVA-LEGS"};
    CHECK(std::ranges::all_of(added, [&](std::string_view name) {
        return looks.materials.contains(name) && looks.textures.contains(name);
    }));
    CHECK(looks.materials.at("LAVA_RED").diffuse == Rgba8{200, 60, 20, 255});
}

TEST_CASE("a mod's face brings every expression, so none load from the game's images") {
    ModFolders mods;
    if (!mods.MountRetail()) {
        return;
    }
    mods.AddLava();
    const assets::Vfs& vfs = mods.Vfs();

    const Result<assets::TrackAssets> faces = LoadExpressionFaces(vfs, Looks(vfs).materials, "lv_");

    REQUIRE(faces.has_value());
    CHECK(faces->materials.empty());
}
}
