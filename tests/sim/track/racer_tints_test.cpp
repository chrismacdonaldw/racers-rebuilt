#include "sim/track/racer_tints.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::sim {
namespace {

constexpr assets::EventId kInside = 0;
constexpr assets::EventId kOutside = 1;

assets::EventTableAsset Tunnel() {
    assets::EventTableAsset table;
    table.colorTransforms.push_back(
        {.trigger = {.event = kInside, .ignoresEnd = true}, .shifts = {1, 1, 1, 0}});
    table.colorTransforms.push_back(
        {.trigger = {.event = kOutside, .ignoresEnd = true}, .clears = true});
    return table;
}

TrackEvent For(assets::EventId id, EventEdge edge, std::uint8_t racer) {
    return {.id = id, .edge = edge, .racer = racer};
}

TEST_CASE("a racer entering is tinted until it reaches the clearing event") {
    const assets::EventTableAsset table = Tunnel();
    RacerTints tints(table, 2);

    tints.Apply(std::vector{For(kInside, EventEdge::Start, 1), For(kInside, EventEdge::End, 1)});
    CHECK_FALSE(tints.Of(0).has_value());
    CHECK(tints.Of(1) == 0);

    tints.Apply(std::vector{For(kOutside, EventEdge::Start, 1)});
    CHECK_FALSE(tints.Of(1).has_value());
}

TEST_CASE("position-path events tint nobody") {
    const assets::EventTableAsset table = Tunnel();
    RacerTints tints(table, 1);

    tints.Apply(std::vector<TrackEvent>{{.id = kInside, .edge = EventEdge::Start}});

    CHECK_FALSE(tints.Of(0).has_value());
}

TEST_CASE("a tint without the no-end flag clears on its end edge") {
    assets::EventTableAsset table;
    table.colorTransforms.push_back({.trigger = {.event = kInside}, .offsets = {32, 0, 0, 0}});
    RacerTints tints(table, 1);

    tints.Apply(std::vector{For(kInside, EventEdge::Start, 0)});
    CHECK(tints.Of(0) == 0);
    tints.Apply(std::vector{For(kInside, EventEdge::End, 0)});
    CHECK_FALSE(tints.Of(0).has_value());
}

TEST_CASE("a tint that starts on the end edge applies when the racer leaves") {
    assets::EventTableAsset table;
    table.colorTransforms.push_back(
        {.trigger = {.event = kInside, .startsOnEnd = true, .ignoresEnd = true}});
    RacerTints tints(table, 1);

    tints.Apply(std::vector{For(kInside, EventEdge::Start, 0)});
    CHECK_FALSE(tints.Of(0).has_value());
    tints.Apply(std::vector{For(kInside, EventEdge::End, 0)});
    CHECK(tints.Of(0) == 0);
}

}
}
