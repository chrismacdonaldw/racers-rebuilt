#include "app/ldraw_driver_faces.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/driver_parts.hpp"
#include "assets/game_path.hpp"
#include "assets/ldraw_prints.hpp"
#include "assets/texture_database.hpp"
#include "core/utf8_path.hpp"
#include "platform/mapped_file.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kHappy = 3;
constexpr Rgba8 kYellow{0xFA, 0xC8, 0x0A, 0xFF};
constexpr Rgba8 kRed{0xB4, 0x00, 0x00, 0xFF};

std::filesystem::path ShippedFaces() {
    return std::filesystem::path(RR_LDRAW_DIR) / "faces";
}

void WriteFace(const std::filesystem::path& folder, std::string_view name, std::string_view text) {
    std::ofstream(folder / PathFromUtf8(std::string(name)), std::ios::binary)
        .write(text.data(), static_cast<std::streamsize>(text.size()));
}

Rgba8 CentreOf(const std::vector<std::byte>& file) {
    const Result<assets::ImageAsset> image =
        assets::DecodeImage(file, assets::TextureFileKind::Bmp);
    REQUIRE(image.has_value());
    REQUIRE(image->widthPx == assets::kFaceDrawingSizePx);
    const std::size_t at =
        ((std::size_t{image->heightPx / 2} * image->widthPx) + (image->widthPx / 2)) * 4;
    return {std::to_integer<std::uint8_t>(image->rgba[at]),
            std::to_integer<std::uint8_t>(image->rgba[at + 1]),
            std::to_integer<std::uint8_t>(image->rgba[at + 2]),
            std::to_integer<std::uint8_t>(image->rgba[at + 3])};
}

TEST_CASE("a face image path names its prefix and expression") {
    const std::optional<FaceImage> face = FaceOfImagePath("MENUDATA/PARTDB/kk_Happy.BMP");

    REQUIRE(face.has_value());
    CHECK(face.transform([](const FaceImage& found) { return found.prefix; }) == "KK");
    CHECK(face.transform([](const FaceImage& found) { return found.expression; }) == kHappy);
}

TEST_CASE("only a face expression's bmp is a face image") {
    constexpr std::array kOthers{"MENUDATA/PARTDB/KK_CHST.BMP", "MENUDATA/PARTDB/KK_DFLT.PNG",
                                 "MENUDATA/PARTDB/../K_DFLT.BMP", "MENUDATA/PARTDB/_DFLT.BMP",
                                 "GAMEDATA/KK_DFLT.BMP"};

    for (const std::string_view path : kOthers) {
        CHECK_MESSAGE(!FaceOfImagePath(path).has_value(), path);
    }
}

TEST_CASE("a face file draws each expression") {
    const test::TempFolder folder;
    WriteFace(folder.Path(), "ZZ.face", "base plain FAC80A\n[HAPPY]\nfill B40000 all\n");
    LdrawDriverFaces faces(folder.Path());
    assets::LdrawLibrary library(RR_LDRAW_DIR);

    const std::optional<std::vector<std::byte>> plain =
        faces.ImageFile({.prefix = "ZZ", .expression = 0}, library);
    const std::optional<std::vector<std::byte>> happy =
        faces.ImageFile({.prefix = "ZZ", .expression = kHappy}, library);

    REQUIRE(plain.has_value());
    REQUIRE(happy.has_value());
    CHECK(plain.transform(CentreOf) == kYellow);
    CHECK(happy.transform(CentreOf) == kRed);
}

TEST_CASE("a face with no file or a broken one keeps the game's") {
    const test::TempFolder folder;
    WriteFace(folder.Path(), "ZZ.face", "base plain FAC80A\n[HAPPY]\nfill B40000\n");
    LdrawDriverFaces faces(folder.Path());
    assets::LdrawLibrary library(RR_LDRAW_DIR);

    CHECK_FALSE(faces.ImageFile({.prefix = "ZZ", .expression = 0}, library).has_value());
    CHECK_FALSE(faces.ImageFile({.prefix = "YY", .expression = 0}, library).has_value());
}

std::string ProblemsOf(const std::filesystem::path& path, assets::LdrawLibrary& library) {
    const Result<assets::FaceDrawing> drawing =
        platform::MappedFile::Open(path).and_then([](const platform::MappedFile& mapped) {
            const std::span<const std::byte> bytes = mapped.Bytes();
            return assets::ParseFaceDrawing(
                {reinterpret_cast<const char*>(bytes.data()), bytes.size()});
        });
    if (!drawing) {
        return drawing.error().message;
    }
    if (drawing->ldrawPart.empty()) {
        return {};
    }
    const Result<assets::GeometryAsset> head = library.LoadPart(drawing->ldrawPart);
    if (!head) {
        return head.error().message;
    }
    std::string unknown;
    for (const std::string& material : head->materials) {
        if (material != "16" && !assets::LdrawColourOf(material)) {
            unknown += material + " ";
        }
    }
    return unknown;
}

std::vector<std::filesystem::path> ShippedFaceFiles() {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(ShippedFaces())) {
        files.push_back(entry.path());
    }
    return files;
}

TEST_CASE("every shipped face parses and its head print loads in known colours") {
    assets::LdrawLibrary library(RR_LDRAW_DIR);
    const std::vector<std::filesystem::path> files = ShippedFaceFiles();

    REQUIRE_FALSE(files.empty());
    for (const std::filesystem::path& file : files) {
        CAPTURE(Utf8FromPath(file));
        CHECK(file.extension() == ".face");
        CHECK(ProblemsOf(file, library).empty());
    }
}

TEST_CASE("every shipped face names a game face") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::filesystem::path& file : ShippedFaceFiles()) {
        const std::string prefix = assets::ToUpperAscii(Utf8FromPath(file.stem()));
        for (std::int32_t expression = 0; expression < 6; ++expression) {
            const std::string path =
                std::format("MENUDATA/PARTDB/{}.BMP",
                            assets::ToUpperAscii(assets::FaceMaterial(prefix + "_", expression)));
            CHECK_MESSAGE(vfs->Contains(path), path);
        }
    }
}

}
}
