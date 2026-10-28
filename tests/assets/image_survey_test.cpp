#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "archive_survey.hpp"
#include "assets/bmp_image.hpp"
#include "assets/folder_layer.hpp"
#include "assets/jam_archive.hpp"
#include "assets/texture_database.hpp"
#include "assets/tga_image.hpp"
#include "game_files.hpp"

namespace rr::assets {
namespace {

struct RetailFormat {
    std::string_view extension;
    std::size_t fileCount = 0;
    Result<void> (*check)(std::span<const std::byte> bytes) = nullptr;
};

Result<void> DecodesAsBmp(std::span<const std::byte> bytes) {
    return DecodeBmp(bytes).transform([](const ImageAsset&) {});
}

Result<void> DecodesAsTga(std::span<const std::byte> bytes) {
    return DecodeTga(bytes).transform([](const ImageAsset&) {});
}

Result<void> ParsesAsTdb(std::span<const std::byte> bytes) {
    return ParseTextureDatabase(bytes).transform([](const auto&) {});
}

constexpr std::array kJamFormats{
    RetailFormat{.extension = "BMP", .fileCount = 1616, .check = DecodesAsBmp},
    RetailFormat{.extension = "TGA", .fileCount = 9, .check = DecodesAsTga},
    RetailFormat{.extension = "TDB", .fileCount = 162, .check = ParsesAsTdb},
};

constexpr std::array kLooseFormats{
    RetailFormat{.extension = "BMP", .fileCount = 31, .check = DecodesAsBmp},
};

void CheckRetailFiles(const VfsLayer& layer, std::span<const RetailFormat> formats) {
    for (const RetailFormat& format : formats) {
        CAPTURE(format.extension);
        const test::Survey survey = test::SurveyLayer(
            layer, [&](std::string_view name) { return test::Extension(name) == format.extension; },
            format.check);

        CHECK(survey.fileCount == format.fileCount);
        for (const std::string& failure : survey.failures) {
            FAIL_CHECK(failure);
        }
    }
}

TEST_CASE("every image and texture database in the retail game decodes") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }

    SUBCASE("inside LEGO.JAM") {
        const Result<JamArchive> archive = JamArchive::Open(*gameFolder / "LEGO.JAM");
        REQUIRE(archive.has_value());
        CheckRetailFiles(*archive, kJamFormats);
    }
    SUBCASE("loose in the install folder") {
        const Result<FolderLayer> folder = FolderLayer::Scan(*gameFolder, ScanDepth::TopLevelFiles);
        REQUIRE(folder.has_value());
        CheckRetailFiles(*folder, kLooseFormats);
    }
}

}
}
