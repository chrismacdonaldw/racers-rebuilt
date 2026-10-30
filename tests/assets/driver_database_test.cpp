#include "assets/driver_database.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

TEST_CASE("drivers read their model, champion, stats and AI charge") {
    const std::vector ddb = NamedLibrary(
        0x27, {NamedEntry(0x27, "RR",
                          Join({Bytes({0x33}), Ints({7}),        Bytes({0x28}), Text("rrpelvis"),
                                Bytes({0x29}), Text("rrpelvis"), Bytes({0x2A}), Text("rrpelvis"),
                                Bytes({0x2B}), Text("rr"),       Bytes({0x2C}), Ints({10}),
                                Bytes({0x2D}), Ints({20}),       Bytes({0x2E}), Ints({30}),
                                Bytes({0x2F}), Ints({40}),       Bytes({0x30}), Ints({50}),
                                Bytes({0x31}), Ints({60}),       Bytes({0x34}), Ints({3}),
                                Bytes({0x35}), Ints({1}),        Bytes({0x36}), Ints({2}),
                                Bytes({0x37}), Ints({4}),        Bytes({0x38}), Ints({5}),
                                Bytes({0x3A}), Ints({3, 2})})),
               NamedEntry(0x27, "AD", {})});

    const Result<std::vector<DriverEntry>> drivers = ParseDrivers(ddb);

    REQUIRE(drivers.has_value());
    REQUIRE(drivers->size() == 2);
    const DriverEntry& rr = drivers->at(0);
    CHECK(rr.name == "RR");
    CHECK(rr.nameIndex == 7);
    CHECK(rr.model ==
          ModelFiles{.materials = "rrpelvis", .textures = "rrpelvis", .geometry = "rrpelvis"});
    CHECK(rr.champion == "rr");
    CHECK(rr.redStat == 10);
    CHECK(rr.yellowStat == 20);
    CHECK(rr.greenStat == 30);
    CHECK(rr.blueStat == 40);
    CHECK(rr.voiceBank == 3);
    CHECK(rr.hatIndex == 1);
    CHECK(rr.faceIndex == 2);
    CHECK(rr.torsoIndex == 4);
    CHECK(rr.legsIndex == 5);
    CHECK(rr.aiChargeColor == 3);
    CHECK(rr.aiWhiteBrickTarget == 2);

    const DriverEntry& ad = drivers->at(1);
    CHECK(ad.champion.empty());
    CHECK(ad.aiChargeColor == 0);
}

TEST_CASE("a driver with an unknown field fails") {
    CHECK_FALSE(
        ParseDrivers(NamedLibrary(0x27, {NamedEntry(0x27, "RR", Join({Bytes({0x32}), Ints({1})}))}))
            .has_value());
}

TEST_CASE("a driver whose champion is a number fails") {
    CHECK_FALSE(
        ParseDrivers(NamedLibrary(0x27, {NamedEntry(0x27, "RR", Join({Bytes({0x2B}), Ints({1})}))}))
            .has_value());
}

}
}
