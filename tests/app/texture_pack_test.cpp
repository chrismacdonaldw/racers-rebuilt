#include "app/texture_pack.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/hq_textures.hpp"
#include "assets/png_image.hpp"
#include "assets/texture_database.hpp"
#include "assets/token_bytes.hpp"
#include "game_layers.hpp"
#include "mods/hook_registry.hpp"
#include "mods/mod_manifest.hpp"

namespace rr::app {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;

constexpr std::uint8_t kEntry = 0x27;
constexpr std::uint8_t kTextureKey = 0x2C;
constexpr std::string_view kBrick = "GAMEDATA/COMMON/BRICK.TGA";

Result<assets::ImageAsset> DecodeBrick(const assets::FileData& data) {
    const assets::TextureDefinition brick{.name = "BRICK",
                                          .colorKey = assets::ColorKey{.red = 255, .blue = 255}};
    return assets::DecodeTexture(data.Bytes(), brick);
}

assets::ImageAsset KeyedImage() {
    return {.widthPx = 2, .heightPx = 1, .rgba = Bytes({10, 20, 30, 255, 255, 0, 255, 255})};
}

class PackFixture {
public:
    PackFixture() {
        layers_.WriteGame(
            "GAMEDATA/COMMON/COMMON.TDB",
            test::NamedLibrary(
                kEntry, {test::NamedEntry(kEntry, "brick",
                                          Join({Bytes({kTextureKey}), Ints({255, 0, 255})}))}));
        layers_.WriteGameImage(kBrick, KeyedImage());
        layers_.WriteGameImage("MENUDATA/PLAIN.TGA", KeyedImage());
        layers_.WriteLooseImage("LEGO1.TGA", KeyedImage());
    }

    [[nodiscard]] Result<void> Dump(std::string_view id) const {
        const std::unique_ptr<assets::Vfs> vfs = layers_.Mount();
        return DumpTexturePack(*vfs, test::GameLayers::kGameLayer, Pack(id));
    }

    [[nodiscard]] std::filesystem::path Pack(std::string_view id) const {
        return layers_.Mods() / PathFromUtf8(id);
    }

    [[nodiscard]] assets::ImageAsset ReadPng(std::string_view id, std::string_view path) const {
        Result<assets::ImageAsset> image =
            assets::DecodePng(test::ReadFileBytes(Pack(id) / "files" / PathFromUtf8(path)));
        REQUIRE(image.has_value());
        return std::move(*image);
    }

    [[nodiscard]] const test::GameLayers& Layers() const { return layers_; }

private:
    test::GameLayers layers_;
};

std::string ReadText(const std::filesystem::path& path) {
    const std::vector<std::byte> bytes = test::ReadFileBytes(path);
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}

TEST_CASE_FIXTURE(PackFixture, "a keyed image dumps transparent with its neighbours' colour") {
    REQUIRE(Dump("my-pack").has_value());

    CHECK(ReadPng("my-pack", "GAMEDATA/COMMON/BRICK.PNG").rgba ==
          Bytes({10, 20, 30, 255, 10, 20, 30, 0}));
    CHECK(ReadPng("my-pack", "MENUDATA/PLAIN.PNG").rgba == KeyedImage().rgba);
    CHECK_FALSE(std::filesystem::exists(Pack("my-pack") / "files" / "LEGO1.PNG"));
}

TEST_CASE_FIXTURE(PackFixture, "a texture pack starts disabled") {
    REQUIRE(Dump("my-pack").has_value());

    const Result<mods::ModManifest> manifest =
        mods::ParseModManifest(ReadText(Pack("my-pack") / "mod.toml"), "mod.toml");

    REQUIRE(manifest.has_value());
    CHECK(manifest->id == "my-pack");
    CHECK(manifest->name == "Texture pack");
    CHECK_FALSE(manifest->isEnabledByDefault);
}

TEST_CASE_FIXTURE(PackFixture, "a texture pack refuses a bad id or an existing pack") {
    CHECK_FALSE(Dump("My Pack").has_value());
    CHECK_FALSE(std::filesystem::exists(Pack("My Pack")));

    REQUIRE(Dump("my-pack").has_value());
    std::filesystem::remove_all(Pack("my-pack") / "files");
    CHECK_FALSE(Dump("my-pack").has_value());
    CHECK_FALSE(std::filesystem::exists(Pack("my-pack") / "files"));
}

TEST_CASE_FIXTURE(PackFixture, "a dumped texture loads as the original keyed texture looks") {
    REQUIRE(Dump("my-pack").has_value());
    const std::unique_ptr<assets::Vfs> vfs = Layers().Mount();
    Result<assets::FolderLayer> pack = assets::FolderLayer::Scan(Pack("my-pack") / "files");
    REQUIRE(pack.has_value());
    vfs->Mount(std::make_unique<assets::FolderLayer>(std::move(*pack)));
    mods::HookRegistry hooks;
    settings::Registry settings;
    HqTextures textures;
    textures.DeclareSettings(settings);
    textures.Attach(hooks, settings);
    mods::ResolveThroughHooks(*vfs, hooks);

    const Result<assets::ImageAsset> original = vfs->ReadOriginal(kBrick).and_then(DecodeBrick);
    const Result<assets::ImageAsset> replaced = vfs->Read(kBrick).and_then(DecodeBrick);

    REQUIRE(original.has_value());
    REQUIRE(replaced.has_value());
    CHECK(replaced->rgba == original->enhancedRgba);
}

}
