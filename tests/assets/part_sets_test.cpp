#include "assets/part_sets.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;
using test::Text;

std::vector<std::byte> Choices() {
    return Join({Bytes({0x30}), test::ListHeader(2), Text("l300300"), Text("blue"), Text("SRRWNDW"),
                 Text("black"), Bytes({0x06})});
}

}

TEST_CASE("a part set reads its chassis and its piece and colour choices") {
    const Result<PartSet> set = ParsePartSet(
        Join({Bytes({0x2E}), test::Ints({20}), Bytes({0x31}), Text("ddchas0"), Choices()}));

    REQUIRE(set.has_value());
    CHECK(set->chassisType == 20);
    CHECK(set->chassisName == "ddchas0");
    CHECK(set->choices == std::vector<PartSetChoice>{{.piece = "l300300", .colour = "blue"},
                                                     {.piece = "SRRWNDW", .colour = "black"}});
}

TEST_CASE("a part set needs its chassis type and name once each") {
    const std::vector<std::byte> type = Join({Bytes({0x2E}), test::Ints({20})});
    const std::vector<std::byte> name = Join({Bytes({0x31}), Text("ddchas0")});

    CHECK_FALSE(ParsePartSet(Join({name, Choices()})).has_value());
    CHECK_FALSE(ParsePartSet(Join({type, Choices()})).has_value());
    CHECK_FALSE(ParsePartSet(Join({type, type, name, Choices()})).has_value());
    CHECK(ParsePartSet(Join({type, name})).has_value());
}

TEST_CASE("the colour and set lists read their names in order") {
    const Result<std::vector<std::string>> colours = ParsePieceColours(
        Join({Bytes({0x2D}), test::ListHeader(2), Text("black"), Text("biege"), Bytes({0x06})}));
    const Result<std::vector<std::string>> sets = ParsePartSetList(
        Join({Bytes({0x2F}), test::ListHeader(1), Text("dp_cset.leg"), Bytes({0x06})}));

    REQUIRE(colours.has_value());
    REQUIRE(sets.has_value());
    CHECK(*colours == std::vector<std::string>{"black", "biege"});
    CHECK(*sets == std::vector<std::string>{"dp_cset.leg"});
    CHECK_FALSE(
        ParsePieceColours(Join({Bytes({0x2F}), test::ListHeader(0), Bytes({0x06})})).has_value());
}

}
