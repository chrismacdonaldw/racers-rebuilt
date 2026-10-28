#include "assets/jam.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/jam_builder.hpp"

namespace rr::assets {
namespace {

using test::JamBuilder;

std::string_view ContentsOf(const JamIndex& index, std::span<const std::byte> bytes,
                            std::string_view path) {
    const std::optional<EntryInfo> entry = index.paths.Find(path);
    if (!entry || entry->kind != EntryKind::File) {
        FAIL("no file at ", path);
        return {};
    }
    const JamFileRange range = index.files[entry->slot];
    return test::AsText(bytes.subspan(range.offset, range.sizeBytes));
}

ErrorCode ParseFailure(std::span<const std::byte> bytes) {
    Result<JamIndex> index = ParseJam(bytes);
    REQUIRE_FALSE(index.has_value());
    return index.error().code;
}

std::optional<EntryKind> KindAt(const JamIndex& index, std::string_view path) {
    const std::optional<EntryInfo> entry = index.paths.Find(path);
    return entry ? std::optional(entry->kind) : std::nullopt;
}

std::vector<std::byte> BuildTree() {
    return JamBuilder::Build({{.path = "README.TXT", .contents = "root"},
                              {.path = "GAMEDATA/COMMON/RRJMW.GDB", .contents = "geometry"},
                              {.path = "GAMEDATA/COMMON/CURSEHIT.PCM", .contents = "pcm"}},
                             {"MENUDATA"});
}

TEST_CASE("indexes files in nested directories") {
    const std::vector<std::byte> bytes = BuildTree();

    Result<JamIndex> index = ParseJam(bytes);

    REQUIRE(index.has_value());
    CHECK(ContentsOf(*index, bytes, "README.TXT") == "root");
    CHECK(ContentsOf(*index, bytes, "GAMEDATA/COMMON/RRJMW.GDB") == "geometry");
    CHECK(KindAt(*index, "GAMEDATA/COMMON") == EntryKind::Directory);
    CHECK_FALSE(KindAt(*index, "GAMEDATA/RRJMW.GDB").has_value());
}

TEST_CASE("reads names that fill all 12 bytes") {
    const std::vector<std::byte> bytes = BuildTree();

    Result<JamIndex> index = ParseJam(bytes);

    REQUIRE(index.has_value());
    CHECK(ContentsOf(*index, bytes, "GAMEDATA/COMMON/CURSEHIT.PCM") == "pcm");
}

TEST_CASE("keeps empty directories") {
    const std::vector<std::byte> bytes = BuildTree();

    Result<JamIndex> index = ParseJam(bytes);

    REQUIRE(index.has_value());
    CHECK(KindAt(*index, "MENUDATA") == EntryKind::Directory);
    CHECK(index->paths.Children("MENUDATA").empty());
}

TEST_CASE("lists only the immediate children of a directory") {
    const std::vector<std::byte> bytes = BuildTree();

    Result<JamIndex> index = ParseJam(bytes);

    REQUIRE(index.has_value());
    const std::span<const IndexEntry> root = index->paths.Children("");
    REQUIRE(root.size() == 3);
    CHECK(root[0].Name() == "GAMEDATA");
    CHECK(root[1].Name() == "MENUDATA");
    CHECK(root[2].Name() == "README.TXT");
}

TEST_CASE("rejects data without the LJAM signature") {
    std::vector<std::byte> bytes = BuildTree();
    bytes[0] = std::byte{'X'};

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
    CHECK(ParseFailure({}) == ErrorCode::InvalidData);
}

TEST_CASE("rejects a file whose data runs past the end") {
    std::vector<std::byte> bytes = BuildTree();
    test::PatchU32(bytes, test::kJamRootFilesOffset + 16, 1'000'000);

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects a file offset that overflows when added to its size") {
    std::vector<std::byte> bytes = BuildTree();
    test::PatchU32(bytes, test::kJamRootFilesOffset + 12, 0xFFFF'FFFF);

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects a JAM directory entry that points past the end") {
    std::vector<std::byte> bytes = BuildTree();
    const std::size_t firstDirectory = test::kJamRootFilesOffset + test::kJamFileRecordBytes + 4;
    test::PatchU32(bytes, firstDirectory + test::kJamRecordValueOffset, 0x7FFF'0000);

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects a directory that points back at the root") {
    std::vector<std::byte> bytes = BuildTree();
    const std::size_t firstDirectory = test::kJamRootFilesOffset + test::kJamFileRecordBytes + 4;
    test::PatchU32(bytes, firstDirectory + test::kJamRecordValueOffset, 4);

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects an entry count larger than the archive") {
    std::vector<std::byte> bytes = BuildTree();
    test::PatchU32(bytes, 4, 0x1000'0000);

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects an archive cut off inside the directory tree") {
    std::vector<std::byte> bytes = BuildTree();
    bytes.resize(40);

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects two entries with the same name in one directory") {
    const std::vector<std::byte> bytes =
        JamBuilder::Build({{.path = "A.TXT", .contents = ""}, {.path = "a.txt", .contents = ""}});

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects a directory named like a drive") {
    std::vector<std::byte> bytes = JamBuilder::Build({}, {"CX"});
    bytes[test::kJamRootFilesOffset + 4 + 1] = std::byte{':'};

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

TEST_CASE("rejects a name with a path separator") {
    std::vector<std::byte> bytes = JamBuilder::Build({{.path = "AXB.TXT", .contents = ""}});
    bytes[test::kJamRootFilesOffset + 1] = std::byte{'/'};

    CHECK(ParseFailure(bytes) == ErrorCode::InvalidData);
}

}
}
