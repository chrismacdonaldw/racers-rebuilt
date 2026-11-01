#include "sim/replay/replay.hpp"

#include <bit>
#include <format>
#include <limits>
#include <utility>

#include "core/assert.hpp"
#include "sim/replay/replay_bytes.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kMagic = "RRRP";
constexpr std::string_view kWhat = "replay";
constexpr std::size_t kMaxTicks = 8'000'000;
constexpr unsigned long kActionMask = (1UL << kActionCount) - 1;
constexpr unsigned long kMenuActionMask = (1UL << kMenuActionCount) - 1;

struct Run {
    std::uint32_t length = 0;
    InputFrame frame;
};

std::vector<Run> ToRuns(const std::vector<InputFrame>& frames) {
    std::vector<Run> runs;
    for (const InputFrame& frame : frames) {
        if (!runs.empty() && runs.back().frame == frame &&
            runs.back().length < std::numeric_limits<std::uint32_t>::max()) {
            ++runs.back().length;
        } else {
            runs.push_back({.length = 1, .frame = frame});
        }
    }
    return runs;
}

void AppendStream(std::vector<std::byte>& out, const std::vector<InputFrame>& frames) {
    const std::vector<Run> runs = ToRuns(frames);
    AppendU32(out, static_cast<std::uint32_t>(runs.size()));
    for (const Run& run : runs) {
        AppendU32(out, run.length);
        AppendU8(out, static_cast<std::uint8_t>(run.frame.held.to_ulong()));
        AppendU8(out, static_cast<std::uint8_t>(run.frame.pressed.to_ulong()));
        AppendU8(out, static_cast<std::uint8_t>(run.frame.released.to_ulong()));
        AppendU8(out, std::bit_cast<std::uint8_t>(run.frame.steerLeftAxis));
        AppendU8(out, static_cast<std::uint8_t>(run.frame.menuPressed.to_ulong()));
    }
}

Result<std::bitset<kActionCount>> ReadActions(assets::ByteReader& reader) {
    return reader.ReadU8().and_then([](std::uint8_t bits) -> Result<std::bitset<kActionCount>> {
        if ((bits & ~kActionMask) != 0) {
            return Fail(ErrorCode::InvalidData, "replay names an unknown action");
        }
        return std::bitset<kActionCount>(bits);
    });
}

Result<Run> ReadRun(assets::ByteReader& reader) {
    Run run;
    Result<std::uint32_t> length = reader.ReadU32();
    if (!length || *length == 0) {
        return Fail(ErrorCode::InvalidData, "replay has an empty input run");
    }
    run.length = *length;
    for (std::bitset<kActionCount>* actions :
         {&run.frame.held, &run.frame.pressed, &run.frame.released}) {
        Result<std::bitset<kActionCount>> read = ReadActions(reader);
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
        *actions = *read;
    }
    Result<std::uint8_t> axis = reader.ReadU8();
    if (!axis) {
        return std::unexpected(std::move(axis.error()));
    }
    run.frame.steerLeftAxis = std::bit_cast<std::int8_t>(*axis);
    Result<std::uint8_t> menu = reader.ReadU8();
    if (!menu) {
        return std::unexpected(std::move(menu.error()));
    }
    if ((*menu & ~kMenuActionMask) != 0) {
        return Fail(ErrorCode::InvalidData, "replay names an unknown menu action");
    }
    run.frame.menuPressed = std::bitset<kMenuActionCount>(*menu);
    return run;
}

Result<std::vector<InputFrame>> ReadStream(assets::ByteReader& reader, std::size_t& ticksLeft) {
    Result<std::uint32_t> runCount = reader.ReadU32();
    if (!runCount) {
        return std::unexpected(std::move(runCount.error()));
    }
    std::vector<InputFrame> frames;
    for (std::uint32_t i = 0; i < *runCount; ++i) {
        Result<Run> run = ReadRun(reader);
        if (!run) {
            return std::unexpected(std::move(run.error()));
        }
        if (run->length > ticksLeft) {
            return Fail(ErrorCode::InvalidData, "replay is longer than a day of ticks");
        }
        ticksLeft -= run->length;
        frames.insert(frames.end(), run->length, run->frame);
    }
    return frames;
}

Result<std::vector<std::string>> ReadGameplay(assets::ByteReader& reader) {
    Result<std::uint8_t> count = reader.ReadU8();
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    std::vector<std::string> entries;
    for (std::uint8_t i = 0; i < *count; ++i) {
        Result<std::string> entry = ReadText(reader);
        if (!entry) {
            return std::unexpected(std::move(entry.error()));
        }
        entries.push_back(std::move(*entry));
    }
    return entries;
}

Result<Replay> ReadBody(assets::ByteReader& reader) {
    Replay replay;
    Result<std::string> buildVersion = ReadText(reader);
    Result<std::string> scene =
        buildVersion.and_then([&](const auto&) { return ReadText(reader); });
    Result<std::uint64_t> seed = scene.and_then([&](const auto&) { return ReadU64(reader); });
    Result<std::vector<std::string>> gameplay =
        seed.and_then([&](auto) { return ReadGameplay(reader); });
    Result<std::uint8_t> racerCount =
        gameplay.and_then([&](const auto&) { return reader.ReadU8(); });
    if (!racerCount) {
        return std::unexpected(std::move(racerCount.error()));
    }
    replay.buildVersion = std::move(*buildVersion);
    replay.scene = std::move(*scene);
    replay.seed = *seed;
    replay.gameplay = std::move(*gameplay);
    std::size_t ticksLeft = kMaxTicks;
    for (std::uint8_t i = 0; i < *racerCount; ++i) {
        Result<std::vector<InputFrame>> stream = ReadStream(reader, ticksLeft);
        if (!stream) {
            return std::unexpected(std::move(stream.error()));
        }
        replay.racers.push_back(std::move(*stream));
    }
    return replay;
}

}

std::vector<std::byte> EncodeReplay(const Replay& replay) {
    std::vector<std::byte> out;
    AppendHeader(out, kMagic, kReplayFormatVersion);
    AppendText(out, replay.buildVersion);
    AppendText(out, replay.scene);
    AppendU64(out, replay.seed);
    RR_VERIFY(replay.gameplay.size() <= kMaxGameplayEntries);
    AppendU8(out, static_cast<std::uint8_t>(replay.gameplay.size()));
    for (const std::string& entry : replay.gameplay) {
        AppendText(out, entry);
    }
    AppendU8(out, static_cast<std::uint8_t>(replay.racers.size()));
    for (const std::vector<InputFrame>& frames : replay.racers) {
        AppendStream(out, frames);
    }
    AppendChecksum(out);
    return out;
}

Result<Replay> DecodeReplay(std::span<const std::byte> bytes) {
    Result<assets::ByteReader> reader = OpenChecked(bytes, kMagic, kReplayFormatVersion, kWhat);
    Result<Replay> replay =
        reader.and_then([](assets::ByteReader& body) { return ReadBody(body); });
    if (replay && reader->Remaining() != 0) {
        return Fail(ErrorCode::InvalidData,
                    std::format("replay has {} bytes after its last stream", reader->Remaining()));
    }
    return replay;
}

}
