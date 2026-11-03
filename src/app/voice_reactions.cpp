#include "app/voice_reactions.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/race_voices.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::int32_t kFirstCircuitVoice = 1100;
constexpr std::string_view kTimeTrialVoices = "voice29";
constexpr std::int32_t kLinesPerDriver = 12;
constexpr std::int32_t kBuiltVoicePerSlot = 100;
constexpr std::size_t kCircuitDrivers = 6;
constexpr std::uint32_t kLinesPerMood = 6;
constexpr Milliseconds kShortestQuiet{5000};
constexpr std::int64_t kQuietPerRoll = 4;
constexpr SoundRange kReactionRange{.min = 100.0F, .max = 400.0F};

std::optional<std::string> CircuitVoicesName(const assets::Vfs& vfs, std::string_view track) {
    const std::string path = std::format("GAMEDATA/{}/{}.RAB", track, track);
    const Result<assets::FileData> rab = vfs.Read(path);
    Result<std::optional<std::string>> name =
        rab ? assets::ReadOpponentVoices(rab->Bytes()) : std::unexpected(rab.error());
    if (!name) {
        log::Warn("{}: {}, racing without voices", path, name.error().message);
        return std::nullopt;
    }
    return std::move(*name);
}

std::span<const audio::SoundId> LoadVoices(audio::Mixer& mixer, SoundBanks& banks,
                                           const assets::Vfs& vfs,
                                           const std::optional<std::string>& name) {
    if (!name) {
        return {};
    }
    const std::string path = std::format("GAMEDATA/VOICES/{}.SBK", assets::ToUpperAscii(*name));
    Result<std::span<const audio::SoundId>> bank = banks.Load(mixer, vfs, path);
    if (!bank) {
        log::Warn("{}, racing without those voices", bank.error().message);
        return {};
    }
    return *bank;
}

std::optional<std::string> PlayerVoicesName(std::optional<std::uint8_t> builtFace) {
    return builtFace.transform([](std::uint8_t face) { return std::format("voice{:02}", face); });
}

}

std::int32_t DriverVoice(std::int32_t voiceBank) {
    return kFirstCircuitVoice + (voiceBank * kLinesPerDriver);
}

std::int32_t BuiltCarVoice(std::size_t slot) {
    return kBuiltCarVoice + (static_cast<std::int32_t>(slot) * kBuiltVoicePerSlot);
}

std::optional<std::size_t> CircuitDriver(std::int32_t voice) {
    const std::int32_t line = voice - kFirstCircuitVoice;
    if (line < 0 || line % kLinesPerDriver != 0) {
        return std::nullopt;
    }
    const auto driver = static_cast<std::size_t>(line / kLinesPerDriver);
    return driver < kCircuitDrivers ? std::optional(driver) : std::nullopt;
}

VoiceBanks LoadRaceVoices(audio::Mixer& mixer, SoundBanks& banks, const assets::Vfs& vfs,
                          std::string_view track, VoiceChoice choice) {
    std::optional<std::string> others = PlayerVoicesName(choice.secondBuiltFace);
    if (!others) {
        others = choice.isTimeTrial ? std::string(kTimeTrialVoices) : CircuitVoicesName(vfs, track);
    }
    return {.player = LoadVoices(mixer, banks, vfs, PlayerVoicesName(choice.builtFace)),
            .others = LoadVoices(mixer, banks, vfs, others)};
}

std::optional<audio::SoundId> VoiceSound(const VoiceBanks& banks, std::int32_t voice) {
    const bool isPlayer = voice < kFirstCircuitVoice;
    const std::int32_t index = voice - (isPlayer ? kBuiltCarVoice : kFirstCircuitVoice);
    const std::span<const audio::SoundId> bank = isPlayer ? banks.player : banks.others;
    if (index < 0 || static_cast<std::size_t>(index) >= bank.size()) {
        return std::nullopt;
    }
    return bank[static_cast<std::size_t>(index)];
}

ReactionTimers::ReactionTimers(std::size_t racerCount, sim::Random random)
    : random_(random), quiet_(racerCount) {
    for (Milliseconds& quiet : quiet_) {
        quiet = Roll();
    }
}

void ReactionTimers::Tick(Milliseconds elapsed) {
    for (Milliseconds& quiet : quiet_) {
        quiet = std::max(quiet - elapsed, Milliseconds{0});
    }
}

std::optional<std::size_t> ReactionTimers::React(std::size_t racer, sim::VoiceReaction reaction) {
    if (quiet_.at(racer) > Milliseconds{0}) {
        return std::nullopt;
    }
    const std::uint32_t mood = reaction == sim::VoiceReaction::Positive ? kLinesPerMood : 0;
    const std::uint32_t line = (random_.Below(sim::kRollRange) % kLinesPerMood) + mood;
    quiet_[racer] = Roll();
    return line;
}

Milliseconds ReactionTimers::Roll() {
    return kShortestQuiet + Milliseconds{random_.Below(sim::kRollRange) * kQuietPerRoll};
}

VoiceReactions::VoiceReactions(VoiceBanks banks, std::vector<std::int32_t> voices,
                               sim::Random random)
    : banks_(banks), voices_(std::move(voices)), timers_(voices_.size(), random) {}

void VoiceReactions::Hear(const sim::RaceSession& session, const Listener& listener,
                          Milliseconds elapsed, audio::Mixer& mixer) {
    timers_.Tick(elapsed);
    for (const sim::SoundEvent& event : session.Sounds()) {
        const bool isPositive = event.sound == sim::RaceSound::PositiveReaction;
        if ((!isPositive && event.sound != sim::RaceSound::NegativeReaction) ||
            event.racer >= voices_.size()) {
            continue;
        }
        const std::optional<std::size_t> line = timers_.React(
            event.racer, isPositive ? sim::VoiceReaction::Positive : sim::VoiceReaction::Negative);
        const std::optional<audio::SoundId> sound = line.and_then([&](std::size_t said) {
            return VoiceSound(banks_, voices_[event.racer] + static_cast<std::int32_t>(said));
        });
        if (!sound) {
            continue;
        }
        const std::array cue{audio::SoundCue{
            .sound = *sound,
            .mix = PlaceSound(listener, session.Racers()[event.racer].Body().motion.position, {},
                              kReactionRange)}};
        mixer.Play(cue);
    }
}

}
