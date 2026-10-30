#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "assets/vfs_layer.hpp"
#include "audio/pcm_stream.hpp"
#include "audio/voice_mix.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::audio {

enum class Bus : std::uint8_t { Music, Effects, Voice };

struct SoundId {
    std::uint32_t index = 0;
};

struct SoundCue {
    SoundId sound;
    Bus bus = Bus::Effects;
    VoiceMix mix;
};

/// Goes stale once its voice stops, so it never changes a later voice.
struct VoiceId {
    std::uint32_t slot = 0;
    std::uint32_t generation = 0;
};

/// Mixes on miniaudio's audio thread. Call members from the main thread only.
class Mixer {
public:
    [[nodiscard]] static Result<Mixer> Create();

    /// Mixes stereo at `sampleRateHz` only when `Render` pulls frames.
    [[nodiscard]] static Result<Mixer> CreateHeadless(std::uint32_t sampleRateHz);

    Mixer(Mixer&& other) noexcept;
    Mixer& operator=(Mixer&& other) noexcept;
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;
    ~Mixer();

    /// Keeps `file` until the mixer is destroyed.
    [[nodiscard]] Result<SoundId> AddSound(assets::FileData file);

    void SetVolume(Bus bus, float volume);

    /// Drops cues once every effect voice is busy.
    void Play(std::span<const SoundCue> cues);

    /// Plays `cue` on repeat until `Stop`. Empty once every effect voice is busy.
    [[nodiscard]] std::optional<VoiceId> Loop(const SoundCue& cue);

    /// Does nothing to a voice that has stopped.
    void Mix(VoiceId voice, const VoiceMix& mix);
    void Stop(VoiceId voice);

    [[nodiscard]] Result<void> PlayMusic(std::unique_ptr<PcmStream> track, Milliseconds crossfade);
    /// Loops a sound from `AddSound` as music, with no crossfade.
    [[nodiscard]] Result<void> PlayMusic(SoundId track);
    void StopMusic(Milliseconds fade = Milliseconds{0});

    void Render(std::span<float> interleavedStereo);

    /// Includes allocations on the audio thread.
    [[nodiscard]] std::uint64_t AllocationCount() const;

private:
    struct State;

    explicit Mixer(std::unique_ptr<State> state);

    [[nodiscard]] static Result<Mixer> Start(bool hasDevice, std::uint32_t sampleRateHz);

    std::optional<VoiceId> StartEffect(const SoundCue& cue, bool isLooping);
    [[nodiscard]] bool IsCurrent(VoiceId voice) const;

    std::unique_ptr<State> state_;
};

}
