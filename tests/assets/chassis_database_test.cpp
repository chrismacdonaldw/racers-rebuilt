#include "assets/chassis_database.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Block;
using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

std::vector<std::byte> WheelModel(std::uint8_t field) {
    return Join(
        {Bytes({field}),
         Block(Join({Bytes({0x34}), Text("rrjmw"), Bytes({0x35}), Text("rrjmw"), Bytes({0x36}),
                     Text("rrjmw"), Bytes({0x37}), Text("rrjmw"), Bytes({0x38}), Text("rrjmw")}))});
}

std::vector<std::byte> FullChassis() {
    return Join({
        WheelModel(0x28),
        Bytes({0x2A}),
        Floats({-1, 0, 0}),
        Bytes({0x2B}),
        Floats({-3, 0, 1}),
        Bytes({0x2C}),
        Floats({2500}),
        Bytes({0x2D}),
        Floats({1, 2}),
        Bytes({0x2E}),
        Floats({8, 9}),
        Bytes({0x2F}),
        Floats({1}),
        Bytes({0x30}),
        Block(Floats({2, 3, 1, 1, 0, 1, -1, 0, -1, 1, 0, -1, -1, 0})),
        Bytes({0x31}),
        Block(Floats({2, -2, 0, 2, 2, 0, -1, -2, 0, -1, 2, 0})),
        Bytes({0x32}),
        Ints({0}),
        Bytes({0x33}),
        Ints({0}),
        Bytes({0x39}),
        Text("shadow"),
        Bytes({0x3A}),
        Ints({70}),
        Bytes({0x3B}),
        Ints({60}),
        Bytes({0x3C}),
        Ints({100}),
    });
}

TEST_CASE("a chassis reads its wheel model, seat, wheels, skids and stats") {
    const Result<std::vector<ChassisEntry>> chassis =
        ParseChassis(NamedLibrary(0x27, {NamedEntry(0x27, "rrchas0", FullChassis())}));

    REQUIRE(chassis.has_value());
    REQUIRE(chassis->size() == 1);
    const ChassisEntry& rr = chassis->front();
    CHECK(rr.name == "rrchas0");
    CHECK(rr.wheels.model.geometry == "rrjmw");
    CHECK(rr.wheels.skeleton == "rrjmw");
    CHECK(rr.wheels.animations == "rrjmw");
    CHECK_FALSE(rr.bodyAttachment.has_value());
    CHECK(rr.builtCentreOfMass == Vector3{.x = -1.0F});
    CHECK(rr.seat == Vector3{.x = -3.0F, .z = 1.0F});
    CHECK(rr.baseMass == 2500.0F);
    CHECK(rr.shadowWidth == 8.0F);
    CHECK(rr.shadowLength == 9.0F);
    CHECK(rr.shadowTexture == "shadow");
    CHECK(rr.enginePitchScale == 1.0F);
    CHECK(rr.skids.frontWidth == 2.0F);
    CHECK(rr.skids.rearWidth == 3.0F);
    CHECK(rr.skids.wheelOffsets[3] == Vector3{.x = -1.0F, .y = -1.0F});
    CHECK(rr.wheelPositions[0] == Vector3{.x = 2.0F, .y = -2.0F});
    CHECK(rr.wheelPositions[3] == Vector3{.x = -1.0F, .y = 2.0F});
    CHECK(rr.handling == 70);
    CHECK(rr.topSpeed == 60);
    CHECK(rr.acceleration == 100);
}

TEST_CASE("a chassis without stats takes the defaults") {
    const Result<std::vector<ChassisEntry>> chassis =
        ParseChassis(NamedLibrary(0x27, {NamedEntry(0x27, "dbchas0", WheelModel(0x29))}));

    REQUIRE(chassis.has_value());
    const ChassisEntry& db = chassis->front();
    CHECK(db.bodyAttachment.value_or(JointedModelFiles{}).model.materials == "rrjmw");
    CHECK(db.handling == 50);
    CHECK(db.topSpeed == 50);
    CHECK(db.acceleration == 80);
}

TEST_CASE("a chassis with too few wheel positions fails") {
    const std::vector cmb = NamedLibrary(
        0x27, {NamedEntry(0x27, "rrchas0", Join({Bytes({0x31}), Block(Floats({1, 2, 3}))}))});

    CHECK_FALSE(ParseChassis(cmb).has_value());
}

TEST_CASE("a chassis stat above 255 fails") {
    const std::vector cmb =
        NamedLibrary(0x27, {NamedEntry(0x27, "rrchas0", Join({Bytes({0x3A, 0x0D, 0x00, 0x01})}))});

    CHECK_FALSE(ParseChassis(cmb).has_value());
}

}
}
