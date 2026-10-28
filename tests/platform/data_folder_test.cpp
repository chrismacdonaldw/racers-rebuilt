#include "platform/data_folder.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "temp_folder.hpp"

namespace rr::platform {
namespace {

TEST_CASE("a folder with portable.txt is portable") {
    const test::TempFolder folder;

    CHECK_FALSE(IsPortable(folder.Path()));
    std::ofstream(folder.Path() / kPortableMarker).put('\n');
    CHECK(IsPortable(folder.Path()));
}

TEST_CASE("a folder named portable.txt doesn't make the install portable") {
    const test::TempFolder folder;
    std::filesystem::create_directories(folder.Path() / kPortableMarker);

    CHECK_FALSE(IsPortable(folder.Path()));
}

TEST_CASE("a missing exe folder isn't portable") {
    const test::TempFolder folder;

    CHECK_FALSE(IsPortable(folder.Path() / "missing"));
}

}
}
