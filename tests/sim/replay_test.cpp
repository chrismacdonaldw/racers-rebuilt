#include "sim/replay/replay.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "sim/replay/replay_bytes.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kTicksPerHour = 60 * 60 * 1000 / 12;
constexpr std::size_t kVersionOffset = 4;
constexpr std::size_t kChecksumBytes = 8;

InputFrame Held(std::initializer_list<Action> actions) {
    InputFrame frame;
    for (const Action action : actions) {
        frame.held.set(InputFrame::Index(action));
    }
    return frame;
}

Replay SampleReplay() {
    InputFrame tap = Held({Action::Accelerate});
    tap.pressed.set(InputFrame::Index(Action::Accelerate));
    std::vector<InputFrame> frames(30, InputFrame{});
    frames.push_back(tap);
    frames.insert(frames.end(), 50, Held({Action::Accelerate}));
    frames.insert(frames.end(), 20, Held({Action::Accelerate, Action::SteerLeft}));
    InputFrame stick = Held({Action::Accelerate});
    stick.steerLeftAxis = -90;
    frames.insert(frames.end(), 10, stick);
    return {.buildVersion = "0.1.0",
            .scene = "RACEC0R0",
            .seed = 0x1234'5678'9ABC'DEF0ULL,
            .racers = {frames}};
}

std::vector<std::byte> Resealed(std::vector<std::byte> bytes) {
    bytes.resize(bytes.size() - kChecksumBytes);
    AppendChecksum(bytes);
    return bytes;
}

std::vector<std::byte> TwoLongStreams() {
    constexpr std::uint8_t kRacers = 2;
    constexpr std::uint32_t kTicksPerRacer = 5'000'000;
    std::vector<std::byte> bytes;
    AppendHeader(bytes, "RRRP", kReplayFormatVersion);
    AppendText(bytes, "");
    AppendText(bytes, "");
    AppendU64(bytes, 0);
    AppendU8(bytes, kRacers);
    for (std::uint8_t racer = 0; racer < kRacers; ++racer) {
        AppendU32(bytes, 1);
        AppendU32(bytes, kTicksPerRacer);
        bytes.insert(bytes.end(), 4, std::byte{0});
    }
    AppendChecksum(bytes);
    return bytes;
}

TEST_CASE("a replay survives encoding") {
    const Replay replay = SampleReplay();

    const Result<Replay> decoded = DecodeReplay(EncodeReplay(replay));

    REQUIRE(decoded.has_value());
    CHECK(*decoded == replay);
}

TEST_CASE("an hour of input fits in under 1 MB") {
    Replay replay{.buildVersion = "0.1.0"};
    std::vector<InputFrame>& frames = replay.racers.emplace_back();
    constexpr std::size_t kTicksPerChange = 17;
    for (std::size_t tick = 0; tick < kTicksPerHour; ++tick) {
        const std::size_t step = tick / kTicksPerChange;
        InputFrame frame = Held({Action::Accelerate});
        frame.held.set(InputFrame::Index(step % 2 == 0 ? Action::SteerLeft : Action::SteerRight));
        frames.push_back(frame);
    }

    const std::vector<std::byte> bytes = EncodeReplay(replay);

    CHECK(bytes.size() < std::size_t{1024} * 1024);
    CHECK(DecodeReplay(bytes) == replay);
}

TEST_CASE("a damaged replay is rejected") {
    const std::vector<std::byte> good = EncodeReplay(SampleReplay());

    SUBCASE("wrong magic") {
        std::vector<std::byte> bytes = good;
        bytes[0] = std::byte{'X'};
        CHECK_FALSE(DecodeReplay(bytes).has_value());
    }
    SUBCASE("truncated") {
        CHECK_FALSE(DecodeReplay(std::span(good).first(good.size() - 3)).has_value());
        CHECK_FALSE(DecodeReplay(std::span(good).first(5)).has_value());
    }
    SUBCASE("a flipped bit fails the checksum") {
        std::vector<std::byte> bytes = good;
        bytes[good.size() / 2] ^= std::byte{0x10};
        CHECK_FALSE(DecodeReplay(bytes).has_value());
    }
    SUBCASE("a newer format version") {
        std::vector<std::byte> bytes = good;
        bytes[kVersionOffset] = std::byte{kReplayFormatVersion + 1};
        CHECK_FALSE(DecodeReplay(Resealed(bytes)).has_value());
    }
    SUBCASE("an unknown action") {
        std::vector<std::byte> bytes = good;
        bytes[good.size() - kChecksumBytes - 4] = std::byte{0xFF};
        CHECK_FALSE(DecodeReplay(Resealed(bytes)).has_value());
    }
    SUBCASE("streams that add up to too many ticks") {
        CHECK_FALSE(DecodeReplay(TwoLongStreams()).has_value());
    }
    SUBCASE("bytes after the last stream") {
        std::vector<std::byte> bytes = good;
        bytes.insert(bytes.end() - kChecksumBytes, std::byte{0});
        CHECK_FALSE(DecodeReplay(Resealed(bytes)).has_value());
    }
}

}
}
