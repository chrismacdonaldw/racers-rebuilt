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

#include "app/ambient_sounds.hpp"
#include "app/car_sounds.hpp"
#include "app/hazard/hazard_sounds.hpp"
#include "app/loop_voice.hpp"
#include "app/positional_sound.hpp"
#include "app/powerup/powerup_sounds.hpp"
#include "app/race_music.hpp"
#include "app/scene.hpp"
#include "app/sound_banks.hpp"
#include "app/voice_reactions.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

/// `effects` is `GENERAL.SBK`, `ambient` the track's own bank and `voices` its circuit bank.
struct RaceSoundSet {
    std::span<const audio::SoundId> effects;
    std::span<const audio::SoundId> ambient;
    std::span<const audio::SoundId> voices;
    std::vector<std::string> music;
    std::vector<assets::SoundResource> ambience;
};

/// Only `player` gets engine and brake loops. `voices` are in the game's numbering.
struct RaceAudioSetup {
    std::size_t racerCount = 0;
    std::size_t player = 0;
    std::uint64_t seed = 0;
    std::span<const assets::SoundResource> ambience;
    std::span<const std::int32_t> voices;
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

    /// Call after each session tick. With several `cameras`, the nearest rival's engine is silent.
    void Hear(const sim::RaceSession& session, std::span<const Transform> cameras,
              Milliseconds elapsed);
    void Hear(const sim::RaceSession& session, const Transform& camera, Milliseconds elapsed) {
        Hear(session, std::span(&camera, 1), elapsed);
    }

    /// Silences every loop until the next `Hear`.
    void StopLoops();

    [[nodiscard]] SceneMusic Music() const { return music_.Current(); }

private:
    void PlayEvent(const sim::RaceSession& session, const sim::SoundEvent& event);
    void PlayPowerupEvent(const sim::SoundEvent& event, const Vector3& car);
    void PlayTurboEnd(sim::TurboLevel level, const Vector3& car);
    void PlayAt(std::size_t effect, const Vector3& source, const audio::VoiceMix& mix = {},
                SoundRange range = {});
    void PlayFlat(std::size_t effect);
    void HearEngine(const sim::RaceSession& session, Milliseconds elapsed);
    void HearSurfaces(const sim::RaceSession& session, Milliseconds elapsed);
    void HearShields(const sim::RaceSession& session);
    void HearNearestOpponent(const sim::RaceSession& session);

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
    std::vector<LoopVoice> shieldLoops_;
    LoopVoice opponentLoop_;
    AmbientSounds ambience_;
    HazardSounds hazards_;
    PowerupSounds powerups_;
    VoiceReactions reactions_;
};

}
