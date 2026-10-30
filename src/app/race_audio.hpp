#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/car_sounds.hpp"
#include "app/positional_sound.hpp"
#include "app/race_music.hpp"
#include "app/scene.hpp"
#include "app/sound_banks.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

/// `effects` is `GENERAL.SBK` and `ambient` the track's own bank, both in bank order.
struct RaceSoundSet {
    std::span<const audio::SoundId> effects;
    std::span<const audio::SoundId> ambient;
    std::vector<std::string> music;
};

/// Only `player` gets engine and brake loops.
struct RaceAudioSetup {
    std::size_t racerCount = 0;
    std::size_t player = 0;
    std::uint64_t seed = 0;
};

/// Only reads the session, so it never changes a race's outcome.
class RaceAudio {
public:
    /// Fails only without the effects bank. `mixer` must outlive the audio.
    [[nodiscard]] static Result<std::unique_ptr<RaceAudio>> Load(audio::Mixer& mixer,
                                                                 SoundBanks& banks,
                                                                 const assets::Vfs& vfs,
                                                                 std::string_view track,
                                                                 const RaceAudioSetup& setup);

    RaceAudio(audio::Mixer& mixer, RaceSoundSet sounds, const RaceAudioSetup& setup);
    RaceAudio(const RaceAudio&) = delete;
    RaceAudio& operator=(const RaceAudio&) = delete;
    RaceAudio(RaceAudio&&) = delete;
    RaceAudio& operator=(RaceAudio&&) = delete;
    ~RaceAudio();

    /// Call after each session tick. `camera` is the player's, in engine space.
    void Hear(const sim::RaceSession& session, const Transform& camera, Milliseconds elapsed);

    /// Silences the engine, brake, surface and opponent loops until the next `Hear`.
    void StopLoops();

    [[nodiscard]] SceneMusic Music() const { return music_.Current(); }

private:
    class LoopVoice {
    public:
        void Play(audio::Mixer& mixer, audio::SoundId sound, const audio::VoiceMix& mix);
        void Stop(audio::Mixer& mixer);

    private:
        std::optional<audio::VoiceId> voice_;
        audio::SoundId sound_;
    };

    void PlayEvent(const sim::RaceSession& session, const sim::SoundEvent& event);
    void PlayAt(std::size_t effect, const Vector3& source, const audio::VoiceMix& mix = {});
    void PlayFlat(std::size_t effect);
    void HearEngine(const sim::RaceSession& session, Milliseconds elapsed);
    void HearSurfaces(const sim::RaceSession& session, Milliseconds elapsed);
    void HearNearestOpponent(const sim::RaceSession& session);
    [[nodiscard]] std::optional<audio::SoundId> AmbientSound(std::int32_t soundId) const;

    audio::Mixer* mixer_;
    RaceSoundSet sounds_;
    std::size_t player_;
    sim::Random variants_;
    RaceMusic music_;
    Listener listener_;
    EngineSound engine_;
    std::array<LoopVoice, kEngineLoopCount> engineLoops_;
    Brakes brakes_;
    LoopVoice brakeLoop_;
    std::vector<SurfaceSound> surfaces_;
    std::vector<LoopVoice> surfaceLoops_;
    LoopVoice opponentLoop_;
};

}
