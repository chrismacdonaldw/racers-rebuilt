#include "assets/ghost_run.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/token_bytes.hpp"
#include "core/mirror.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;

std::vector<std::byte> Short(std::int16_t value) {
    const auto bits = static_cast<std::uint16_t>(value);
    return Bytes(
        {0x0D, static_cast<std::uint8_t>(bits & 0xFFU), static_cast<std::uint8_t>(bits >> 8U)});
}

std::vector<std::byte> Sample(std::int16_t x, std::initializer_list<std::uint8_t> rotation) {
    return Join({Short(x), Short(-2), Short(3), Ints(rotation)});
}

std::vector<std::byte> Samples(std::initializer_list<std::vector<std::byte>> samples) {
    return Join({Bytes({0x27}), test::ListHeader(static_cast<std::uint8_t>(samples.size())),
                 Join(samples), Bytes({0x06})});
}

std::vector<std::byte> Ghost(const std::vector<std::byte>& fields) {
    return Join({Bytes({0x2C}), test::Block(fields)});
}

std::vector<std::byte> LapTimes() {
    return Join({Bytes({0x2A}), Ints({10, 20, 30})});
}

bool IsNear(const Vector3& a, const Vector3& b, float tolerance) {
    return Length(a - b) <= tolerance;
}

bool IsNear(const Quaternion& a, const Quaternion& b, float tolerance) {
    return std::abs(Dot(a, b)) >= 1.0F - tolerance;
}

TEST_CASE("a ghost file reads its lap times, start pose and samples in any field order") {
    const std::vector<std::byte> file = Ghost(Join({
        Samples({Sample(64, {0, 0, 0, 127}), Sample(-96, {251, 0, 0, 127})}),
        Bytes({0x2B}),
        Ints({0}),
        Bytes({0x29, 0x12, 0, 0x12, 0, 0x12, 0, 0x12, 127}),
        Bytes({0x28}),
        test::Floats({5, 6, 7}),
        LapTimes(),
    }));

    const Result<GhostRun> run = ParseGhb(file);

    REQUIRE(run.has_value());
    CHECK(run->laps == GhostLapTimes{Milliseconds{10}, Milliseconds{20}, Milliseconds{30}});
    CHECK(RaceTime(*run) == Milliseconds{60});
    CHECK(run->start.position == Vector3{5, 6, 7});
    CHECK(run->start.rotation == Quaternion{});
    REQUIRE(run->samples.size() == 2);
    CHECK(run->samples[0] == GhostSample{.position = {64, -2, 3}, .rotation = {0, 0, 0, 127}});
    CHECK(run->samples[1].rotation[0] == -5);
    CHECK(FromGhostSample(run->samples[1]).position == Vector3{-3, -0.0625F, 0.09375F});
}

TEST_CASE("a ghost file rejects bad samples, unknown fields and a missing ghost block") {
    CHECK_FALSE(ParseGhb(Ghost(Join({LapTimes(), Bytes({0x30}), Ints({1})}))).has_value());
    CHECK_FALSE(ParseGhb(LapTimes()).has_value());
    CHECK_FALSE(ParseGhb(Ghost(Samples({Sample(1, {0, 0, 0, 0})}))).has_value());
    CHECK_FALSE(ParseGhb(Ghost(Join({Bytes({0x27}), test::ListHeader(1), Short(1), Short(1),
                                     Short(1), Ints({0, 0, 0}), Short(300), Bytes({0x06})})))
                    .has_value());
}

TEST_CASE("a ghost file holds at most 961 samples") {
    const auto withSamples = [](std::uint16_t count) {
        std::vector<std::byte> file = Join({Bytes({0x2C, 0x05, 0x27, 0x07, 0x0E}),
                                            Bytes({static_cast<std::uint8_t>(count & 0xFFU),
                                                   static_cast<std::uint8_t>(count >> 8U)}),
                                            Bytes({0x08, 0x05})});
        for (std::uint16_t i = 0; i < count; ++i) {
            const std::vector<std::byte> sample = Sample(1, {0, 0, 0, 127});
            file.insert(file.end(), sample.begin(), sample.end());
        }
        const std::vector<std::byte> end = Bytes({0x06, 0x06});
        file.insert(file.end(), end.begin(), end.end());
        return file;
    };

    CHECK(ParseGhb(withSamples(961)).has_value());
    CHECK_FALSE(ParseGhb(withSamples(962)).has_value());
}

