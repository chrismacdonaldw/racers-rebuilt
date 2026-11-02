#include "sim/replay/replay.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "sim/replay/replay_bytes.hpp"
#include "sim/replay/replay_recorder.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kTicksPerHour = 60 * 60 * 1000 / 12;
constexpr std::size_t kVersionOffset = 4;
constexpr std::size_t kChecksumBytes = 8;

InputFrame Held(std::initializer_list<Action> actions) {
    InputFrame frame;
    for (const Action action : actions) {
        frame.players[0].held.set(PlayerInput::Index(action));
    }
    return frame;
}

Replay SampleReplay() {
    InputFrame tap = Held({Action::Accelerate});
    tap.players[0].pressed.set(PlayerInput::Index(Action::Accelerate));
    std::vector<InputFrame> frames(30, InputFrame{});
    frames.push_back(tap);
    frames.insert(frames.end(), 50, Held({Action::Accelerate}));
    frames.insert(frames.end(), 20, Held({Action::Accelerate, Action::SteerLeft}));
    InputFrame stick = Held({Action::Accelerate});
    stick.players[0].steerLeftAxis = -90;
    frames.insert(frames.end(), 10, stick);
    InputFrame second = Held({Action::Accelerate});
    second.players[1].held.set(PlayerInput::Index(Action::Brake));
    second.players[1].steerLeftAxis = 64;
    frames.insert(frames.end(), 5, second);
    InputFrame menu;
    menu.menuPressed.set(InputFrame::Index(MenuAction::Confirm));
    frames.push_back(menu);
    return {.buildVersion = "0.1.0",
            .scene = "RACEC0R0",
            .seed = 0x1234'5678'9ABC'DEF0ULL,
            .gameplay = {"race.example=true", "mod:example@1.0.0"},
            .frames = frames};
}

std::vector<std::byte> Resealed(std::vector<std::byte> bytes) {
    bytes.resize(bytes.size() - kChecksumBytes);
    AppendChecksum(bytes);
    return bytes;
}

std::vector<std::byte> TwoLongRuns() {
    constexpr std::uint32_t kRuns = 2;
    constexpr std::uint32_t kTicksPerRun = 5'000'000;
    constexpr std::size_t kBytesPerFrame = (4 * kMaxPlayers) + 1;
    std::vector<std::byte> bytes;
    AppendHeader(bytes, "RRRP", kReplayFormatVersion);
    AppendText(bytes, "");
    AppendText(bytes, "");
    AppendU64(bytes, 0);
    AppendU8(bytes, 0);
    AppendU32(bytes, kRuns);
    for (std::uint32_t run = 0; run < kRuns; ++run) {
        AppendU32(bytes, kTicksPerRun);
        bytes.insert(bytes.end(), kBytesPerFrame, std::byte{0});
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
    std::vector<InputFrame>& frames = replay.frames;
    constexpr std::size_t kTicksPerChange = 17;
    for (std::size_t tick = 0; tick < kTicksPerHour; ++tick) {
        const std::size_t step = tick / kTicksPerChange;
        InputFrame frame = Held({Action::Accelerate});
        frame.players[0].held.set(
            PlayerInput::Index(step % 2 == 0 ? Action::SteerLeft : Action::SteerRight));
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
    SUBCASE("runs that add up to too many ticks") {
        CHECK_FALSE(DecodeReplay(TwoLongRuns()).has_value());
    }
    SUBCASE("bytes after the last run") {
        std::vector<std::byte> bytes = good;
        bytes.insert(bytes.end() - kChecksumBytes, std::byte{0});
        CHECK_FALSE(DecodeReplay(Resealed(bytes)).has_value());
    }
}

TEST_CASE("recording keeps menu presses") {
    ReplayRecorder recorder("0.1.0", "RACEC0R0", 0);
    InputFrame frame = Held({Action::Accelerate});
    frame.menuPressed.set(InputFrame::Index(MenuAction::Confirm));

    recorder.Record(frame, RenderSnapshot{});

    CHECK(recorder.Recorded().frames.front() == frame);
}

}
}
