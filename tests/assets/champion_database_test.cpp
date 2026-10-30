#include "assets/champion_database.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

TEST_CASE("champions read their body model, chassis, mass and centre of mass") {
    const std::vector ccb = NamedLibrary(
        0x27,
        {NamedEntry(0x27, "sb",
                    Join({Bytes({0x28}), Text("sscm"), Bytes({0x29}), Text("sscm"), Bytes({0x2A}),
                          Text("sscm"), Bytes({0x2B}), Text("sbchas0"), Bytes({0x2C}),
                          Floats({4500}), Bytes({0x2D}), Floats({-1, 0, -1})}))});

    const Result<std::vector<ChampionEntry>> champions = ParseChampions(ccb);

    REQUIRE(champions.has_value());
    REQUIRE(champions->size() == 1);
    const ChampionEntry& sb = champions->front();
    CHECK(sb.name == "sb");
    CHECK(sb.body == ModelFiles{.materials = "sscm", .textures = "sscm", .geometry = "sscm"});
    CHECK(sb.chassis == "sbchas0");
    CHECK(sb.mass == 4500.0F);
    CHECK(sb.centreOfMass == Vector3{.x = -1.0F, .y = 0.0F, .z = -1.0F});
}

TEST_CASE("a champion mass stored as an int fails") {
    CHECK_FALSE(ParseChampions(NamedLibrary(0x27, {NamedEntry(0x27, "rr",
                                                              Join({Bytes({0x2C}), Ints({200})}))}))
                    .has_value());
}

}
}
