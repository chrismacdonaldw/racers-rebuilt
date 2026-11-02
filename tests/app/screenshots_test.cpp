#include "app/screenshots.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kRgbaBytes = 16;

void Touch(const std::filesystem::path& path) {
    const std::ofstream file(path);
}

}

TEST_CASE("the first screenshot is LEGO0.bmp") {
    const test::TempFolder folder;

    CHECK(FreeScreenshotPath(folder.Path()) == folder.Path() / "LEGO0.bmp");
}

TEST_CASE("a screenshot takes the first free number, filling gaps") {
    const test::TempFolder folder;
    Touch(folder.Path() / "LEGO0.bmp");
    Touch(folder.Path() / "LEGO2.bmp");

    CHECK(FreeScreenshotPath(folder.Path()) == folder.Path() / "LEGO1.bmp");
}

TEST_CASE("saving a screenshot writes a BMP and moves on to the next number") {
    const test::TempFolder folder;
    const assets::ImageAsset image{
        .widthPx = 2, .heightPx = 2, .rgba = std::vector(kRgbaBytes, std::byte{0x80})};

    const Result<std::filesystem::path> saved = SaveScreenshot(image, folder.Path());

    REQUIRE(saved.has_value());
    CHECK(*saved == folder.Path() / "LEGO0.bmp");
    CHECK(std::filesystem::file_size(*saved) > kRgbaBytes);
    CHECK(FreeScreenshotPath(folder.Path()) == folder.Path() / "LEGO1.bmp");
}

}
