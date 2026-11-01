#include "assets/event_table.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> Section(std::uint8_t id,
                               std::initializer_list<std::vector<std::byte>> records) {
    return test::Join({test::Bytes({id}),
                       test::ListHeader(static_cast<std::uint8_t>(records.size())),
                       test::Join(records), test::Bytes({0x06})});
}

std::vector<std::byte> Record(std::uint8_t event, const std::vector<std::byte>& fields) {
    return test::Join({test::Bytes({0x27}), test::Ints({event}), test::Block(fields)});
}

std::vector<std::byte> EndRecord(std::uint8_t event, const std::vector<std::byte>& fields) {
    return test::Join(
        {test::Bytes({0x27}), test::Ints({event}), test::Bytes({0x3C}), test::Block(fields)});
}

std::vector<std::byte> SubEvent(std::uint8_t slot, std::uint8_t event) {
    return test::Join({test::Bytes({0x27, slot}), test::Ints({event})});
}

TEST_CASE("a sound record keeps its defaults for fields it leaves out") {
    const std::vector<std::byte> bytes = Section(
        0x2A,
        {Record(4, test::Join({test::Bytes({0x2C}), test::Ints({12}), test::Bytes({0x3B}),
                               test::Floats({1, 2, 3}), test::Bytes({0x2E}), test::Ints({0}),
                               test::Bytes({0x2D, 0x3F})})),
         Record(5, test::Join({test::Bytes({0x40}), test::Floats({0}), test::Bytes({0x33}),
                               test::Text("bone01"), test::Bytes({0x54}), test::Ints({2})}))});

    const Result<EventTableAsset> table = ParseEventTable(bytes);

    REQUIRE(table.has_value());
    REQUIRE(table->sounds.size() == 2);
    const SoundResource& looped = table->sounds[0];
    CHECK(looped.trigger.event == 4);
    CHECK(looped.soundId == 12);
    CHECK(looped.position == Vector3{.x = 1.0F, .y = 2.0F, .z = 3.0F});
    CHECK(looped.isLooping);
    CHECK(looped.trigger.usesEventPosition);
    CHECK(looped.volume == 1.0F);
    CHECK(looped.minDistance == 30.0F);
    CHECK(looped.maxDistance == 300.0F);
    CHECK(looped.probability == 1.0F);
    const SoundResource& attached = table->sounds[1];
    CHECK_FALSE(attached.position.has_value());
    CHECK(attached.probability == 0.0F);
    CHECK(attached.entity == "bone01");
    CHECK(attached.node == 2);
}

TEST_CASE("sections may come in any order but only once each") {
    const std::vector<std::byte> lapZones = Section(
        0x51,
        {Record(100, test::Bytes({0x36})), Record(101, {}), Record(102, test::Bytes({0x37}))});
    const std::vector<std::byte> timers =
        Section(0x4B, {Record(70, test::Join({test::Bytes({0x49}), test::Ints({250})}))});

    const Result<EventTableAsset> table = ParseEventTable(test::Join({lapZones, timers}));

    REQUIRE(table.has_value());
    REQUIRE(table->lapZones.size() == 3);
    CHECK(table->lapZones[0].zone == LapZone::OpenTrack);
    CHECK(table->lapZones[1].zone == LapZone::FinishLine);
    CHECK(table->lapZones[2].zone == LapZone::PostLine);
    REQUIRE(table->timers.size() == 1);
    CHECK(table->timers[0].duration == Milliseconds{250});
    CHECK(table->timers[0].holdEvent == kNoEvent);
    CHECK_FALSE(ParseEventTable(test::Join({timers, lapZones, timers})).has_value());
}

TEST_CASE("an unknown section fails") {
    CHECK_FALSE(ParseEventTable(Section(0x30, {})).has_value());
}

TEST_CASE("sub-events fill their slots") {
    const std::vector<std::byte> bytes = Section(
        0x28,
        {Record(3, test::Join({test::Bytes({0x33}), test::Text("rkhamm02"), SubEvent(0x36, 20),
                               SubEvent(0x37, 21), test::Bytes({0x36}), test::Ints({1})}))});

    const Result<EventTableAsset> table = ParseEventTable(bytes);

    REQUIRE(table.has_value());
    REQUIRE(table->partAnimations.size() == 1);
    const PartAnimationResource& animation = table->partAnimations[0];
    CHECK(animation.trigger.subEvents == SubEvents{.start = 20, .end = 21});
    CHECK(animation.startPart == 1);
    CHECK(animation.endPart == -1);
    CHECK(animation.entity == EntityRef{.name = "rkhamm02"});
}

TEST_CASE("an unknown sub-event slot fails") {
    CHECK_FALSE(ParseEventTable(Section(0x4B, {Record(1, SubEvent(0x35, 2))})).has_value());
}

