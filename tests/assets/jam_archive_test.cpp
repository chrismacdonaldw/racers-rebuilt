#include "assets/jam_archive.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <fstream>
#include <optional>
#include <ranges>

#include "assets/jam_builder.hpp"
#include "game_files.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kRetailDirectoryCount = 63;
constexpr std::size_t kRetailFileCount = 5444;
constexpr std::chrono::milliseconds kIndexBudget{20};

void WriteFile(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream(path, std::ios::binary) << test::AsText(bytes);
}

TEST_CASE("an opened archive reads files from its mapping") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "TEST.JAM";
    WriteFile(path, test::JamBuilder::Build({{.path = "GAMEDATA/A.TXT", .contents = "alpha"}}));

    Result<JamArchive> archive = JamArchive::Open(path);

    REQUIRE(archive.has_value());
    const std::optional<EntryInfo> entry = archive->Index().Find("GAMEDATA/A.TXT");
    REQUIRE(entry.has_value());
    Result<FileData> data = archive->Read(*entry);
    REQUIRE(data.has_value());
    CHECK(test::AsText(data->Bytes()) == "alpha");
}

TEST_CASE("opening a corrupt archive names the file") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "BROKEN.JAM";
    std::ofstream(path, std::ios::binary) << "LJAM\xFF\xFF";

    Result<JamArchive> archive = JamArchive::Open(path);

    REQUIRE_FALSE(archive.has_value());
    CHECK(archive.error().code == ErrorCode::InvalidData);
    CHECK(archive.error().message.find("BROKEN.JAM") != std::string::npos);
}

TEST_CASE("opening a missing archive fails with not found") {
    const test::TempFolder folder;

    CHECK(JamArchive::Open(folder.Path() / "LEGO.JAM").error().code == ErrorCode::NotFound);
}

std::optional<std::filesystem::path> RetailJamPath() {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return std::nullopt;
    }
    return *gameFolder / "LEGO.JAM";
}

TEST_CASE("the retail LEGO.JAM is indexed within budget") {
    const std::optional<std::filesystem::path> path = RetailJamPath();
    if (!path) {
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    const bool isOpened = JamArchive::Open(*path).has_value();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(isOpened);
    CHECK(elapsed < kIndexBudget);
}

TEST_CASE("the retail LEGO.JAM indexes every directory and file") {
    const std::optional<std::filesystem::path> path = RetailJamPath();
    if (!path) {
        return;
    }

    Result<JamArchive> archive = JamArchive::Open(*path);

    REQUIRE(archive.has_value());
    const std::span<const IndexEntry> entries = archive->Index().Entries();
    const auto isDirectory = [](const IndexEntry& e) {
        return e.info.kind == EntryKind::Directory;
    };
    CHECK(static_cast<std::size_t>(std::ranges::count_if(entries, isDirectory)) ==
          kRetailDirectoryCount);
    CHECK(entries.size() - kRetailDirectoryCount == kRetailFileCount);
}

}
}
