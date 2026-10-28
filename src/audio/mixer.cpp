#include "audio/mixer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <format>
#include <utility>
#include <vector>

#include "audio/alp.hpp"
#include "audio/looping_stream.hpp"
#include "audio/voice.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"

#include <miniaudio.h>

namespace rr::audio {
namespace {

constexpr std::size_t kBusCount = 3;
constexpr std::size_t kEffectVoiceCount = 32;
constexpr ma_uint32 kChannelCount = 2;

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

}

struct Mixer::State {
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;

    ~State() {
        for (Voice& voice : music) {
            voice.Stop();
        }
        for (Voice& voice : effects) {
            voice.Stop();
        }
        std::for_each_n(buses.begin(), busCount,
                        [](ma_sound_group& bus) { ma_sound_group_uninit(&bus); });
        if (hasEngine) {
            ma_engine_uninit(&engine);
        }
    }

    std::atomic<std::uint64_t> allocationCount{0};
    ma_engine engine{};
    bool hasEngine = false;
    std::array<ma_sound_group, kBusCount> buses{};
    std::size_t busCount = 0;
    std::array<Voice, kEffectVoiceCount> effects;
    std::array<Voice, 2> music;
    std::size_t currentMusic = 0;
    std::vector<LoadedSound> sounds;
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
    config.allocationCallbacks = {.pUserData = &state->allocationCount,
                                  .onMalloc = CountedMalloc,
                                  .onRealloc = CountedRealloc,
                                  .onFree = Free};
    ma_result result = ma_engine_init(&config, &state->engine);
    if (result != MA_SUCCESS) {
        return Fail(ErrorCode::Platform,
                    std::format("can't start audio: {}", ma_result_description(result)));
    }
    state->hasEngine = true;
    for (ma_sound_group& bus : state->buses) {
        result = ma_sound_group_init(&state->engine,
                                     MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
                                     nullptr, &bus);
        if (result != MA_SUCCESS) {
            return Fail(ErrorCode::Platform, std::format("can't create an audio bus: {}",
                                                         ma_result_description(result)));
        }
        ++state->busCount;
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
    ma_sound_group_set_volume(&state_->buses[IndexOf(bus)], volume);
}

void Mixer::Play(std::span<const SoundCue> cues) {
    for (const SoundCue& cue : cues) {
        RR_ASSERT(cue.sound.index < state_->sounds.size());
        const auto voice = std::ranges::find_if_not(state_->effects, &Voice::IsPlaying);
        if (voice == state_->effects.end()) {
            return;
        }
        Result<void> started =
            voice->Start(state_->engine, state_->buses[IndexOf(cue.bus)],
                         std::make_unique<AlpDecoder>(state_->sounds[cue.sound.index].sound),
                         {.volume = cue.volume});
        if (!started) {
            log::Warn("{}", started.error().message);
        }
    }
}

Result<void> Mixer::PlayMusic(std::unique_ptr<PcmStream> track, Milliseconds crossfade) {
    state_->music[state_->currentMusic].FadeOut(crossfade);
    state_->currentMusic = (state_->currentMusic + 1) % state_->music.size();
    return state_->music[state_->currentMusic].Start(state_->engine,
                                                     state_->buses[IndexOf(Bus::Music)],
                                                     std::move(track), {.fadeIn = crossfade});
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

void Mixer::Render(std::span<float> interleavedStereo) {
    ma_engine_read_pcm_frames(&state_->engine, interleavedStereo.data(),
                              interleavedStereo.size() / kChannelCount, nullptr);
}

std::uint64_t Mixer::AllocationCount() const {
    return state_->allocationCount.load(std::memory_order_relaxed);
}

}
