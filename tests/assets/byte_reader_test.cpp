#include "assets/byte_reader.hpp"

#include <doctest/doctest.h>

#include <array>

namespace rr::assets {
namespace {

constexpr std::array kBytes{std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12},
                            std::byte{0xFF}};

TEST_CASE("reads little-endian 32-bit values") {
    ByteReader reader(kBytes, 0);

    CHECK(reader.ReadU32() == 0x1234'5678U);
    CHECK(reader.Position() == 4);
    CHECK(reader.Remaining() == 1);
}

TEST_CASE("a read past the end fails and keeps the position") {
    ByteReader reader(kBytes, 2);

    CHECK(reader.ReadU32().error().code == ErrorCode::InvalidData);
    CHECK(reader.Position() == 2);
    CHECK(reader.ReadBytes(3).has_value());
}

TEST_CASE("a reader starting past the end has nothing left") {
    ByteReader reader(kBytes, 100);

    CHECK(reader.Remaining() == 0);
    CHECK_FALSE(reader.ReadBytes(1).has_value());
}

}
}
