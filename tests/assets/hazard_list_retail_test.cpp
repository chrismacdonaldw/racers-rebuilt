#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "archive_survey.hpp"
#include "assets/hazard_list.hpp"

namespace rr::assets {
namespace {

constexpr float kTolerance = 0.001F;

std::vector<Hazard> RetailHazards(const JamArchive& archive, std::string_view track) {
    Result<std::vector<Hazard>> hazards =
        test::ReadRetailFile(archive, std::format("GAMEDATA/{}/HAZARDS.HZB", track))
            .and_then([](const FileData& data) { return ParseHazards(data.Bytes()); });
    REQUIRE_MESSAGE(hazards.has_value(), track);
    return std::move(*hazards);
}

template <typename Payload>
std::vector<Payload> PayloadsOf(const JamArchive& archive, std::string_view track) {
    std::vector<Payload> payloads;
    for (const Hazard& hazard : RetailHazards(archive, track)) {
        if (const auto* payload = std::get_if<Payload>(&hazard.payload)) {
            payloads.push_back(*payload);
        }
    }
    return payloads;
}

bool Near(float actual, float expected) {
    return std::abs(actual - expected) <= kTolerance * std::max(1.0F, std::abs(expected));
}

bool Near(const Vector3& actual, const Vector3& expected) {
    return Near(actual.x, expected.x) && Near(actual.y, expected.y) && Near(actual.z, expected.z);
}

bool Near(const std::optional<Vector3>& actual, const std::optional<Vector3>& expected) {
    return actual.has_value() == expected.has_value() && (!actual || Near(*actual, *expected));
}

bool Near(const CannonShot& actual, const CannonShot& expected) {
    return Near(actual.launch, expected.launch) && Near(actual.target, expected.target) &&
           Near(actual.triggerCentre, expected.triggerCentre) &&
           Near(actual.triggerRadius, expected.triggerRadius) && actual.event == expected.event;
}

bool Near(const LightningCannon& actual, const LightningCannon& expected) {
    return actual.trigger == expected.trigger && actual.entity == expected.entity &&
           Near(actual.source, expected.source) && actual.period == expected.period &&
           Near(actual.sweepRadians, expected.sweepRadians) &&
           Near(actual.verticalRange, expected.verticalRange) &&
           Near(actual.verticalBase, expected.verticalBase);
}

bool Near(const WaterScroll& actual, const WaterScroll& expected) {
    return actual.entity == expected.entity && Near(actual.uAmplitude, expected.uAmplitude) &&
           Near(actual.vAmplitude, expected.vAmplitude);
}

template <typename Payload>
bool AllNear(const std::vector<Payload>& actual, const std::vector<Payload>& expected) {
    return std::ranges::equal(actual, expected,
                              [](const Payload& a, const Payload& b) { return Near(a, b); });
}

std::vector<HazardKind> Sorted(std::vector<HazardKind> kinds) {
    std::ranges::sort(kinds);
    return kinds;
}

TEST_CASE("every retail hazard list parses, with 27 hazards in 14 files") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t hazardCount = 0;
    const test::Survey survey = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "HZB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseHazards(bytes).transform(
                [&](const std::vector<Hazard>& read) { hazardCount += read.size(); });
        });
    test::CheckSurvey(survey, 14);
    CHECK(hazardCount == 27);
}

TEST_CASE("each retail track lists the hazard classes it uses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    using enum HazardKind;
    const std::vector<std::pair<std::string_view, std::vector<HazardKind>>> expected{
        {"RACEC0R0", {HammerSwish, SwingingBox}},
        {"RACEC0R1", {Crane, Cannon, BreakableProp}},
        {"RACEC0R2", {FallingPillar, Sphinx}},
        {"RACEC0R3", {LavaBlob, CodePuzzle, RocketPad}},
        {"RACEC1R0", {Ghost}},
        {"RACEC1R1", {WaterScroll}},
        {"RACEC1R2", {}},
        {"RACEC1R3", {Snowfall, SmokeVent}},
        {"RACEC2R0", {DragonCannonballs}},
        {"RACEC2R1", {WaterScroll, ShipCannons, ShipCannons}},
        {"RACEC2R2", {SwingingBox, SwingingBox, SwingingBox, CurseDrop, LightningCannon}},
        {"RACEC2R3", {BreakableProp, LightningCannon, UfoGrabber}},
        {"RACEC3R0", {WarpPad}},
    };

    for (const auto& [track, trackKinds] : expected) {
        std::vector<HazardKind> kinds;
        std::ranges::transform(RetailHazards(*archive, track), std::back_inserter(kinds),
                               &Hazard::kind);
        CHECK_MESSAGE(Sorted(kinds) == Sorted(trackKinds), track);
    }
}

