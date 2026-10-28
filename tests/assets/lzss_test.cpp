#include "assets/lzss.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;

std::vector<std::byte> Text(std::string_view text) {
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

Result<std::vector<std::byte>> Decode(const std::vector<std::byte>& packed, std::size_t size) {
    std::vector<std::byte> unpacked(size);
    return DecodeLzss(packed, unpacked).transform([&] { return unpacked; });
}

TEST_CASE("literals copy through until the terminator") {
    const Result<std::vector<std::byte>> unpacked =
        Decode(Bytes({'A', 0x20, 'B', 'C', 0x00, 0x00}), 3);
    REQUIRE(unpacked.has_value());
    CHECK(*unpacked == Text("ABC"));
}

TEST_CASE("a short match may overlap the bytes it is writing") {
    const Result<std::vector<std::byte>> unpacked =
        Decode(Bytes({'A', 0x60, 'B', 0x0C, 0x02, 0x00, 0x00}), 8);
    REQUIRE(unpacked.has_value());
    CHECK(*unpacked == Text("ABABABAB"));
}

TEST_CASE("a long match takes its length from an extra byte") {
    const Result<std::vector<std::byte>> unpacked =
        Decode(Bytes({'A', 0xC0, 0x00, 0x01, 0x02, 0x00, 0x00}), 21);
    REQUIRE(unpacked.has_value());
    CHECK(*unpacked == Text("AAAAAAAAAAAAAAAAAAAAA"));
}

TEST_CASE("a match reaches back up to 4095 bytes") {
    const std::vector<std::byte> longestRun = Bytes({0x00, 0x01, 0xFF});
    const std::vector<std::byte> sevenRuns = test::Join(
        {longestRun, longestRun, longestRun, longestRun, longestRun, longestRun, longestRun});
    const std::vector<std::byte> packed = test::Join({
        Bytes({'B', 0x7F, 'A'}),
        sevenRuns,
        Bytes({0xFF}),
        sevenRuns,
        Bytes({0x00, 0x01, 0xFD, 0xC0, 0xFF, 0xFF, 0x00, 0x00}),
    });
    const Result<std::vector<std::byte>> unpacked = Decode(packed, 4098);
    REQUIRE(unpacked.has_value());
    CHECK(unpacked->at(4094) == std::byte{'A'});
    CHECK(unpacked->at(4095) == std::byte{'B'});
}

TEST_CASE("a new flag byte follows every eight items") {
    const Result<std::vector<std::byte>> unpacked =
        Decode(Bytes({'A', 0x00, 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 0x80, 0x00, 0x00}), 9);
    REQUIRE(unpacked.has_value());
    CHECK(*unpacked == Text("ABCDEFGHI"));
}

TEST_CASE("malformed streams fail") {
    SUBCASE("a match reaching before the start") {
        CHECK_FALSE(Decode(Bytes({'A', 0x80, 0x0F, 0x02, 0x00, 0x00}), 4).has_value());
    }
    SUBCASE("more bytes than the output holds") {
        CHECK_FALSE(Decode(Bytes({'A', 0x20, 'B', 'C', 0x00, 0x00}), 2).has_value());
    }
    SUBCASE("a terminator before the output is full") {
        CHECK_FALSE(Decode(Bytes({'A', 0x20, 'B', 'C', 0x00, 0x00}), 4).has_value());
    }
    SUBCASE("no terminator") {
        CHECK_FALSE(Decode(Bytes({'A', 0x00, 'B', 'C'}), 3).has_value());
    }
}

}
}
