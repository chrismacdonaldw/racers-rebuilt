#include "app/powerup/powerup_sounds.hpp"

#include <algorithm>
#include <utility>

#include "app/track_space.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/grapple_effect.hpp"
#include "sim/powerup/lightning_effect.hpp"
#include "sim/powerup/magnet_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kCannonballFlight = 7;
constexpr std::size_t kCurseLoop = 9;
constexpr std::size_t kFuseLoop = 0x11;
constexpr std::size_t kGrappleAttached = 0x13;
constexpr std::size_t kGrappleFlight = 0x17;
constexpr std::size_t kPlayerTaunt = 0x1A;
constexpr std::size_t kMagnetLoop = 0x22;
constexpr std::size_t kOilLoop = 0x30;
constexpr std::size_t kMissileFlight = 0x34;
constexpr std::size_t kSpinLoop = 0x3C;
constexpr std::size_t kLightningLoop = 0x44;
constexpr std::size_t kLightningCrackle = 0x45;
constexpr std::array<std::size_t, 6> kOpponentTaunts{0x1C, 0x1B, 0x1D, 0x1E, 0x1F, 0x1F};
constexpr std::array kFlightLoops{kCannonballFlight, kMissileFlight, kGrappleFlight,
                                  kGrappleAttached};
constexpr SoundRange kFlightRange{.min = 200.0F, .max = 500.0F};
constexpr float kLightningFadePerMs = 0.0002F;
constexpr Milliseconds kLightningFade{500};
constexpr Milliseconds kShortestCrackleWait{200};
constexpr std::uint32_t kCrackleWaitSteps = 300;
constexpr std::uint32_t kCrackleReachSteps = 50;

bool IsPastDrop(const sim::EffectProgress& progress) {
    return progress.phase >= static_cast<std::uint8_t>(sim::TrapPhase::Armed);
}

bool IsPhase(const sim::EffectProgress& progress, auto phase) {
    return progress.phase == static_cast<std::uint8_t>(phase);
}

float LightningVolume(const sim::EffectProgress& progress) {
    const auto left = static_cast<float>(progress.phaseTime.count());
    if (IsPhase(progress, sim::LightningPhase::RampIn)) {
        return 1.0F - (left * kLightningFadePerMs);
    }
    if (IsPhase(progress, sim::LightningPhase::Fade)) {
        return 1.0F - ((static_cast<float>(kLightningFade.count()) - left) * kLightningFadePerMs);
    }
    return 1.0F;
}

std::optional<std::size_t> FlightIndex(std::size_t effect) {
    const auto found = std::ranges::find(kFlightLoops, effect);
    if (found == kFlightLoops.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - kFlightLoops.begin());
}

}

std::optional<std::size_t> TauntSound(bool isPlayer, std::int32_t voiceBank) {
    if (isPlayer) {
        return kPlayerTaunt;
    }
    if (voiceBank < 0 || std::cmp_greater_equal(voiceBank, kOpponentTaunts.size())) {
        return std::nullopt;
    }
    return kOpponentTaunts.at(static_cast<std::size_t>(voiceBank));
}

std::optional<EffectLoop> LoopOf(sim::PowerupKind kind, const sim::EffectProgress& progress) {
    switch (kind) {
        case sim::PowerupKind::OilSlick:
            return IsPastDrop(progress) ? std::optional(EffectLoop{.effect = kOilLoop})
                                        : std::nullopt;
        case sim::PowerupKind::Dynamite:
            return IsPastDrop(progress) ? std::optional(EffectLoop{.effect = kFuseLoop})
                                        : std::nullopt;
        case sim::PowerupKind::Curse:
            return IsPastDrop(progress) ? std::optional(EffectLoop{.effect = kCurseLoop})
                                        : std::nullopt;
        case sim::PowerupKind::Magnet:
            return EffectLoop{.effect = kMagnetLoop};
        case sim::PowerupKind::Lightning:
            return EffectLoop{.effect = kLightningLoop, .volume = LightningVolume(progress)};
        default:
            return std::nullopt;
    }
}

std::optional<std::size_t> FlightLoopOf(sim::PowerupKind kind,
                                        const sim::EffectProgress& progress) {
    switch (kind) {
        case sim::PowerupKind::Cannonball:
            return IsPhase(progress, sim::ShotPhase::Flying) ? std::optional(kCannonballFlight)
                                                             : std::nullopt;
        case sim::PowerupKind::Missile:
            return IsPhase(progress, sim::ShotPhase::Flying) ? std::optional(kMissileFlight)
                                                             : std::nullopt;
        case sim::PowerupKind::GrapplingHook:
            if (IsPhase(progress, sim::GrapplePhase::Flying)) {
                return kGrappleFlight;
            }
            return IsPhase(progress, sim::GrapplePhase::Pulling) ? std::optional(kGrappleAttached)
                                                                 : std::nullopt;
        default:
            return std::nullopt;
    }
}

std::optional<float> Crackle::Tick(bool isSustained, Milliseconds elapsed, sim::Random& random) {
    if (!isSustained) {
        return std::nullopt;
    }
    if (wait_ >= elapsed) {
        wait_ -= elapsed;
        return std::nullopt;
    }
    const auto reach = static_cast<float>(random.Below(sim::kRollRange) % kCrackleReachSteps);
    wait_ = kShortestCrackleWait +
            Milliseconds{static_cast<int>(random.Below(sim::kRollRange) % kCrackleWaitSteps)};
    return reach;
}

