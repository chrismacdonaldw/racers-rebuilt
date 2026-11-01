#include "assets/hazard_list.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "token_bytes.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> HazardFile(std::uint8_t count, const std::vector<std::byte>& hazards) {
    return test::Join({test::Bytes({0x27}), test::ListHeader(count), hazards, test::Bytes({0x06})});
}

std::vector<std::byte> Prop(std::initializer_list<std::string_view> entities) {
    std::vector<std::byte> names;
    for (const std::string_view entity : entities) {
        names = test::Join({names, test::Bytes({0x42}), test::Text(entity)});
    }
    return test::Join({test::Bytes({0x34}),
                       test::Block(test::Join({test::Bytes({0x41}), test::Text("shootme"), names,
                                               test::Bytes({0x3B}), test::Ints({7})}))});
}

TEST_CASE("hazards without data and with field blocks read in order") {
    const std::vector<std::byte> bytes = HazardFile(
        3, test::Join({test::Bytes({0x28}), Prop({"block", "barrel"}), test::Bytes({0x43}),
                       test::Block(test::Join({test::Bytes({0x42}), test::Text("prntdum"),
                                               test::Bytes({0x3B}), test::Ints({4}),
                                               test::Bytes({0x44}), test::Ints({150})}))}));

    const Result<std::vector<Hazard>> hazards = ParseHazards(bytes);

    REQUIRE(hazards.has_value());
    REQUIRE(hazards->size() == 3);
    CHECK((*hazards)[0].kind == HazardKind::FallingPillar);
    CHECK(std::holds_alternative<std::monostate>((*hazards)[0].payload));
    const auto& prop = std::get<BreakableProp>((*hazards)[1].payload);
    CHECK(prop.surface == "shootme");
    CHECK(prop.entities == std::vector<std::string>{"block", "barrel"});
    CHECK(prop.trigger == 7);
    const auto& dragon = std::get<DragonCannonballs>((*hazards)[2].payload);
    CHECK_FALSE(dragon.material.has_value());
    CHECK(dragon.entity == "prntdum");
    CHECK(dragon.interval == Milliseconds{150});
}

TEST_CASE("a positional swinging box and water scroll read their values") {
    const std::vector<std::byte> bytes = HazardFile(
        2, test::Join({test::Bytes({0x3E}),
                       test::Block(test::Join({test::Text("dum02"), test::Ints({5}),
                                               test::Floats({100, 40, 10, 40})})),
                       test::Bytes({0x36}), test::Text("water01"), test::Floats({2, 0})}));

    const Result<std::vector<Hazard>> hazards = ParseHazards(bytes);

    REQUIRE(hazards.has_value());
    const auto& box = std::get<SwingingBox>((*hazards)[0].payload);
    CHECK(box.entity == "dum02");
    CHECK(box.trigger == 5);
    CHECK(box.startFrame == 100.0F);
    CHECK(box.size == Vector3{.x = 40.0F, .y = 10.0F, .z = 40.0F});
    const auto& scroll = std::get<WaterScroll>((*hazards)[1].payload);
    CHECK(scroll.entity == "water01");
    CHECK(scroll.uAmplitude == 2.0F);
}

TEST_CASE("ship cannons read their launch and target lists and their shot") {
    const auto point = [](std::int16_t x, std::uint8_t event) {
        return test::Join({test::Floats({x, 0, 0}), test::Ints({event})});
    };
    const std::vector<std::byte> bytes = HazardFile(
        1, test::Join({test::Bytes({0x3D}),
                       test::Block(test::Join(
                           {test::Bytes({0x37}), test::ListHeader(1), point(1, 8),
                            test::Bytes({0x06}), test::Bytes({0x38}), test::ListHeader(2),
                            point(2, 50), point(3, 8), test::Bytes({0x06}), test::Bytes({0x33}),
                            test::Block(test::Join({test::Bytes({0x37}), test::Floats({4, 5, 6}),
                                                    test::Bytes({0x3A}), test::Floats({700}),
                                                    test::Bytes({0x3B}), test::Ints({96})}))}))}));

    const Result<std::vector<Hazard>> hazards = ParseHazards(bytes);

    REQUIRE(hazards.has_value());
    CHECK(std::get<ShipCannons>(hazards->front().payload) ==
          ShipCannons{.launches = {{.position = {.x = 1.0F}, .event = 8}},
                      .targets = {{.position = {.x = 2.0F}, .event = 50},
                                  {.position = {.x = 3.0F}, .event = 8}},
                      .shot = {.launch = {.x = 4.0F, .y = 5.0F, .z = 6.0F},
                               .triggerRadius = 700.0F,
                               .event = 96}});
}

TEST_CASE("an unknown hazard class fails") {
    const Result<std::vector<Hazard>> hazards = ParseHazards(HazardFile(1, test::Bytes({0x31})));

    REQUIRE_FALSE(hazards.has_value());
    CHECK(hazards.error().message.contains("0x31"));
}

TEST_CASE("a breakable prop with more than four entities fails") {
    CHECK(ParseHazards(HazardFile(1, Prop({"a", "b", "c", "d"}))).has_value());
    CHECK_FALSE(ParseHazards(HazardFile(1, Prop({"a", "b", "c", "d", "e"}))).has_value());
}

TEST_CASE("a breakable prop with a repeated surface or trigger fails") {
    const auto prop = [](std::uint8_t field, const std::vector<std::byte>& first,
                         const std::vector<std::byte>& second) {
        return HazardFile(1, test::Join({test::Bytes({0x34}),
                                         test::Block(test::Join({test::Bytes({field}), first,
                                                                 test::Bytes({field}), second}))}));
    };

    CHECK_FALSE(ParseHazards(prop(0x41, test::Text("a"), test::Text("b"))).has_value());
    CHECK_FALSE(ParseHazards(prop(0x3B, test::Ints({7}), test::Ints({8}))).has_value());
}

TEST_CASE("an empty hazard list parses with or without its braces") {
    const Result<std::vector<Hazard>> braced = ParseHazards(HazardFile(0, {}));
    const Result<std::vector<Hazard>> bare =
        ParseHazards(test::Join({test::Bytes({0x27, 0x07, 0x0B, 0x00, 0x08})}));

    REQUIRE(braced.has_value());
    REQUIRE(bare.has_value());
    CHECK(braced->empty());
    CHECK(bare->empty());
}

}
}
