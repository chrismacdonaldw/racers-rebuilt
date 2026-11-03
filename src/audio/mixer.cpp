#include "audio/mixer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "audio/alp.hpp"
#include "audio/looping_stream.hpp"
#include "audio/peak_limiter.hpp"
#include "audio/voice.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"

#include <miniaudio.h>

namespace rr::audio {
namespace {

constexpr std::size_t kBusCount = 3;
constexpr std::size_t kEffectVoiceCount = 32;
constexpr ma_uint32 kChannelCount = 2;
constexpr float kHeadroomGain = 0.5F;
constexpr float kMinPitch = 0.01F;

VoiceMix FiniteMix(const VoiceMix& mix) {
    const VoiceMix fallback;
    const auto finite = [](float value, float otherwise) {
        return std::isfinite(value) ? value : otherwise;
    };
    return {.volume = std::max(finite(mix.volume, fallback.volume), 0.0F),
            .pitch = std::max(finite(mix.pitch, fallback.pitch), kMinPitch),
            .pan = std::clamp(finite(mix.pan, fallback.pan), -1.0F, 1.0F)};
}

struct LoadedSound {
    assets::FileData file;
    AlpSound sound;
};

std::atomic<std::uint64_t>& CounterOf(void* userData) {
    return *static_cast<std::atomic<std::uint64_t>*>(userData);
}

void* CountedMalloc(std::size_t size, void* userData) {
    CounterOf(userData).fetch_add(1, std::memory_order_relaxed);
    return ma_malloc(size, nullptr);
}

void* CountedRealloc(void* pointer, std::size_t size, void* userData) {
    CounterOf(userData).fetch_add(1, std::memory_order_relaxed);
    return ma_realloc(pointer, size, nullptr);
}

void Free(void* pointer, void* /*userData*/) {
    ma_free(pointer, nullptr);
}

std::size_t IndexOf(Bus bus) {
    return static_cast<std::size_t>(bus);
}

void DownmixToMono(std::span<float> interleavedStereo) noexcept {
    for (std::size_t sample = 0; sample + 1 < interleavedStereo.size(); sample += kChannelCount) {
        const float average = (interleavedStereo[sample] + interleavedStereo[sample + 1]) * 0.5F;
        interleavedStereo[sample] = average;
        interleavedStereo[sample + 1] = average;
    }
}

std::unexpected<Error> CantStart(ma_result result) {
    return Fail(ErrorCode::Platform,
                std::format("can't start audio: {}", ma_result_description(result)));
}

}

struct Mixer::State {
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;

    static void LimitMix(void* userData, float* frames, ma_uint64 frameCount) {
        auto* state = static_cast<State*>(userData);
        const std::span mix(frames, static_cast<std::size_t>(frameCount) * kChannelCount);
        if (!state->isStereo.load(std::memory_order_relaxed)) {
            DownmixToMono(mix);
        }
        if (std::optional<PeakLimiter>& limiter = state->limiter) {
            limiter->Process(mix);
        }
    }

    ~State() {
        for (Voice& voice : music) {
            voice.Stop();
        }
        for (Voice& voice : effects) {
            voice.Stop();
        }
        if (hasEngine) {
            ma_engine_uninit(&engine);
        }
    }

