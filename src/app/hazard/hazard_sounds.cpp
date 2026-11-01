#include "app/hazard/hazard_sounds.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <variant>

#include "app/ambient_sounds.hpp"

namespace rr::app {
namespace {

constexpr std::int32_t kGhostLoop = 3012;
constexpr Milliseconds kGhostLoopLag{300};
constexpr Milliseconds kWobblePeriod{500};
constexpr std::uint32_t kDepthSteps = 100;
constexpr float kDepthPerStep = 0.004F;
constexpr std::int32_t kLavaLoop = 3002;

}

HazardSounds::HazardSounds(std::span<const audio::SoundId> bank, sim::Random random)
    : bank_(bank), random_(random) {}

void HazardSounds::Hear(const sim::RaceSession& session, const Listener& listener,
                        Milliseconds elapsed, audio::Mixer& mixer) {
    for (const sim::HazardSound& sound : session.TrackHazards().Sounds()) {
        if (const std::optional<audio::SoundId> id = AmbientSoundAt(bank_, sound.soundId)) {
            const std::array cue{
                audio::SoundCue{.sound = *id, .mix = PlaceSound(listener, sound.position, {})}};
            mixer.Play(cue);
        }
    }
    const std::span<const sim::HazardSlot> hazards = session.TrackHazards().Slots();
    loops_.resize(hazards.size());
    for (std::size_t i = 0; i < hazards.size(); ++i) {
        if (hazards[i].phase == sim::HazardPhase::Active) {
            Loop(loops_[i], session, hazards[i].state, listener, elapsed, mixer);
        } else {
            loops_[i].voice.Stop(mixer);
        }
    }
}

void HazardSounds::Silence(audio::Mixer& mixer) {
    for (SlotLoop& loop : loops_) {
        loop.voice.Stop(mixer);
    }
}

void HazardSounds::Loop(SlotLoop& loop, const sim::RaceSession& session,
                        const sim::HazardState& state, const Listener& listener,
                        Milliseconds elapsed, audio::Mixer& mixer) {
    const sim::TrackEntities& entities = session.Entities();
    if (const auto* ghost = std::get_if<sim::Ghost>(&state); ghost != nullptr && ghost->entity) {
        const Vector3 tail =
            entities.NodeWorldBefore(*ghost->entity, sim::kGhostBodyNode, kGhostLoopLag).position;
        Play(kGhostLoop, loop, tail, Wobble(loop, elapsed), listener, mixer);
    } else if (const auto* blob = std::get_if<sim::LavaBlob>(&state);
               blob != nullptr && blob->entity && blob->isBubbling) {
        Play(kLavaLoop, loop, entities.NodeWorld(*blob->entity, sim::kLavaBlobNode).position, 1.0F,
             listener, mixer);
    } else {
        loop.voice.Stop(mixer);
    }
}

float HazardSounds::Wobble(SlotLoop& loop, Milliseconds elapsed) {
    loop.wobble += elapsed;
    if (loop.wobble >= kWobblePeriod) {
        loop.wobble = Milliseconds{0};
        loop.depth =
            static_cast<float>(random_.Below(sim::kRollRange) % kDepthSteps) * kDepthPerStep;
    }
    const float share =
        static_cast<float>(loop.wobble.count()) / static_cast<float>(kWobblePeriod.count());
    return 1.0F - (loop.depth * std::sin(std::numbers::pi_v<float> * share));
}

void HazardSounds::Play(std::int32_t soundId, SlotLoop& loop, const Vector3& source, float pitch,
                        const Listener& listener, audio::Mixer& mixer) const {
    if (const std::optional<audio::SoundId> id = AmbientSoundAt(bank_, soundId)) {
        loop.voice.Play(mixer, *id, PlaceSound(listener, source, {.pitch = pitch}));
    }
}

}