TEST_CASE("a ghost sample truncates toward zero and stores the inverse rotation") {
    const Quaternion turned = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F);
    const Transform pose{.position = {.x = 1.99F, .y = -1.99F, .z = 1000.0F}, .rotation = turned};

    const GhostSample sample = ToGhostSample(pose);

    CHECK(sample.position == std::array<std::int16_t, 3>{63, -63, 32000});
    CHECK(sample.rotation == std::array<std::int8_t, 4>{0, 0, -89, 89});
    const Transform back = FromGhostSample(sample);
    CHECK(IsNear(back.position, pose.position, 1.0F / 32.0F));
    CHECK(IsNear(back.rotation, turned, 1e-4F));
}

TEST_CASE("a ghost sample clamps positions past the stored range") {
    const GhostSample sample =
        ToGhostSample({.position = {.x = 5000.0F, .y = -5000.0F}, .rotation = {}});

    CHECK(sample.position[0] == 32767);
    CHECK(sample.position[1] == -32768);
}

using RetailGhosts = std::map<std::string, std::pair<std::int64_t, std::size_t>>;

void CheckRetailGhost(const GhostRun& run, const std::pair<std::int64_t, std::size_t>& expected) {
    CHECK(RaceTime(run).count() == expected.first);
    CHECK(run.samples.size() == expected.second);
    const Transform first = FromGhostSample(run.samples.front());
    CHECK(IsNear(first.position, run.start.position, 0.06F));
    CHECK(IsNear(first.rotation, run.start.rotation, 1e-3F));
}

void CheckRetailFile(const JamArchive& archive, const std::string& path,
                     const RetailGhosts& expected) {
    CAPTURE(path);
    const Result<GhostRun> run =
        test::ReadRetailFile(archive, path).and_then([](const FileData& data) {
            return ParseGhb(data.Bytes());
        });
    REQUIRE(run.has_value());
    const auto found = expected.find(path);
    REQUIRE(found != expected.end());
    CheckRetailGhost(*run, found->second);
}

void CheckTurnsMirrored(const Quaternion& mirrored, const Quaternion& original) {
    constexpr Vector3 kAhead{.x = 1.0F, .y = 2.0F, .z = 0.5F};
    const Vector3 actual = Rotate(mirrored, MirroredY(kAhead));
    const Vector3 expected = MirroredY(Rotate(original, kAhead));
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(1e-4));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(1e-4));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(1e-4));
}

TEST_CASE("a mirrored ghost run drives the mirrored line and turns the mirrored way") {
    constexpr Quaternion kTurn{.w = 0.8F, .x = 0.36F, .y = 0.48F};
    constexpr Transform kPose{.position = {.x = 10.0F, .y = 20.5F, .z = 3.0F}, .rotation = kTurn};
    const GhostRun run{.start = kPose, .samples = {ToGhostSample(kPose)}};

    const GhostRun mirrored = Mirrored(run);

    const Transform original = FromGhostSample(run.samples[0]);
    const Transform sample = FromGhostSample(mirrored.samples[0]);
    CHECK(sample.position == MirroredY(original.position));
    CheckTurnsMirrored(sample.rotation, original.rotation);
    CHECK(mirrored.start.position == MirroredY(kPose.position));
    CheckTurnsMirrored(mirrored.start.rotation, kTurn);
    CHECK(Mirrored(mirrored).samples == run.samples);
}

TEST_CASE("every retail ghost parses with Veronica Voltage's lap times") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const RetailGhosts expected{
        {"GAMEDATA/RACEC0R0/GHOST.GHB", {95347, 382}},
        {"GAMEDATA/RACEC0R1/GHOST.GHB", {122093, 489}},
        {"GAMEDATA/RACEC0R2/GHOST.GHB", {101658, 407}},
        {"GAMEDATA/RACEC0R3/GHOST.GHB", {97181, 389}},
        {"GAMEDATA/RACEC1R0/GHOST.GHB", {122077, 489}},
        {"GAMEDATA/RACEC1R1/GHOST.GHB", {106236, 426}},
        {"GAMEDATA/RACEC1R2/GHOST.GHB", {104317, 418}},
        {"GAMEDATA/RACEC1R3/GHOST.GHB", {103842, 415}},
        {"GAMEDATA/RACEC2R0/GHOST.GHB", {119373, 478}},
        {"GAMEDATA/RACEC2R1/GHOST.GHB", {115386, 462}},
        {"GAMEDATA/RACEC2R2/GHOST.GHB", {150691, 603}},
        {"GAMEDATA/RACEC2R3/GHOST.GHB", {104374, 417}},
    };
    for (const IndexEntry& entry : test::FilesWithExtension(*archive, "GHB")) {
        CheckRetailFile(*archive, entry.path, expected);
    }
    CHECK(test::FilesWithExtension(*archive, "GHB").size() == expected.size());
}

}
}
