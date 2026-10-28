#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include "assets/vfs_layer.hpp"
#include "audio/pcm_stream.hpp"
#include "core/result.hpp"

namespace rr::audio {

enum class Bus : std::uint8_t { Music, Effects, Voice };

struct SoundId {
    std::uint32_t index = 0;
};

struct SoundCue {
    SoundId sound;
    Bus bus = Bus::Effects;
    float volume = 1.0F;
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

    /// Loops `track` in place of the current one.
    [[nodiscard]] Result<void> PlayMusic(std::unique_ptr<PcmStream> track);
    [[nodiscard]] Result<void> PlayMusic(SoundId track);
    void StopMusic();

    void Render(std::span<float> interleavedStereo);

    /// Includes allocations on the audio thread.
    [[nodiscard]] std::uint64_t AllocationCount() const;

private:
    struct State;

    explicit Mixer(std::unique_ptr<State> state);

    [[nodiscard]] static Result<Mixer> Start(bool hasDevice, std::uint32_t sampleRateHz);

    std::unique_ptr<State> state_;
};

}
