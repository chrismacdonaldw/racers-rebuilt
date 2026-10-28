#include "assets/vfs.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

#include "assets/folder_layer.hpp"
#include "assets/jam_archive.hpp"
#include "assets/jam_builder.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

void WriteText(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

class VfsFixture {
public:
    VfsFixture() {
        const std::filesystem::path jamPath = folder_.Path() / "LEGO.JAM";
        WriteText(jamPath, test::AsText(test::JamBuilder::Build(
                               {{.path = "GAMEDATA/A.TXT", .contents = "jam a"},
                                {.path = "GAMEDATA/B.TXT", .contents = "jam b"}})));
        WriteText(Loose() / "gamedata" / "a.txt", "loose a");
        WriteText(Loose() / "gamedata" / "c.txt", "loose c");

        Result<JamArchive> archive = JamArchive::Open(jamPath);
        REQUIRE(archive.has_value());
        vfs_.Mount(std::make_unique<JamArchive>(std::move(*archive)));
    }

    void MountLoose() {
        Result<FolderLayer> loose = FolderLayer::Scan(Loose());
        REQUIRE(loose.has_value());
        vfs_.Mount(std::make_unique<FolderLayer>(std::move(*loose)));
    }

    [[nodiscard]] std::filesystem::path Loose() const { return folder_.Path() / "loose"; }

    [[nodiscard]] std::string ReadText(std::string_view path) const {
        Result<FileData> data = vfs_.Read(path);
        REQUIRE(data.has_value());
        return std::string(test::AsText(data->Bytes()));
    }

protected:
    test::TempFolder folder_;
    Vfs vfs_;
};

TEST_CASE_FIXTURE(VfsFixture, "a loose-folder layer overrides a JAM file of the same path") {
    CHECK(ReadText("GAMEDATA/A.TXT") == "jam a");

    MountLoose();

    CHECK(ReadText("GAMEDATA/A.TXT") == "loose a");
    CHECK(ReadText("GAMEDATA/B.TXT") == "jam b");
    CHECK(ReadText("GAMEDATA/C.TXT") == "loose c");
}

TEST_CASE_FIXTURE(VfsFixture, "lookups ignore case and separator style") {
    CHECK(ReadText("gamedata\\b.txt") == "jam b");
}

TEST_CASE_FIXTURE(VfsFixture, "a listing merges layers and names each entry once") {
    MountLoose();

    Result<std::vector<VfsEntry>> listing = vfs_.List("GAMEDATA");

    REQUIRE(listing.has_value());
    REQUIRE(listing->size() == 3);
    CHECK((*listing)[0].name == "A.TXT");
    CHECK((*listing)[1].name == "B.TXT");
    CHECK((*listing)[2].name == "C.TXT");
    CHECK((*listing)[2].kind == EntryKind::File);
}

TEST_CASE_FIXTURE(VfsFixture, "missing files and directories fail with not found") {
    CHECK(vfs_.Read("GAMEDATA/MISSING.TXT").error().code == ErrorCode::NotFound);
    CHECK(vfs_.Read("GAMEDATA").error().code == ErrorCode::NotFound);
    CHECK(vfs_.List("MENUDATA").error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(VfsFixture, "an invalid path is rejected before any layer is searched") {
    CHECK(vfs_.Read("GAMEDATA/../A.TXT").error().code == ErrorCode::InvalidData);
}

TEST_CASE_FIXTURE(VfsFixture, "a folder layer skips names that can't be game paths") {
    WriteText(Loose() / u8"café" / "x.txt", "skipped");

    Result<FolderLayer> loose = FolderLayer::Scan(Loose());

    REQUIRE(loose.has_value());
    CHECK(loose->Index().Find("GAMEDATA/C.TXT").has_value());
    CHECK(loose->Index().Children("").size() == 1);
}

TEST_CASE("scanning a missing folder fails with not found") {
    const test::TempFolder folder;

    CHECK(FolderLayer::Scan(folder.Path() / "missing").error().code == ErrorCode::NotFound);
}

}
}