TEST_CASE("the castle's barricade and cannon hold their retail values") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    CHECK(PayloadsOf<BreakableProp>(*archive, "RACEC0R1") ==
          std::vector<BreakableProp>{{.surface = "shootme",
                                      .entities = {"block", "barrel", "barrel01", "barrel02"},
                                      .trigger = 7}});
    CHECK(AllNear(PayloadsOf<CannonShot>(*archive, "RACEC0R1"),
                  {{.launch = {.x = 173.056F, .y = 350.142F, .z = 56.325F},
                    .target = Vector3{.x = 145.522F, .y = 629.248F, .z = 29.004F},
                    .triggerCentre = Vector3{.x = 160.187F, .y = 484.582F, .z = 23.528F},
                    .triggerRadius = 188.565F,
                    .event = 999}}));
}

std::vector<EventId> EventsOf(const std::vector<AimPoint>& points) {
    std::vector<EventId> events;
    std::ranges::transform(points, std::back_inserter(events), &AimPoint::event);
    return events;
}

TEST_CASE("the pirate ships aim through their launch and target points and fire one shot") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    constexpr Vector3 kLaunch{.x = 172.8F, .y = -405.8F, .z = -127.7F};
    std::vector<std::vector<EventId>> launches;
    std::vector<std::vector<EventId>> targets;
    std::vector<CannonShot> shots;
    for (const ShipCannons& ship : PayloadsOf<ShipCannons>(*archive, "RACEC2R1")) {
        launches.push_back(EventsOf(ship.launches));
        targets.push_back(EventsOf(ship.targets));
        shots.push_back(ship.shot);
    }

    CHECK(launches == std::vector<std::vector<EventId>>{{-1, -1}, {-1}});
    CHECK(targets ==
          std::vector<std::vector<EventId>>{{8, 8, 8, 8, 8, 50, 50}, {50, 50, 50, 8, 8, 8, 8, 8}});
    CHECK(AllNear(shots, {{.launch = kLaunch, .triggerRadius = 700.0F, .event = 96},
                          {.launch = kLaunch, .triggerRadius = 700.0F, .event = 97}}));
}

TEST_CASE("the water scrolls hold their retail amplitudes") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    CHECK(AllNear(PayloadsOf<WaterScroll>(*archive, "RACEC2R1"),
                  {{.entity = "pswater", .uAmplitude = 0.3F, .vAmplitude = 0.03F}}));
    CHECK(AllNear(PayloadsOf<WaterScroll>(*archive, "RACEC1R1"),
                  {{.entity = "water01", .uAmplitude = 0.2F}}));
}

TEST_CASE("the swinging boxes hold their entities, triggers, start frames and sizes") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    constexpr Vector3 kSize{.x = 40.0F, .y = 10.0F, .z = 40.0F};

    CHECK(PayloadsOf<SwingingBox>(*archive, "RACEC0R0") ==
          std::vector<SwingingBox>{
              {.entity = "rkhamm02", .trigger = 53, .size = {.x = 30.0F, .y = 12.0F, .z = 14.0F}}});
    CHECK(PayloadsOf<SwingingBox>(*archive, "RACEC2R2") ==
          std::vector<SwingingBox>{
              {.entity = "dum02", .trigger = 5, .startFrame = 100.0F, .size = kSize},
              {.entity = "dum03", .trigger = 6, .size = kSize},
              {.entity = "dum04", .trigger = 7, .startFrame = 150.0F, .size = kSize}});
}

TEST_CASE("the lightning cannons aim from an entity or a fixed source") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    CHECK(AllNear(PayloadsOf<LightningCannon>(*archive, "RACEC2R2"), {{.trigger = 9,
                                                                       .entity = "arktp",
                                                                       .period = Milliseconds{2000},
                                                                       .sweepRadians = 1.5708F,
                                                                       .verticalRange = -0.1F,
                                                                       .verticalBase = -0.4F}}));
    CHECK(AllNear(PayloadsOf<LightningCannon>(*archive, "RACEC2R3"),
                  {{.trigger = 0,
                    .source = Vector3{.x = 534.0F, .y = 370.6F, .z = 46.0F},
                    .period = Milliseconds{2000},
                    .sweepRadians = 6.2832F,
                    .verticalBase = -0.55F}}));
}

TEST_CASE("the space track's grabber and door and the dragon hold their retail values") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    CHECK(PayloadsOf<UfoGrabber>(*archive, "RACEC2R3") ==
          std::vector<UfoGrabber>{{.trigger = 1,
                                   .entity = "ufofly1",
                                   .pullStrength = 800.0F,
                                   .grabFrameLow = 85.0F,
                                   .grabFrameHigh = 200.0F}});
    CHECK(PayloadsOf<BreakableProp>(*archive, "RACEC2R3") ==
          std::vector<BreakableProp>{{.surface = "doorcol", .entities = {"door"}, .trigger = 2}});
    CHECK(PayloadsOf<DragonCannonballs>(*archive, "RACEC2R0") ==
          std::vector<DragonCannonballs>{
              {.entity = "prntdum", .trigger = 4, .interval = Milliseconds{1500}}});
}

}
}
