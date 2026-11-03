#include "app/hq_textures.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "app/ui_text/ui_pixels.hpp"
#include "assets/folder_layer.hpp"
#include "image_files.hpp"
#include "mods/hook_registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

class HqFixture {
public:
    HqFixture() {
        test::WriteTga(folder_.Path() / "menudata" / "logo.tga",
                       test::SolidImage(1, 1, std::byte{0xFF}));
        test::WriteTga(folder_.Path() / "menudata" / "plain.tga",
                       test::SolidImage(1, 1, std::byte{0xFF}));
        test::WritePng(folder_.Path() / "menudata" / "logo.png",
                       test::SolidImage(4, 2, std::byte{0x40}));
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder_.Path());
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
        textures_.DeclareSettings(settings_);
        mods::ResolveThroughHooks(vfs_, hooks_);
    }

    void Attach(bool isEnabled) {
        settings_.Set(kHqTextures, isEnabled);
        textures_.Attach(hooks_, settings_);
    }

    void Turn(bool isEnabled) { settings_.Set(kHqTextures, isEnabled); }

    [[nodiscard]] UiPixels Read(std::string_view path) const {
        Result<UiPixels> pixels = ReadUiPixels(vfs_, path, assets::TextureFileKind::Tga);
        REQUIRE(pixels.has_value());
        return std::move(*pixels);
    }

private:
    test::TempFolder folder_;
    assets::Vfs vfs_;
    mods::HookRegistry hooks_;
    settings::Registry settings_;
    HqTextures textures_;
};

TEST_CASE("only bmp and tga images have a png sibling") {
    CHECK(PngSibling("GAMEDATA/COMMON/BRICK.TGA") == "GAMEDATA/COMMON/BRICK.PNG");
    CHECK(PngSibling("MENUDATA/LOGO.BMP") == "MENUDATA/LOGO.PNG");
    CHECK_FALSE(PngSibling("MENUDATA/LOGO.PNG").has_value());
    CHECK_FALSE(PngSibling("GAMEDATA/COMMON/BRICK.TDB").has_value());
}

TEST_CASE_FIXTURE(HqFixture, "a png replaces its image and ui layout keeps the original size") {
    Attach(true);

    const UiPixels logo = Read("MENUDATA/LOGO.TGA");
    const UiPixels plain = Read("MENUDATA/PLAIN.TGA");

    CHECK(logo.pixels.widthPx == 4);
    CHECK(logo.pixels.rgba[3] == std::byte{0x40});
    REQUIRE(logo.original.has_value());
    CHECK(logo.Layout().widthPx == 1);
    CHECK(logo.Layout().heightPx == 1);
    CHECK(plain.pixels.widthPx == 1);
    CHECK_FALSE(plain.original.has_value());
}

TEST_CASE_FIXTURE(HqFixture, "turning hq textures off keeps the game images") {
    Attach(false);

    const UiPixels logo = Read("MENUDATA/LOGO.TGA");

    CHECK(logo.pixels.widthPx == 1);
    CHECK_FALSE(logo.original.has_value());
}

TEST_CASE_FIXTURE(HqFixture, "the toggle applies to reads after it changes") {
    Attach(false);
    Turn(true);

    CHECK(Read("MENUDATA/LOGO.TGA").pixels.widthPx == 4);

    Turn(false);

    CHECK(Read("MENUDATA/LOGO.TGA").pixels.widthPx == 1);
}

}
}
