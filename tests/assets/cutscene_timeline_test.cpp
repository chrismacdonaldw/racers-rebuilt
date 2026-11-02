#include "assets/cutscene_timeline.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <variant>
#include <vector>

#include "core/result.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> Tagged(std::uint8_t id, const std::vector<std::byte>& value = {}) {
    return test::Join({test::Bytes({id}), value});
}

std::vector<std::byte> Span(std::uint8_t start, std::uint8_t duration) {
    return test::Join({Tagged(0x2B, test::Ints({start})), Tagged(0x2C, test::Ints({duration}))});
}

std::vector<std::byte> Int32(std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    return test::Bytes(
        {0x04, static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8U),
         static_cast<std::uint8_t>(bits >> 16U), static_cast<std::uint8_t>(bits >> 24U)});
}

std::vector<std::byte> Worlds(std::initializer_list<std::string_view> names) {
    std::vector<std::byte> bytes = test::Join(
        {test::Bytes({0x28}), test::ListHeader(static_cast<std::uint8_t>(names.size()))});
    for (const std::string_view name : names) {
        bytes = test::Join({bytes, test::Text(name)});
    }
    return test::Join({bytes, test::Bytes({0x06})});
}

std::vector<std::byte> Frames(std::initializer_list<std::vector<std::byte>> frames) {
    return test::NamedLibrary(0x27, frames);
}

std::vector<std::byte> Frame(std::string_view name, const std::vector<std::byte>& fields) {
    return test::NamedEntry(0x27, name, fields);
}

std::vector<std::byte> OneFrame(const std::vector<std::byte>& fields) {
    return test::Join({Worlds({"scene"}), Frames({Frame("main", fields)})});
}

std::vector<std::byte> Cameras(std::initializer_list<std::vector<std::byte>> cameras) {
    return test::NamedLibrary(0x29, cameras);
}

std::vector<std::byte> Camera(std::string_view name, std::uint8_t start, std::uint8_t duration) {
    return test::NamedEntry(0x29, name,
                            test::Join({Span(start, duration), Tagged(0x2A, test::Text("cam"))}));
}

std::vector<std::byte> CameraFrom(std::int32_t start) {
    return Cameras(
        {test::NamedEntry(0x29, "a",
                          test::Join({Tagged(0x2B, Int32(start)), Tagged(0x2C, test::Ints({1})),
                                      Tagged(0x2A, test::Text("cam"))}))});
}

std::vector<std::byte> Model(const std::vector<std::byte>& fields) {
    return test::NamedLibrary(0x2E, {test::NamedEntry(0x2E, "model", fields)});
}

TEST_CASE("a frame without a length runs to its last camera's end") {
    const Result<CutsceneTimelineAsset> timeline =
        ParseCutsceneTimeline(OneFrame(Cameras({Camera("a", 0, 40), Camera("b", 30, 50)})));

    REQUIRE(timeline.has_value());
    REQUIRE(timeline->frames.size() == 1);
    const CutsceneFrame& frame = timeline->frames[0];
    CHECK(frame.name == "main");
    CHECK(frame.framesPerSecond == 30);
    CHECK(frame.length == 80);
    REQUIRE(frame.cameras.size() == 2);
    CHECK(frame.cameras[1].span == CutsceneSpan{.start = 30, .duration = 50});
    CHECK(frame.cameras[1].camera == "cam");
}

TEST_CASE("a frame keeps its own length and rate") {
    const Result<CutsceneTimelineAsset> timeline = ParseCutsceneTimeline(
        OneFrame(test::Join({Tagged(0x2C, test::Ints({150})), Tagged(0x3B, test::Ints({25})),
                             Cameras({Camera("a", 0, 40)})})));

    REQUIRE(timeline.has_value());
    CHECK(timeline->frames[0].length == 150);
    CHECK(timeline->frames[0].framesPerSecond == 25);
}

TEST_CASE("a zero-duration event and duplicate event names are kept") {
    const std::vector<std::byte> transforms = test::NamedLibrary(
        0x37, {test::NamedEntry(0x37, "flash", Span(12, 0)),
               test::NamedEntry(0x37, "flash",
                                test::Join({Span(20, 5), Tagged(0x33, test::Floats({1, 2, 3}))}))});

    const Result<CutsceneTimelineAsset> timeline = ParseCutsceneTimeline(OneFrame(transforms));

    REQUIRE(timeline.has_value());
    const std::vector<CutsceneTransform>& parsed = timeline->frames[0].transforms;
    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].name == "flash");
    CHECK(parsed[0].span == CutsceneSpan{.start = 12, .duration = 0});
    CHECK(parsed[1].name == "flash");
    CHECK(parsed[1].position == Vector3{.x = 1.0F, .y = 2.0F, .z = 3.0F});
    CHECK(parsed[1].direction == Vector3{.x = 1.0F});
    CHECK(timeline->frames[0].length == 0);
}

