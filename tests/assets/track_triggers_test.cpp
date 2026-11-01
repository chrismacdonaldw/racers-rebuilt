#include "assets/track_triggers.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> EntryList(std::uint8_t count, const std::vector<std::byte>& entries) {
    return test::Join({test::Bytes({0x27}), test::ListHeader(count), entries, test::Bytes({0x06})});
}

std::vector<std::byte> Entry(const std::vector<std::byte>& fields) {
    return test::Join({test::Bytes({0x27}), test::Block(fields)});
}

std::vector<std::byte> SphereFields() {
    return test::Join({test::Bytes({0x29}), test::Floats({10, -20, 5}), test::Bytes({0x2A}),
                       test::Floats({40}), test::Bytes({0x2B}), test::Ints({7})});
}

TEST_CASE("an environment sphere reads its centre, radius and event") {
    const std::vector<std::byte> bytes =
        EntryList(2, test::Join({Entry(SphereFields()),
                                 Entry(test::Join({SphereFields(), test::Bytes({0x2F})}))}));

    const Result<std::vector<TriggerSphere>> spheres =
        ParseTriggerSpheres(bytes, TriggerList::Environment);

    REQUIRE(spheres.has_value());
    REQUIRE(spheres->size() == 2);
    const TriggerSphere& first = (*spheres)[0];
    CHECK(first.centre == Vector3{.x = 10.0F, .y = -20.0F, .z = 5.0F});
    CHECK(first.radius == 40.0F);
    CHECK(first.event == 7);
    CHECK_FALSE(first.isPlayerOnly);
    CHECK_FALSE(first.collisionWorld.has_value());
    CHECK_FALSE(first.lap.has_value());
    CHECK((*spheres)[1].isPlayerOnly);
}

std::vector<std::byte> RacerSphere(std::string_view world) {
    return EntryList(
        1, Entry(test::Join({SphereFields(), test::Bytes({0x2D}), test::Text(world),
                             test::Bytes({0x2E}), test::Ints({0}), test::Bytes({0x2C})})));
}

TEST_CASE("a racer sphere reads its collision world, lap and projectile flag") {
    const Result<std::vector<TriggerSphere>> racer =
        ParseTriggerSpheres(RacerSphere("startfin"), TriggerList::Racer);

    REQUIRE(racer.has_value());
    REQUIRE(racer->size() == 1);
    const TriggerSphere& sphere = racer->front();
    CHECK(sphere.collisionWorld == "startfin");
    CHECK(sphere.lap == 0);
    CHECK(sphere.isForProjectiles);
}

TEST_CASE("the environment list rejects racer-only fields") {
    const std::vector<std::byte> bytes =
        EntryList(1, Entry(test::Join({SphereFields(), test::Bytes({0x2E}), test::Ints({1})})));

    CHECK_FALSE(ParseTriggerSpheres(bytes, TriggerList::Environment).has_value());
    CHECK(ParseTriggerSpheres(bytes, TriggerList::Racer).has_value());
}

TEST_CASE("an empty trigger list parses with or without its braces") {
    const std::vector<std::byte> bare = test::Bytes({0x27, 0x07, 0x0B, 0x00, 0x08});
    const std::vector<std::byte> braced = EntryList(0, {});

    const Result<std::vector<TriggerSphere>> fromBare =
        ParseTriggerSpheres(bare, TriggerList::Racer);
    const Result<std::vector<TriggerSphere>> fromBraced =
        ParseTriggerSpheres(braced, TriggerList::Racer);

    REQUIRE(fromBare.has_value());
    REQUIRE(fromBraced.has_value());
    CHECK(fromBare->empty());
    CHECK(fromBraced->empty());
}

TEST_CASE("a trigger list with fewer entries than its count fails") {
    const std::vector<std::byte> bytes = EntryList(2, Entry(SphereFields()));

    CHECK_FALSE(ParseTriggerSpheres(bytes, TriggerList::Environment).has_value());
}

TEST_CASE("a timer reads fixed and random phases") {
    const std::vector<std::byte> bytes =
        EntryList(1, Entry(test::Join({test::Bytes({0x28, 0x2B}), test::Ints({120}),
                                       test::Bytes({0x29}), test::Ints({200}), test::Bytes({0x2A}),
                                       test::Ints({50}), test::Bytes({0x2D}), test::Ints({30})})));

    const Result<std::vector<TrackTimer>> timers = ParseTrackTimers(bytes);

    REQUIRE(timers.has_value());
    REQUIRE(timers->size() == 1);
    const TrackTimer& timer = timers->front();
    CHECK(timer.on.length == Milliseconds{120});
    CHECK(timer.on.isRandom);
    CHECK(timer.off.length == Milliseconds{200});
    CHECK_FALSE(timer.off.isRandom);
    CHECK(timer.event == 50);
    CHECK(timer.startDelay == Milliseconds{30});
}

TEST_CASE("a timer without an event fires nothing") {
    const std::vector<std::byte> bytes =
        EntryList(1, Entry(test::Join({test::Bytes({0x28}), test::Ints({10})})));

    const Result<std::vector<TrackTimer>> timers = ParseTrackTimers(bytes);

    REQUIRE(timers.has_value());
    REQUIRE(timers->size() == 1);
    CHECK(timers->front().event == kNoEvent);
    CHECK(timers->front().off.length == Milliseconds{0});
}

TEST_CASE("a timer with a negative time fails") {
    const std::vector<std::byte> bytes =
        EntryList(1, Entry(test::Join({test::Bytes({0x2D, 0x04, 0xFF, 0xFF, 0xFF, 0xFF})})));

    CHECK_FALSE(ParseTrackTimers(bytes).has_value());
}

struct SphereCounts {
    std::size_t files = 0;
    std::size_t environment = 0;
    std::size_t racer = 0;

    friend bool operator==(const SphereCounts&, const SphereCounts&) = default;
};

SphereCounts CountRetailSpheres(const JamArchive& archive) {
    SphereCounts counts;
    for (const IndexEntry& entry : test::FilesWithExtension(archive, "TRB")) {
        ++counts.files;
        const bool isEnvironment = entry.Name() == "ENVIRON.TRB";
        const Result<std::vector<TriggerSphere>> spheres =
            test::ParseEntry(archive, entry, [&](std::span<const std::byte> bytes) {
                return ParseTriggerSpheres(
                    bytes, isEnvironment ? TriggerList::Environment : TriggerList::Racer);
            });
        if (!spheres) {
            FAIL_CHECK(entry.path, ": ", spheres.error().message);
            continue;
        }
        (isEnvironment ? counts.environment : counts.racer) += spheres->size();
    }
    return counts;
}

TEST_CASE("every trigger list in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    CHECK(CountRetailSpheres(*archive) ==
          SphereCounts{.files = 28, .environment = 182, .racer = 93});
}

TEST_CASE("every timer list in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t timerCount = 0;
    const test::Survey timers = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "TIB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseTrackTimers(bytes).transform(
                [&](const std::vector<TrackTimer>& read) { timerCount += read.size(); });
        });
    test::CheckSurvey(timers, 14);
    CHECK(timerCount == 9);
}

}
}
