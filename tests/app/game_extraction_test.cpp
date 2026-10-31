#include "app/game_extraction.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string_view>

#include "core/utf8_path.hpp"
#include "install_fixture.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

void WriteFile(const std::filesystem::path& path, std::string_view contents = "x") {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << contents;
}

std::filesystem::path WriteRetailInstall(const std::filesystem::path& folder) {
    const std::filesystem::path install = folder / "install";
    test::WriteInstall(install);
    WriteFile(install / "Circuit1.tun");
    WriteFile(install / "introcmp.avi");
    WriteFile(install / "LEGORacers.exe");
    WriteFile(install / "LEGO0.BMP");
    WriteFile(install / "Save" / "0" / "SAVE.DAT");
    return install;
}

TEST_CASE("extraction copies the jam, music and videos") {
    const test::TempFolder folder;
    const std::filesystem::path copy = folder.Path() / "game";

    REQUIRE(ExtractGameFiles(WriteRetailInstall(folder.Path()), copy).has_value());

    CHECK(std::filesystem::exists(copy / "LEGO.JAM"));
    CHECK(std::filesystem::exists(copy / "Circuit1.tun"));
    CHECK(std::filesystem::exists(copy / "introcmp.avi"));
}

TEST_CASE("a fresh copy opens from its own folder") {
    const test::TempFolder folder;
    const std::filesystem::path copy = folder.Path() / "game";
    REQUIRE(ExtractGameFiles(WriteRetailInstall(folder.Path()), copy).has_value());

    const std::optional<GameInstall> opened = OpenExtractedCopy(copy, std::nullopt);

    CHECK((opened && opened->folder == copy));
}

TEST_CASE("extraction skips the exe, screenshots and saves") {
    const test::TempFolder folder;
    const std::filesystem::path copy = folder.Path() / "game";

    REQUIRE(ExtractGameFiles(WriteRetailInstall(folder.Path()), copy).has_value());

    CHECK_FALSE(std::filesystem::exists(copy / "LEGORacers.exe"));
    CHECK_FALSE(std::filesystem::exists(copy / "LEGO0.BMP"));
    CHECK_FALSE(std::filesystem::exists(copy / "Save"));
}

TEST_CASE("a folder without a stamp isn't an extracted copy") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path());

    CHECK_FALSE(OpenExtractedCopy(folder.Path(), std::nullopt).has_value());
}

TEST_CASE("a copy from another extraction version is extracted again") {
    const test::TempFolder folder;
    const std::filesystem::path install = folder.Path() / "install";
    const std::filesystem::path copy = folder.Path() / "game";
    test::WriteInstall(install);
    REQUIRE(ExtractGameFiles(install, copy).has_value());

    WriteFile(copy / kExtractionStamp,
              std::format("{}\n{}\n", kExtractionVersion + 1, Utf8FromPath(install)));

    CHECK_FALSE(OpenExtractedCopy(copy, std::nullopt).has_value());
}

TEST_CASE("a copy is kept while game.path names its source or the copy itself") {
    const test::TempFolder folder;
    const std::filesystem::path install = folder.Path() / "install";
    const std::filesystem::path copy = folder.Path() / "game";
    test::WriteInstall(install);
    REQUIRE(ExtractGameFiles(install, copy).has_value());

    CHECK(OpenExtractedCopy(copy, install).has_value());
    CHECK(OpenExtractedCopy(copy, copy).has_value());
    CHECK_FALSE(OpenExtractedCopy(copy, folder.Path() / "other install").has_value());
}

TEST_CASE("extracting again replaces the older copy and a leftover partial copy") {
    const test::TempFolder folder;
    const std::filesystem::path oldInstall = folder.Path() / "old";
    const std::filesystem::path newInstall = folder.Path() / "new";
    const std::filesystem::path copy = folder.Path() / "game";
    test::WriteInstall(oldInstall);
    WriteFile(oldInstall / "old.tun");
    test::WriteInstall(newInstall);
    WriteFile(newInstall / "new.tun");
    REQUIRE(ExtractGameFiles(oldInstall, copy).has_value());
    WriteFile(folder.Path() / "game.partial" / "stale.tun");

    REQUIRE(ExtractGameFiles(newInstall, copy).has_value());

    CHECK(std::filesystem::exists(copy / "new.tun"));
    CHECK_FALSE(std::filesystem::exists(copy / "old.tun"));
    CHECK_FALSE(std::filesystem::exists(folder.Path() / "game.partial"));
    CHECK(OpenExtractedCopy(copy, newInstall).has_value());
}

TEST_CASE("a failed extraction leaves no copy behind") {
    const test::TempFolder folder;
    const std::filesystem::path copy = folder.Path() / "game";

    const Result<void> extracted = ExtractGameFiles(folder.Path() / "missing", copy);

    CHECK_FALSE(extracted.has_value());
    CHECK_FALSE(std::filesystem::exists(copy));
    CHECK_FALSE(std::filesystem::exists(folder.Path() / "game.partial"));
}

}
}
