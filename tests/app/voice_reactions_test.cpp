#include "app/voice_reactions.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>

namespace rr::app {
namespace {

constexpr Milliseconds kLongestQuiet{5000 + (1023 * 4)};
constexpr Milliseconds kShortestQuiet{5000};
constexpr std::uint64_t kSeed = 7;

ReactionTimers Unmuted(std::size_t racerCount) {
    ReactionTimers timers(racerCount, sim::Random(kSeed));
    timers.Tick(kLongestQuiet);
    return timers;
}

std::size_t LineOf(ReactionTimers& timers, sim::VoiceReaction reaction) {
    const std::optional<std::size_t> line = timers.React(0, reaction);
    timers.Tick(kLongestQuiet);
    REQUIRE(line);
    return *line;
}

}

TEST_CASE("prebuilt drivers speak with one of the circuit bank's six drivers") {
    CHECK(DriverVoice(0) == 1100);
    CHECK(DriverVoice(5) == 1160);
    CHECK(CircuitDriver(DriverVoice(3)) == 3);
    CHECK_FALSE(CircuitDriver(kBuiltCarVoice));
    CHECK_FALSE(CircuitDriver(DriverVoice(6)));
    CHECK_FALSE(CircuitDriver(DriverVoice(2) + 1));
}

TEST_CASE("built players speak from their own bank and the rest from the others'") {
    std::array<audio::SoundId, 12> player{};
    std::array<audio::SoundId, 72> others{};
    player[11] = {.index = 3};
    others[25] = {.index = 7};
    const VoiceBanks banks{.player = player, .others = others};
    const auto index = [&](std::int32_t voice) {
        return VoiceSound(banks, voice).transform([](audio::SoundId sound) { return sound.index; });
    };

    CHECK(index(BuiltCarVoice(0) + 11) == 3U);
    CHECK(index(DriverVoice(2) + 1) == 7U);
    CHECK_FALSE(VoiceSound(banks, BuiltCarVoice(0) + 12));
    CHECK_FALSE(VoiceSound(banks, DriverVoice(6)));
    CHECK_FALSE(VoiceSound(banks, BuiltCarVoice(0) - 1));
}

TEST_CASE("racers stay quiet for the first 5 s of a race") {
    ReactionTimers timers(2, sim::Random(kSeed));
    timers.Tick(kShortestQuiet - Milliseconds{1});

    CHECK_FALSE(timers.React(0, sim::VoiceReaction::Positive));
    CHECK_FALSE(timers.React(1, sim::VoiceReaction::Negative));
}

TEST_CASE("a reaction quiets only its own racer for 5 to 9 s") {
    ReactionTimers timers = Unmuted(2);

    REQUIRE(timers.React(0, sim::VoiceReaction::Positive));
    timers.Tick(kShortestQuiet - Milliseconds{1});
    CHECK_FALSE(timers.React(0, sim::VoiceReaction::Positive));
    CHECK(timers.React(1, sim::VoiceReaction::Positive));
    timers.Tick(kLongestQuiet);
    CHECK(timers.React(0, sim::VoiceReaction::Negative));
}

TEST_CASE("negative reactions say the first six lines and positive ones the last six") {
    ReactionTimers timers = Unmuted(1);
    for (int i = 0; i < 50; ++i) {
        CHECK(LineOf(timers, sim::VoiceReaction::Negative) < 6);
        const std::size_t positive = LineOf(timers, sim::VoiceReaction::Positive);
        CHECK(positive >= 6);
        CHECK(positive < 12);
    }
}

}