PowerupSounds::PowerupSounds(std::span<const audio::SoundId> effects,
                             std::vector<std::optional<std::size_t>> taunts, sim::Random random)
    : effects_(effects), taunts_(std::move(taunts)), random_(random), spins_(taunts_.size()) {}

void PowerupSounds::Hear(const sim::RaceSession& session, const Listener& listener,
                         Milliseconds elapsed, audio::Mixer& mixer) {
    HearTaunts(session, listener, mixer);
    HearEffects(session, listener, elapsed, mixer);
    HearFlights(session, listener, mixer);
    HearSpins(session, listener, mixer);
}

void PowerupSounds::Silence(audio::Mixer& mixer) {
    for (SlotSound& slot : slots_) {
        slot.loop.Stop(mixer);
    }
    for (LoopVoice& flight : flights_) {
        flight.Stop(mixer);
    }
    for (LoopVoice& spin : spins_) {
        spin.Stop(mixer);
    }
}

void PowerupSounds::HearTaunts(const sim::RaceSession& session, const Listener& listener,
                               audio::Mixer& mixer) const {
    for (const sim::SoundEvent& event : session.Sounds()) {
        if (event.sound != sim::RaceSound::Taunt || event.racer >= taunts_.size()) {
            continue;
        }
        if (const std::optional<std::size_t> taunt = taunts_[event.racer]) {
            PlayOnce(*taunt, session.Racers()[event.racer].Body().motion.position, listener, mixer);
        }
    }
}

void PowerupSounds::HearEffects(const sim::RaceSession& session, const Listener& listener,
                                Milliseconds elapsed, audio::Mixer& mixer) {
    const std::span<const sim::EffectSlot> effects = session.Powerups().Slots();
    slots_.resize(effects.size());
    for (std::size_t i = 0; i < effects.size(); ++i) {
        const sim::EffectSlot& effect = effects[i];
        SlotSound& sound = slots_[i];
        const sim::EffectProgress progress =
            effect.isActive ? effect.effect->Progress() : sim::EffectProgress{};
        const std::optional<EffectLoop> loop =
            effect.isActive ? LoopOf(effect.kind, progress) : std::nullopt;
        if (!loop) {
            sound.loop.Stop(mixer);
            continue;
        }
        Source source{.position = effect.effect->Pose().position};
        if (effect.kind == sim::PowerupKind::Lightning) {
            if (effect.use.hazard) {
                const sim::HazardAim& aim = session.TrackHazards().Aims()[*effect.use.hazard];
                source = {.position = aim.source, .forward = aim.direction};
            } else {
                const sim::BodyMotion& owner = session.Racers()[effect.use.racer].Body().motion;
                source = {.position = owner.position, .forward = owner.basis.forward};
            }
            const bool isSustained = IsPhase(progress, sim::LightningPhase::Sustain);
            if (const std::optional<float> reach =
                    sound.crackle.Tick(isSustained, elapsed, random_)) {
                PlayOnce(kLightningCrackle, source.position + (source.forward * *reach), listener,
                         mixer);
            }
        }
        sound.loop.Play(mixer, effects_[loop->effect],
                        PlaceSound(listener, source.position, {.volume = loop->volume}));
    }
}

void PowerupSounds::HearFlights(const sim::RaceSession& session, const Listener& listener,
                                audio::Mixer& mixer) {
    const Vector3 ear = ToGame(listener.position);
    std::array<std::optional<Vector3>, kFlightLoops.size()> nearest{};
    std::array<float, kFlightLoops.size()> nearestDistance{};
    nearestDistance.fill(kFlightRange.max * kFlightRange.max);
    for (const std::uint16_t slot : session.Powerups().Active()) {
        const sim::EffectSlot& effect = session.Powerups().Slots()[slot];
        const std::optional<std::size_t> flight =
            FlightLoopOf(effect.kind, effect.effect->Progress()).and_then(FlightIndex);
        if (!flight) {
            continue;
        }
        const Vector3 position = effect.effect->Pose().position;
        const Vector3 offset = position - ear;
        if (Dot(offset, offset) < nearestDistance.at(*flight)) {
            nearestDistance.at(*flight) = Dot(offset, offset);
            nearest.at(*flight) = position;
        }
    }
    for (std::size_t i = 0; i < kFlightLoops.size(); ++i) {
        if (nearest.at(i)) {
            flights_.at(i).Play(mixer, effects_[kFlightLoops.at(i)],
                                PlaceSound(listener, *nearest.at(i), {}, kFlightRange));
        } else {
            flights_.at(i).Stop(mixer);
        }
    }
}

void PowerupSounds::HearSpins(const sim::RaceSession& session, const Listener& listener,
                              audio::Mixer& mixer) {
    for (std::size_t i = 0; i < spins_.size() && i < session.Racers().size(); ++i) {
        const sim::Racer& racer = session.Racers()[i];
        if (!racer.Route() && racer.IsSpinning()) {
            spins_[i].Play(mixer, effects_[kSpinLoop],
                           PlaceSound(listener, racer.Body().motion.position, {}));
        } else {
            spins_[i].Stop(mixer);
        }
    }
}

void PowerupSounds::PlayOnce(std::size_t effect, const Vector3& source, const Listener& listener,
                             audio::Mixer& mixer) const {
    const std::array cue{
        audio::SoundCue{.sound = effects_[effect], .mix = PlaceSound(listener, source, {})}};
    mixer.Play(cue);
}

}
