#include "assets/ldraw.hpp"

#include <doctest/doctest.h>

#include <string_view>

namespace rr::assets {
namespace {

constexpr std::string_view kBrick =
    "0 Brick  2 x  4\r\n"
    "0 Name: 3001.dat\r\n"
    "0 !LICENSE Licensed under CC BY 4.0 : see CAreadme.txt\r\n"
    "0 BFC CERTIFY CCW\r\n"
    "1 16 0 0 0 1 0 0 0 1 0 0 0 1 s\\3001s01.dat\r\n"
    "2 24 -40 0 -20 40 0 -20\r\n"
    "3 4 0 0 0 1 0 0 0 1 0\r\n"
    "4 16 -40 0 -20 -40 24 -20 40 24 -20 40 0 -20\r\n"
    "5 24 0 0 0 1 0 0 0 1 0 0 0 1\r\n";

TEST_CASE("an LDraw file reads its licence, sub-files and faces, skipping edge lines") {
    const Result<LdrawFile> file = ParseLdraw(kBrick);

    REQUIRE(file.has_value());
    CHECK(file->licence == "Licensed under CC BY 4.0 : see CAreadme.txt");
    REQUIRE(file->references.size() == 1);
    CHECK(file->references[0] == LdrawReference{.file = "s\\3001s01.dat"});
    REQUIRE(file->faces.size() == 2);
    CHECK(file->faces[0] == LdrawFace{.colour = 4, .corners = {{{}, {.x = 1.0F}, {.y = 1.0F}}}});
    CHECK(file->faces[1].cornerCount == 4);
    CHECK(file->faces[1].corners[2] == Vector3{.x = 40.0F, .y = 24.0F, .z = -20.0F});
}

TEST_CASE("a sub-file line reads its colour, matrix and a name with spaces") {
    const Result<LdrawFile> file = ParseLdraw("1 0x2FF0000 1 2 3 0 0 1 0 1 0 -1 0 0 my part.dat");

    REQUIRE(file.has_value());
    REQUIRE(file->references.size() == 1);
    const LdrawReference& reference = file->references[0];
    CHECK(reference.colour == 0x2FF0000);
    CHECK(reference.file == "my part.dat");
    CHECK(Apply(reference.matrix, {.x = 1.0F}) == Vector3{.x = 1.0F, .y = 2.0F, .z = 2.0F});
}

TEST_CASE("clockwise faces are stored counter-clockwise, and INVERTNEXT marks the next sub-file") {
    const Result<LdrawFile> file = ParseLdraw(
        "0 BFC CERTIFY CW\n3 16 0 0 0 1 0 0 0 1 0\n0 BFC INVERTNEXT\n"
        "1 16 0 0 0 1 0 0 0 1 0 0 0 1 a.dat\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 b.dat\n");

    REQUIRE(file.has_value());
    CHECK(file->faces[0].corners[1] == Vector3{.y = 1.0F});
    CHECK(file->references[0].isInverted);
    CHECK_FALSE(file->references[1].isInverted);
}

TEST_CASE("a short or non-numeric face or sub-file line fails") {
    CHECK_FALSE(ParseLdraw("3 16 0 0 0 1 0 0 0 1").has_value());
    CHECK_FALSE(ParseLdraw("4 16 0 0 0 1 0 0 0 1 0 0 x 0").has_value());
    CHECK_FALSE(ParseLdraw("1 16 0 0 0 1 0 0 0 1 0 0 0 1").has_value());
}

TEST_CASE("composed matrices apply the inner one first") {
    const LdrawMatrix turn{.rows = {Vector3{.y = -1.0F}, Vector3{.x = 1.0F}, Vector3{.z = 1.0F}}};
    const LdrawMatrix shift{.position = {.x = 10.0F}};

    CHECK(Apply(Compose(turn, shift), {}) == Vector3{.y = 10.0F});
    CHECK(Apply(Compose(shift, turn), {.x = 1.0F}) == Vector3{.x = 10.0F, .y = 1.0F});
    CHECK(Determinant(turn) == 1.0F);
}

}
}
