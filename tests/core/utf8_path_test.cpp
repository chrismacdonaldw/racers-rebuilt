#include "core/utf8_path.hpp"

#include <doctest/doctest.h>

#include <string>

namespace rr {
namespace {

TEST_CASE("UTF-8 text survives a round trip through a path") {
    std::string utf8 = "C:/Users/Ren\xC3\xA9/racers-rebuilt";

    const std::filesystem::path path = PathFromUtf8(utf8);

    CHECK(path.u8string() == u8"C:/Users/René/racers-rebuilt");
    CHECK(Utf8FromPath(path) == utf8);
}

}
}
