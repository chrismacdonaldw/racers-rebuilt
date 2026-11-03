#include "app/menu/preview_cycle.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>

#include "app/frame_loop.hpp"

namespace rr::app {
namespace {

struct Switch {
    Milliseconds at{0};
    std::size_t race = 0;

    friend bool operator==(const Switch&, const Switch&) = default;
};

constexpr Switch kNoSwitch{.at = Milliseconds{-1}};

std::optional<Switch> RunUntilSwitch(PreviewCycle& cycle, Milliseconds limit) {
    for (Milliseconds played{0}; played < limit;) {
        played += kSimTick;
        if (const std::optional<std::size_t> race = cycle.Advance(kSimTick)) {
            return Switch{.at = played, .race = *race};
        }
    }
    return std::nullopt;
}

TEST_CASE("a circuit preview fades in, then moves on to the next race after 2,000 ms") {
    PreviewCycle cycle;
    cycle.Restart({.fades = true, .cycles = true});
    cycle.Shown(true);
    CHECK(cycle.FadeAlpha() == doctest::Approx(1.0F));

    const Switch first = RunUntilSwitch(cycle, Milliseconds{5000}).value_or(kNoSwitch);

    CHECK(first == Switch{.at = Milliseconds{2004}, .race = 1});
    CHECK(cycle.FadeAlpha() > 0.9F);
}

TEST_CASE("a circuit preview fades to black over the last 250 ms before a switch") {
    PreviewCycle cycle;
    cycle.Restart({.fades = true, .cycles = true});
    cycle.Shown(true);
    for (Milliseconds played{0}; played < Milliseconds{1500}; played += kSimTick) {
        static_cast<void>(cycle.Advance(kSimTick));
    }
    CHECK(cycle.FadeAlpha() == 0.0F);

    for (Milliseconds played{0}; played < Milliseconds{372}; played += kSimTick) {
        static_cast<void>(cycle.Advance(kSimTick));
    }

    CHECK(cycle.FadeAlpha() > 0.0F);
    CHECK(cycle.FadeAlpha() < 1.0F);
}

TEST_CASE("the circuit preview wraps from its fourth race to its first") {
    PreviewCycle cycle;
    cycle.Restart({.fades = true, .cycles = true});
    std::size_t race = 0;
    for (int shown = 0; shown < 4; ++shown) {
        cycle.Shown(true);
        race = RunUntilSwitch(cycle, Milliseconds{5000}).value_or(kNoSwitch).race;
    }

    CHECK(race == 0);
}

TEST_CASE("a race whose preview doesn't change frame moves on at the next update") {
    PreviewCycle cycle;
    cycle.Restart({.fades = true, .cycles = true});
    cycle.Shown(false);

    const Switch next = RunUntilSwitch(cycle, kSimTick).value_or(kNoSwitch);

    CHECK(next == Switch{.at = kSimTick, .race = 1});
}

TEST_CASE("the final circuit's preview fades in but stays on its race") {
    PreviewCycle cycle;
    cycle.Restart({.fades = true, .cycles = false});
    cycle.Shown(true);

    CHECK_FALSE(RunUntilSwitch(cycle, Milliseconds{10000}).has_value());
    CHECK(cycle.FadeAlpha() == 0.0F);
}

TEST_CASE("a single race preview neither fades nor moves on") {
    PreviewCycle cycle;
    cycle.Restart({});
    cycle.Shown(true);

    CHECK(cycle.FadeAlpha() == 0.0F);
    CHECK_FALSE(RunUntilSwitch(cycle, Milliseconds{10000}).has_value());
}

}
}