TEST_CASE("sounds and links take no sub-events") {
    CHECK_FALSE(ParseEventTable(Section(0x2A, {Record(1, SubEvent(0x36, 2))})).has_value());
}

TEST_CASE("the shared database applies only to entities named after it") {
    const std::vector<std::byte> bytes = Section(
        0x53,
        {Record(60, test::Join({test::Bytes({0x41, 0x33}), test::Text("mmlaser1")})),
         Record(61, test::Join({test::Bytes({0x33}), test::Text("mmlaser2"), test::Bytes({0x41})})),
         Record(62, test::Bytes({0x41, 0x46}))});

    const Result<EventTableAsset> table = ParseEventTable(bytes);

    REQUIRE(table.has_value());
    REQUIRE(table->modelDistances.size() == 3);
    CHECK(table->modelDistances[0].entity == EntityRef{.name = "mmlaser1", .isShared = true});
    CHECK(table->modelDistances[1].entity == EntityRef{.name = "mmlaser2"});
    CHECK_FALSE(table->modelDistances[2].entity.has_value());
    CHECK(table->modelDistances[2].hidesWhileActive);
}

TEST_CASE("an event link names the event it forces to end") {
    const std::vector<std::byte> link =
        test::Join({test::Bytes({0x27}), test::Ints({8}), test::Bytes({0x05, 0x39}),
                    test::Ints({9}), test::Bytes({0x06})});

    const Result<EventTableAsset> table = ParseEventTable(Section(0x39, {link}));

    REQUIRE(table.has_value());
    REQUIRE(table->links.size() == 1);
    CHECK(table->links[0].event == 8);
    CHECK(table->links[0].forced == 9);
}

TEST_CASE("lap zones and look targets reject the trigger-on-end flag") {
    CHECK_FALSE(ParseEventTable(Section(0x51, {EndRecord(100, {})})).has_value());
    CHECK_FALSE(ParseEventTable(Section(0x55, {EndRecord(1, {})})).has_value());
    CHECK(ParseEventTable(Section(0x59, {EndRecord(1, {})})).has_value());
}

TEST_CASE("a colour transform that triggers on end also ignores the end") {
    const std::vector<std::byte> bytes =
        Section(0x4D, {EndRecord(3, test::Join({test::Bytes({0x4E}), test::Ints({1, 1, 1, 0}),
                                                test::Bytes({0x4F}), test::Ints({32, 32, 32, 0})})),
                       Record(4, test::Bytes({0x50}))});

    const Result<EventTableAsset> table = ParseEventTable(bytes);

    REQUIRE(table.has_value());
    REQUIRE(table->colorTransforms.size() == 2);
    const ColorTransformResource& applied = table->colorTransforms[0];
    CHECK(applied.trigger.startsOnEnd);
    CHECK(applied.trigger.ignoresEnd);
    CHECK(applied.shifts == std::array<std::int32_t, 4>{1, 1, 1, 0});
    CHECK(applied.offsets == std::array<std::int32_t, 4>{32, 32, 32, 0});
    CHECK(table->colorTransforms[1].clears);
    CHECK_FALSE(table->colorTransforms[1].trigger.ignoresEnd);
}

TEST_CASE("a particle record reads its emitter and orientation") {
    const std::vector<std::byte> bytes = Section(
        0x3D, {EndRecord(9, test::Join({test::Bytes({0x3D}), test::Text("drgnsmok"),
                                        test::Bytes({0x3E}), test::Floats({0, 1, 0, 0, 0, -1})}))});

    const Result<EventTableAsset> table = ParseEventTable(bytes);

    REQUIRE(table.has_value());
    REQUIRE(table->particles.size() == 1);
    const ParticleResource& particle = table->particles[0];
    CHECK(particle.emitter == "drgnsmok");
    CHECK(particle.trigger.startsOnEnd);
    CHECK(particle.direction == Vector3{.y = 1.0F});
    CHECK(particle.up == Vector3{.z = -1.0F});
    CHECK(particle.position == Vector3{});
}

using SectionCounts = std::array<std::size_t, 13>;

SectionCounts CountSections(const EventTableAsset& table) {
    return {
        table.sounds.size(),        table.partAnimations.size(), table.materialAnimations.size(),
        table.particles.size(),     table.links.size(),          table.skyStates.size(),
        table.timers.size(),        table.nodeTransforms.size(), table.colorTransforms.size(),
        table.lapZones.size(),      table.modelDistances.size(), table.lookTargets.size(),
        table.externalForces.size()};
}

TEST_CASE("every event table in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    SectionCounts totals{};
    const test::Survey tables = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "EVB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseEventTable(bytes).transform([&](const EventTableAsset& table) {
                std::ranges::transform(totals, CountSections(table), totals.begin(), std::plus{});
            });
        });
    test::CheckSurvey(tables, 14);
    CHECK(totals == SectionCounts{238, 11, 0, 37, 0, 4, 15, 5, 32, 39, 7, 0, 0});
}

}
}
