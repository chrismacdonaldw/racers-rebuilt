#include "app/ldraw_driver_prints.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "app/hq_textures.hpp"
#include "assets/folder_layer.hpp"
#include "assets/texture_database.hpp"
#include "image_files.hpp"
#include "mods/hook_registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kTorsoPath = "MENUDATA/PARTDB/IN_CHST.BMP";
constexpr std::string_view kFacePath = "MENUDATA/PARTDB/KK_HAPPY.BMP";
constexpr std::uint32_t kTorsoSizePx = 8;

void PutU32(std::vector<std::byte>& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFF);
    }
}

std::vector<std::byte> GreyBmp(std::uint32_t sizePx) {
    constexpr std::size_t kHeaderBytes = 54;
    const std::size_t strideBytes = ((std::size_t{sizePx} * 3) + 3) / 4 * 4;
    std::vector<std::byte> bytes(kHeaderBytes + (strideBytes * sizePx), std::byte{0x80});
    std::fill_n(bytes.begin(), kHeaderBytes, std::byte{0});
    bytes[0] = std::byte{'B'};
    bytes[1] = std::byte{'M'};
    PutU32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
    PutU32(bytes, 10, kHeaderBytes);
    PutU32(bytes, 14, 40);
    PutU32(bytes, 18, sizePx);
    PutU32(bytes, 22, sizePx);
    bytes[26] = std::byte{1};
    bytes[28] = std::byte{24};
    return bytes;
}

class PrintsFixture {
public:
    PrintsFixture() {
        WriteImage(game_.Path(), "in_chst.bmp", kTorsoSizePx);
        WriteImage(game_.Path(), "kk_happy.bmp", kTorsoSizePx);
        test::WritePng(game_.Path() / "menudata" / "partdb" / "in_chst.png",
                       test::SolidImage(4, 4, std::byte{0xFF}));
        Mount(game_.Path());
        hq_.DeclareSettings(settings_);
        prints_.DeclareSettings(settings_);
        mods::ResolveThroughHooks(vfs_, hooks_);
        hq_.Attach(hooks_, settings_);
        prints_.Attach(hooks_, settings_);
    }

    void Turn(bool arePrintsOn, bool isHqOn) {
        settings_.Set(kLdrawPrintsSetting, arePrintsOn);
        settings_.Set(kHqTextures, isHqOn);
    }

    void MountModImages(std::uint32_t sizePx) {
        WriteImage(mod_.Path(), "in_chst.bmp", sizePx);
        WriteImage(mod_.Path(), "kk_happy.bmp", sizePx);
        Mount(mod_.Path());
    }

    [[nodiscard]] std::uint32_t TorsoWidthPx() const { return WidthPx(kTorsoPath); }
    [[nodiscard]] std::uint32_t FaceWidthPx() const { return WidthPx(kFacePath); }

private:
    [[nodiscard]] std::uint32_t WidthPx(std::string_view path) const {
        Result<assets::ImageAsset> image =
            vfs_.Read(path).and_then([](const assets::FileData& file) {
                return assets::DecodeImage(file.Bytes(), assets::TextureFileKind::Bmp);
            });
        REQUIRE(image.has_value());
        return image->widthPx;
    }

    static void WriteImage(const std::filesystem::path& root, std::string_view name,
                           std::uint32_t sizePx) {
        const std::filesystem::path image = root / "menudata" / "partdb" / name;
        std::filesystem::create_directories(image.parent_path());
        const std::vector<std::byte> bmp = GreyBmp(sizePx);
        std::ofstream(image, std::ios::binary)
            .write(reinterpret_cast<const char*>(bmp.data()), std::ssize(bmp));
    }

    void Mount(const std::filesystem::path& root) {
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root);
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    }

    test::TempFolder game_;
    test::TempFolder mod_;
    assets::Vfs vfs_;
    mods::HookRegistry hooks_;
    settings::Registry settings_;
    HqTextures hq_;
    LdrawDriverPrints prints_{RR_LDRAW_DIR};
};

TEST_CASE("only a mapped torso's bmp has a print") {
    CHECK(PrintOfImagePath(kTorsoPath).transform([](const assets::LdrawPrint& print) {
        return print.torso;
    }) == "IN_CHST");
    CHECK_FALSE(PrintOfImagePath("MENUDATA/PARTDB/RR_CHST.BMP").has_value());
    CHECK_FALSE(PrintOfImagePath("MENUDATA/PARTDB/IN_CHST.PNG").has_value());
    CHECK_FALSE(PrintOfImagePath("GAMEDATA/IN_CHST.BMP").has_value());
}

TEST_CASE_FIXTURE(PrintsFixture, "a mapped torso reads as its print only while prints are on") {
    Turn(false, false);
    CHECK(TorsoWidthPx() == kTorsoSizePx);

    Turn(true, false);
    CHECK(TorsoWidthPx() == kTorsoPrintSizePx);

    Turn(false, false);
    CHECK(TorsoWidthPx() == kTorsoSizePx);
}

TEST_CASE_FIXTURE(PrintsFixture, "a drawn face reads as its LDraw face only while prints are on") {
    Turn(false, false);
    CHECK(FaceWidthPx() == kTorsoSizePx);

    Turn(true, false);
    CHECK(FaceWidthPx() == assets::kFaceDrawingSizePx);
}

TEST_CASE_FIXTURE(PrintsFixture, "a mod's own torso and face images win over their prints") {
    Turn(true, false);
    REQUIRE(TorsoWidthPx() == kTorsoPrintSizePx);
    REQUIRE(FaceWidthPx() == assets::kFaceDrawingSizePx);

    MountModImages(16);

    CHECK(TorsoWidthPx() == 16);
    CHECK(FaceWidthPx() == 16);
}

TEST_CASE_FIXTURE(PrintsFixture, "a high quality torso wins over its print") {
    Turn(true, true);

    CHECK(TorsoWidthPx() == 4);
}

}
}