TEST_CASE("models name or index their entity and bind material animations") {
    const std::vector<std::byte> bytes = test::Join(
        {Worlds({"scene", "blended"}),
         Frames({Frame(
             "main",
             test::Join({test::NamedLibrary(
                 0x2E,
                 {test::NamedEntry(0x2E, "car",
                                   test::Join({Span(0, 10), Tagged(0x30, test::Text("carbody")),
                                               Tagged(0x2D, test::Ints({2}))})),
                  test::NamedEntry(
                      0x2E, "sky",
                      test::Join({Tagged(0x32, test::Ints({0, 4})),
                                  Tagged(0x36, test::Join({test::ListHeader(1),
                                                           test::Ints({1, 2, 0, 5, 3}),
                                                           test::Bytes({0x06})}))}))})}))})});

    const Result<CutsceneTimelineAsset> timeline = ParseCutsceneTimeline(bytes);

    REQUIRE(timeline.has_value());
    const std::vector<CutsceneModel>& models = timeline->frames[0].models;
    REQUIRE(models.size() == 2);
    CHECK(std::get<EntityByName>(models[0].model) ==
          EntityByName{.name = "carbody", .isJointed = true});
    CHECK(models[0].part == 2);
    CHECK(std::get<EntityByIndex>(models[1].model) == EntityByIndex{.world = 0, .entity = 4});
    REQUIRE(models[1].materials.size() == 1);
    CHECK(models[1].materials[0] ==
          CutsceneMaterialBinding{
              .world = 1, .animation = 2, .track = 0, .slot = 5, .materialTable = 3});
}

TEST_CASE("lights read their colour, direction and blink") {
    const std::vector<std::byte> lights = test::Join(
        {test::NamedLibrary(
             0x35, {test::NamedEntry(0x35, "amb",
                                     test::Join({Span(0, 9), Tagged(0x38, test::Ints({10, 20, 30})),
                                                 Tagged(0x3C, test::Ints({4, 2}))}))}),
         test::NamedLibrary(
             0x3A, {test::NamedEntry(0x3A, "sun",
                                     test::Join({Tagged(0x38, test::Ints({255, 255, 0})),
                                                 Tagged(0x39, test::Floats({0, 0, -1}))}))})});

    const Result<CutsceneTimelineAsset> timeline = ParseCutsceneTimeline(OneFrame(lights));

    REQUIRE(timeline.has_value());
    const CutsceneFrame& frame = timeline->frames[0];
    REQUIRE(frame.ambientLights.size() == 1);
    CHECK(frame.ambientLights[0].color == CutsceneColor{10, 20, 30});
    CHECK(frame.ambientLights[0].blink == LightBlink{.framesOn = 4, .framesOff = 2});
    REQUIRE(frame.directionalLights.size() == 1);
    CHECK(frame.directionalLights[0].direction == Vector3{.z = -1.0F});
    CHECK_FALSE(frame.directionalLights[0].blink.has_value());
}

TEST_CASE("timelines outside the retail slice are rejected") {
    const std::vector<std::byte> camera = Cameras({Camera("a", 0, 10)});

    SUBCASE("a BSP model") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(Model(Tagged(0x31, test::Text("bsp"))))));
    }
    SUBCASE("a model indexing a missing world") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(Model(Tagged(0x32, test::Ints({1, 0}))))));
    }
    SUBCASE("a model naming nothing") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(Model(Span(0, 1)))));
    }
    SUBCASE("an unknown frame field") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(test::Join({camera, Tagged(0x50)}))));
    }
    SUBCASE("an unknown event field") {
        CHECK_FALSE(ParseCutsceneTimeline(
            OneFrame(test::NamedLibrary(0x29, {test::NamedEntry(0x29, "a", Tagged(0x50))}))));
    }
    SUBCASE("a repeated frame field") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(test::Join({camera, camera}))));
    }
    SUBCASE("an empty event list") {
        CHECK_FALSE(ParseCutsceneTimeline(
            OneFrame(test::Join({test::Bytes({0x29}), test::ListHeader(0), test::Bytes({0x06})}))));
    }
    SUBCASE("a name longer than 8 characters") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(Cameras({Camera("toolongname", 0, 1)}))));
    }
    SUBCASE("a negative start") {
        CHECK_FALSE(ParseCutsceneTimeline(OneFrame(CameraFrom(-1))));
    }
    SUBCASE("an end past the largest frame number") {
        CHECK_FALSE(
            ParseCutsceneTimeline(OneFrame(CameraFrom(std::numeric_limits<std::int32_t>::max()))));
    }
    SUBCASE("a repeated section") {
        CHECK_FALSE(ParseCutsceneTimeline(test::Join({OneFrame(camera), Worlds({"other"})})));
    }
    SUBCASE("an unknown section") {
        CHECK_FALSE(ParseCutsceneTimeline(
            test::Join({OneFrame(camera), test::Bytes({0x30}), test::Ints({1})})));
    }
    SUBCASE("no frames") {
        CHECK_FALSE(ParseCutsceneTimeline(Worlds({"scene"})));
    }
}

}
}
