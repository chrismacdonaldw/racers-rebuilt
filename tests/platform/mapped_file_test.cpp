#include "platform/mapped_file.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <string_view>
#include <utility>

#include "temp_folder.hpp"

namespace rr::platform {
namespace {

TEST_CASE("a mapped file exposes the file's bytes") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "data.bin";
    std::ofstream(path, std::ios::binary) << "LJAM";

    Result<MappedFile> file = MappedFile::Open(path);

    REQUIRE(file.has_value());
    const std::span<const std::byte> bytes = file->Bytes();
    REQUIRE(bytes.size() == 4);
    CHECK(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()) == "LJAM");
}

TEST_CASE("a mapped file keeps its bytes when moved") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "data.bin";
    std::ofstream(path, std::ios::binary) << "abc";
    Result<MappedFile> file = MappedFile::Open(path);
    REQUIRE(file.has_value());
    const std::byte* data = file->Bytes().data();

    const MappedFile moved = std::move(*file);

    CHECK(moved.Bytes().data() == data);
    CHECK(moved.Bytes().size() == 3);
}

TEST_CASE("an empty file maps to no bytes") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "empty.bin";
    std::ofstream(path, std::ios::binary).flush();

    Result<MappedFile> file = MappedFile::Open(path);

    REQUIRE(file.has_value());
    CHECK(file->Bytes().empty());
}

TEST_CASE("mapping a missing file fails with not found") {
    const test::TempFolder folder;

    Result<MappedFile> file = MappedFile::Open(folder.Path() / "missing.bin");

    REQUIRE_FALSE(file.has_value());
    CHECK(file.error().code == ErrorCode::NotFound);
}

}
}
