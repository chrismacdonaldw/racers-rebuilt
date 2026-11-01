#include "assets/track_triggers.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kEntryField = 0x27;
constexpr FieldId kRandomField = 0x2B;

constexpr std::array kEnvironmentFields{
    Field<&TriggerSphere::centre>(0x29),
    Field<&TriggerSphere::radius>(0x2A),
    Field<&TriggerSphere::event>(0x2B),
    Field<&TriggerSphere::isPlayerOnly>(0x2F),
};

constexpr std::array kRacerFields{
    Field<&TriggerSphere::centre>(0x29),
    Field<&TriggerSphere::radius>(0x2A),
    Field<&TriggerSphere::event>(0x2B),
    Field<&TriggerSphere::isPlayerOnly>(0x2F),
    Field<&TriggerSphere::isForProjectiles>(0x2C),
    Field<&TriggerSphere::collisionWorld>(0x2D),
    Field<&TriggerSphere::lap>(0x2E),
};

Result<void> ReadPhase(TokenReader& reader, TimerPhase& phase) {
    Result<Token> next = reader.Peek();
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    phase.isRandom = next->kind == TokenKind::Field && next->field == kRandomField;
    if (phase.isRandom) {
        return reader.Next().and_then(
            [&](const Token&) { return ReadValue(reader, phase.length); });
    }
    return ReadValue(reader, phase.length);
}

template <auto Phase>
constexpr FieldRule<TrackTimer> PhaseField(FieldId id) {
    return {.id = id, .read = [](TokenReader& reader, TrackTimer& timer) {
                return ReadPhase(reader, timer.*Phase);
            }};
}

constexpr std::array kTimerFields{
    PhaseField<&TrackTimer::on>(0x28),
    PhaseField<&TrackTimer::off>(0x29),
    Field<&TrackTimer::event>(0x2A),
    Field<&TrackTimer::startDelay>(0x2D),
};

template <typename Entry>
Result<std::vector<Entry>> ParseEntryList(std::span<const std::byte> bytes,
                                          std::span<const FieldRule<Entry>> rules) {
    TokenReader reader(bytes);
    std::vector<Entry> entries;
    const auto readEntry = [&] {
        return ExpectField(reader, kEntryField).and_then([&] {
            return ReadBlock<Entry>(reader, rules, entries.emplace_back());
        });
    };
    return ExpectField(reader, kEntryField)
        .and_then([&] { return ReadEntryList(reader, readEntry); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(entries); });
}

bool HasNegativeTime(const TrackTimer& timer) {
    return timer.on.length.count() < 0 || timer.off.length.count() < 0 ||
           timer.startDelay.count() < 0;
}

}

Result<std::vector<TriggerSphere>> ParseTriggerSpheres(std::span<const std::byte> bytes,
                                                       TriggerList list) {
    const std::span<const FieldRule<TriggerSphere>> rules =
        list == TriggerList::Racer ? std::span<const FieldRule<TriggerSphere>>(kRacerFields)
                                   : std::span<const FieldRule<TriggerSphere>>(kEnvironmentFields);
    return ParseEntryList<TriggerSphere>(bytes, rules);
}

Result<std::vector<TrackTimer>> ParseTrackTimers(std::span<const std::byte> bytes) {
    return ParseEntryList<TrackTimer>(bytes, kTimerFields)
        .and_then([](std::vector<TrackTimer>&& timers) -> Result<std::vector<TrackTimer>> {
            const auto negative = std::ranges::find_if(timers, HasNegativeTime);
            if (negative != timers.end()) {
                return Fail(ErrorCode::InvalidData,
                            std::format("timer for event {} has a negative time", negative->event));
            }
            return std::move(timers);
        });
}

}