    std::atomic<std::uint64_t> allocationCount{0};
    std::atomic<bool> isStereo{true};
    ma_engine engine{};
    bool hasEngine = false;
    bool hasDevice = false;
    std::array<float, kBusCount> volumes{1.0F, 1.0F, 1.0F};
    std::array<Voice, kEffectVoiceCount> effects;
    std::array<Bus, kEffectVoiceCount> effectBuses{};
    std::array<std::uint32_t, kEffectVoiceCount> generations{};
    std::array<Voice, 2> music;
    std::size_t currentMusic = 0;
    std::vector<LoadedSound> sounds;
    std::optional<PeakLimiter> limiter;
};

Result<Mixer> Mixer::Create() {
    return Start(true, 0);
}

Result<Mixer> Mixer::CreateHeadless(std::uint32_t sampleRateHz) {
    return Start(false, sampleRateHz);
}

Result<Mixer> Mixer::Start(bool hasDevice, std::uint32_t sampleRateHz) {
    auto state = std::make_unique<State>();
    ma_engine_config config = ma_engine_config_init();
    config.channels = kChannelCount;
    config.sampleRate = sampleRateHz;
    config.noDevice = hasDevice ? MA_FALSE : MA_TRUE;
    config.noAutoStart = MA_TRUE;
    config.onProcess = State::LimitMix;
    config.pProcessUserData = state.get();
    config.allocationCallbacks = {.pUserData = &state->allocationCount,
                                  .onMalloc = CountedMalloc,
                                  .onRealloc = CountedRealloc,
                                  .onFree = Free};
    ma_result result = ma_engine_init(&config, &state->engine);
    if (result != MA_SUCCESS) {
        return CantStart(result);
    }
    state->hasEngine = true;
    state->hasDevice = hasDevice;
    state->limiter.emplace(ma_engine_get_sample_rate(&state->engine), kHeadroomGain);
    if (hasDevice) {
        result = ma_engine_start(&state->engine);
        if (result != MA_SUCCESS) {
            return CantStart(result);
        }
    }
    log::Info("audio mixing at {} Hz", ma_engine_get_sample_rate(&state->engine));
    return Mixer(std::move(state));
}

Mixer::Mixer(std::unique_ptr<State> state) : state_(std::move(state)) {}

Mixer::Mixer(Mixer&& other) noexcept = default;

Mixer& Mixer::operator=(Mixer&& other) noexcept = default;

Mixer::~Mixer() = default;

Result<SoundId> Mixer::AddSound(assets::FileData file) {
    Result<AlpSound> sound = ParseAlp(file.Bytes());
    if (!sound) {
        return std::unexpected(std::move(sound.error()));
    }
    const SoundId id{.index = static_cast<std::uint32_t>(state_->sounds.size())};
    state_->sounds.push_back({.file = std::move(file), .sound = *sound});
    return id;
}

void Mixer::SetVolume(Bus bus, float volume) {
    state_->volumes[IndexOf(bus)] = volume;
    if (bus == Bus::Music) {
        for (Voice& voice : state_->music) {
            voice.SetBusVolume(volume);
        }
    }
    for (std::size_t slot = 0; slot < kEffectVoiceCount; ++slot) {
        if (state_->effectBuses[slot] == bus) {
            state_->effects[slot].SetBusVolume(volume);
        }
    }
}

void Mixer::SetStereo(bool isStereo) {
    state_->isStereo.store(isStereo, std::memory_order_relaxed);
}

void Mixer::Play(std::span<const SoundCue> cues) {
    for (const SoundCue& cue : cues) {
        static_cast<void>(StartEffect(cue, false));
    }
}

std::optional<VoiceId> Mixer::Start(const SoundCue& cue) {
    return StartEffect(cue, false);
}

std::optional<VoiceId> Mixer::Loop(const SoundCue& cue) {
    return StartEffect(cue, true);
}

bool Mixer::IsPlaying(VoiceId voice) const {
    return IsCurrent(voice) && state_->effects.at(voice.slot).IsPlaying();
}

void Mixer::Mix(VoiceId voice, const VoiceMix& mix) {
    if (IsCurrent(voice)) {
        state_->effects.at(voice.slot).Mix(FiniteMix(mix));
    }
}

void Mixer::Stop(VoiceId voice) {
    if (IsCurrent(voice)) {
        state_->effects.at(voice.slot).Stop();
    }
}

std::optional<VoiceId> Mixer::StartEffect(const SoundCue& cue, bool isLooping) {
    RR_ASSERT(cue.sound.index < state_->sounds.size());
    const auto voice = std::ranges::find_if_not(state_->effects, &Voice::IsPlaying);
    if (voice == state_->effects.end()) {
        return std::nullopt;
    }
    std::unique_ptr<PcmStream> stream =
        std::make_unique<AlpDecoder>(state_->sounds[cue.sound.index].sound);
    if (isLooping) {
        stream = std::make_unique<LoopingStream>(std::move(stream), LoopPoints{});
    }
    const auto slot = static_cast<std::uint32_t>(voice - state_->effects.begin());
    const std::uint32_t generation = ++state_->generations.at(slot);
    state_->effectBuses.at(slot) = cue.bus;
    Result<void> started =
        voice->Start(state_->engine, std::move(stream),
                     {.mix = FiniteMix(cue.mix), .busVolume = state_->volumes[IndexOf(cue.bus)]});
    if (!started) {
        log::Warn("{}", started.error().message);
        return std::nullopt;
    }
    return VoiceId{.slot = slot, .generation = generation};
}

bool Mixer::IsCurrent(VoiceId voice) const {
    return voice.slot < kEffectVoiceCount && state_->generations.at(voice.slot) == voice.generation;
}

Result<void> Mixer::PlayMusic(std::unique_ptr<PcmStream> track, Milliseconds crossfade) {
    state_->music[state_->currentMusic].FadeOut(crossfade);
    state_->currentMusic = (state_->currentMusic + 1) % state_->music.size();
    return state_->music[state_->currentMusic].Start(
        state_->engine, std::move(track),
        {.busVolume = state_->volumes[IndexOf(Bus::Music)], .fadeIn = crossfade});
}

Result<void> Mixer::PlayMusic(SoundId track) {
    RR_ASSERT(track.index < state_->sounds.size());
    return PlayMusic(
        std::make_unique<LoopingStream>(
            std::make_unique<AlpDecoder>(state_->sounds[track.index].sound), LoopPoints{}),
        Milliseconds{0});
}

void Mixer::StopMusic(Milliseconds fade) {
    for (Voice& voice : state_->music) {
        voice.FadeOut(fade);
    }
}

bool Mixer::IsMusicPlaying() const {
    return state_->music[state_->currentMusic].IsPlaying();
}

bool Mixer::HasDevice() const {
    return state_->hasDevice;
}

std::optional<Milliseconds> Mixer::MusicTime() const {
    const Voice& music = state_->music[state_->currentMusic];
    return music.IsPlaying() ? std::optional(music.PlayedTime()) : std::nullopt;
}

void Mixer::Render(std::span<float> interleavedStereo) {
    ma_engine_read_pcm_frames(&state_->engine, interleavedStereo.data(),
                              interleavedStereo.size() / kChannelCount, nullptr);
}

std::uint64_t Mixer::AllocationCount() const {
    return state_->allocationCount.load(std::memory_order_relaxed);
}

}
