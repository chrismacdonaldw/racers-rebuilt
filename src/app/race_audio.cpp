#include "app/race_audio.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

#include "assets/event_table.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kEffectsBank = "GAMEDATA/COMMON/GENERAL.SBK";
constexpr std::uint64_t kAmbienceSeed = 0xA3B1E7C5D9F20468;
constexpr std::uint64_t kHazardSeed = 0x5C2E9B71F04D8A36;
constexpr std::uint64_t kPowerupSeed = 0x91D4F3A7C2E85B06;
constexpr std::uint64_t kVoiceSeed = 0x2F6B8D0E4A1C7395;
constexpr std::size_t kCountdownBeep = 0;
constexpr std::size_t kShieldAbsorb = 1;
constexpr std::size_t kBrakeStart = 2;
constexpr std::size_t kBlast = 5;
constexpr std::size_t kCannonFire = 6;
constexpr std::size_t kBrakeLoop = 3;
constexpr std::size_t kWhiteBrickBeforeFirst = 10;
constexpr std::size_t kGrappleHit = 18;
constexpr std::size_t kGrappleRelease = 20;
constexpr std::size_t kGrappleFire = 21;
constexpr std::size_t kGrappleMiss = 22;
constexpr std::size_t kBrickRespawn = 14;
constexpr std::size_t kDriveLoop = 10;
constexpr std::size_t kGo = 15;
constexpr std::size_t kCarScrape = 24;
constexpr std::size_t kMagnetDeploy = 33;
constexpr std::size_t kMagnetGrab = 35;
constexpr std::size_t kOilDrop = 46;
constexpr std::size_t kOilSlip = 47;
constexpr std::size_t kWallHit = 25;
constexpr std::size_t kIdleLoop = 32;
constexpr std::size_t kBrickCollect = 49;
constexpr std::size_t kMissileHit = 50;
constexpr std::size_t kMissileFire = 51;
constexpr std::size_t kTurboStart = 36;
constexpr std::size_t kWarpStart = 39;
constexpr std::size_t kTurboEnd = 40;
constexpr std::size_t kWarpLand = 41;
constexpr std::size_t kShieldLoop1 = 53;
constexpr std::size_t kCarScrapeAlternate = 55;
constexpr std::size_t kWallScrape = 56;
constexpr std::size_t kLightningEnd = 66;
constexpr std::size_t kLightningStrike = 67;
constexpr std::size_t kDriftStart = 57;
constexpr std::size_t kShieldExpire = 59;
constexpr std::size_t kCoastLoop = 61;
constexpr std::size_t kBrickSwap = 72;
constexpr std::size_t kGrappleRetract = 73;
constexpr std::size_t kEngineStart = 62;
constexpr std::size_t kTurboWhoosh = 65;
constexpr std::size_t kShieldAbsorbAlternate = 70;
constexpr std::size_t kMagnetFade = 74;
constexpr std::size_t kHoverTurboEnd = 75;
constexpr std::size_t kShieldLoop0 = 76;
constexpr std::size_t kShieldLoop2 = 77;
constexpr std::size_t kShieldLoop3 = 78;
constexpr std::size_t kOpponentEngine = 79;
constexpr std::size_t kEffectCount = kOpponentEngine + 1;
constexpr std::array kEngineLoopSounds{kIdleLoop, kDriveLoop, kCoastLoop};
constexpr std::array kShieldLoops{kShieldLoop0, kShieldLoop1, kShieldLoop2, kShieldLoop3};
constexpr float kWeakTurboEndVolume = 0.6F;
constexpr float kStrongTurboEndVolume = 0.8F;
constexpr float kOpponentEngineVolume = 0.8F;
constexpr SoundRange kOpponentRange{.min = 30.0F, .max = 200.0F};
constexpr SoundRange kBrickRange{.min = 30.0F, .max = 150.0F};
constexpr SoundRange kScrapeRange{.min = 30.0F, .max = 300.0F};
constexpr SoundRange kWarpRange{.min = 200.0F, .max = 600.0F};
constexpr SoundRange kShieldRange{.min = 200.0F, .max = 600.0F};
constexpr SoundRange kWeaponFireRange{.min = 30.0F, .max = 300.0F};
constexpr SoundRange kWeaponHitRange{.min = 200.0F, .max = 600.0F};
constexpr Vector3 kShieldExpireOffset{.z = 5.0F};

struct WeaponCue {
    std::size_t effect = 0;
    SoundRange range;
};

std::optional<WeaponCue> WeaponCueFor(sim::RaceSound sound) {
    switch (sound) {
        case sim::RaceSound::CannonFire:
            return WeaponCue{.effect = kCannonFire, .range = kWeaponFireRange};
        case sim::RaceSound::CannonHit:
        case sim::RaceSound::DynamiteBlast:
            return WeaponCue{.effect = kBlast, .range = kWeaponHitRange};
        case sim::RaceSound::MissileFire:
            return WeaponCue{.effect = kMissileFire, .range = kWeaponFireRange};
        case sim::RaceSound::MissileHit:
            return WeaponCue{.effect = kMissileHit, .range = kWeaponHitRange};
        case sim::RaceSound::OilDrop:
            return WeaponCue{.effect = kOilDrop, .range = kWeaponFireRange};
        case sim::RaceSound::OilSlip:
            return WeaponCue{.effect = kOilSlip, .range = kWeaponHitRange};
        case sim::RaceSound::MagnetDeploy:
            return WeaponCue{.effect = kMagnetDeploy, .range = kWeaponFireRange};
        case sim::RaceSound::MagnetGrab:
            return WeaponCue{.effect = kMagnetGrab, .range = kWeaponFireRange};
        case sim::RaceSound::MagnetFade:
            return WeaponCue{.effect = kMagnetFade, .range = kWeaponFireRange};
        case sim::RaceSound::GrappleFire:
            return WeaponCue{.effect = kGrappleFire, .range = kWeaponFireRange};
        case sim::RaceSound::GrappleHit:
            return WeaponCue{.effect = kGrappleHit, .range = kWeaponFireRange};
        case sim::RaceSound::GrappleRelease:
            return WeaponCue{.effect = kGrappleRelease, .range = kWeaponFireRange};
        case sim::RaceSound::LightningStrike:
            return WeaponCue{.effect = kLightningStrike, .range = kWeaponFireRange};
        case sim::RaceSound::LightningEnd:
            return WeaponCue{.effect = kLightningEnd, .range = kWeaponFireRange};
        default:
            return std::nullopt;
    }
}

std::size_t ShieldAbsorbSound(std::uint8_t variant) {
    return variant == 0 ? kShieldAbsorb : kShieldAbsorbAlternate + variant - 1;
}

std::vector<std::int32_t> Voices(const RaceAudioSetup& setup) {
    std::vector<std::int32_t> voices(setup.racerCount);
    for (std::size_t i = 0; i < voices.size(); ++i) {
        voices[i] = i < setup.voices.size() ? setup.voices[i] : DriverVoice(0);
    }
    return voices;
}

std::vector<std::optional<std::size_t>> Taunts(std::span<const std::int32_t> voices) {
    std::vector<std::optional<std::size_t>> taunts(voices.size());
    std::ranges::transform(voices, taunts.begin(), TauntSound);
    return taunts;
}

const Vector3& BrickAt(const sim::RaceSession& session, const sim::SoundEvent& event) {
    return session.Bricks().Coloured()[event.brick].brick.position;
}

std::span<const audio::SoundId> LoadOptionalBank(audio::Mixer& mixer, SoundBanks& banks,
                                                 const assets::Vfs& vfs, std::string_view path) {
    Result<std::span<const audio::SoundId>> bank = banks.Load(mixer, vfs, path);
    if (!bank) {
        log::Warn("{}, racing without it", bank.error().message);
        return {};
    }
    return *bank;
}

}

Result<std::unique_ptr<RaceAudio>> RaceAudio::Load(audio::Mixer& mixer, SoundBanks& banks,
                                                   const assets::Vfs& vfs, std::string_view track,
                                                   const RaceAudioSetup& setup) {
    Result<std::span<const audio::SoundId>> effects = banks.Load(mixer, vfs, kEffectsBank);
    if (effects && effects->size() < kEffectCount) {
        return Fail(ErrorCode::InvalidData, std::format("{} has {} sounds, wanted {}", kEffectsBank,
                                                        effects->size(), kEffectCount));
    }
    if (!effects) {
        return std::unexpected(std::move(effects.error()));
    }
    Result<std::vector<std::string>> music = LoadMusicList(vfs, track);
    if (!music) {
        log::Warn("{} music list: {}, racing without music", track, music.error().message);
    }
    return std::make_unique<RaceAudio>(
        mixer,
        RaceSoundSet{.effects = *effects,
                     .ambient = LoadOptionalBank(mixer, banks, vfs, AmbientBankPath(track)),
                     .voices = LoadCircuitVoices(mixer, banks, vfs, track),
                     .music = std::move(music).value_or(std::vector<std::string>{}),
                     .ambience = {setup.ambience.begin(), setup.ambience.end()}},
        setup);
}

RaceAudio::RaceAudio(audio::Mixer& mixer, RaceSoundSet sounds, const RaceAudioSetup& setup)
    : mixer_(&mixer),
      sounds_(std::move(sounds)),
      player_(setup.player),
      variants_(setup.seed),
      music_(std::move(sounds_.music), variants_.Next()),
      surfaces_(setup.racerCount),
      surfaceLoops_(setup.racerCount),
      shieldLoops_(setup.racerCount),
      ambience_(sounds_.ambience, sounds_.ambient, sim::Random(setup.seed ^ kAmbienceSeed)),
      hazards_(sounds_.ambient, sim::Random(setup.seed ^ kHazardSeed)),
      powerups_(sounds_.effects, Taunts(Voices(setup)), sim::Random(setup.seed ^ kPowerupSeed)),
      reactions_(sounds_.voices, Voices(setup), sim::Random(setup.seed ^ kVoiceSeed)) {
    RR_ASSERT(sounds_.effects.size() >= kEffectCount && setup.player < setup.racerCount);
}

RaceAudio::~RaceAudio() {
    StopLoops();
}

void RaceAudio::StopLoops() {
    for (LoopVoice& loop : engineLoops_) {
        loop.Stop(*mixer_);
    }
    for (LoopVoice& loop : surfaceLoops_) {
        loop.Stop(*mixer_);
    }
    for (LoopVoice& loop : shieldLoops_) {
        loop.Stop(*mixer_);
    }
    brakeLoop_.Stop(*mixer_);
    opponentLoop_.Stop(*mixer_);
    ambience_.Silence(*mixer_);
    hazards_.Silence(*mixer_);
    powerups_.Silence(*mixer_);
}

void RaceAudio::Hear(const sim::RaceSession& session, std::span<const Transform> cameras,
                     Milliseconds elapsed) {
    listener_ = ListenerAt(cameras);
    for (const sim::SoundEvent& event : session.Sounds()) {
        PlayEvent(session, event);
    }
    music_.Hear(session.Sounds(), player_, session.Progress().OfRacer(player_).finishPlace);
    HearEngine(session, elapsed);
    HearSurfaces(session, elapsed);
    HearShields(session);
    HearNearestOpponent(session);
    ambience_.Hear(session.Triggers().Events(), listener_, elapsed, *mixer_);
    hazards_.Hear(session, listener_, elapsed, *mixer_);
    powerups_.Hear(session, listener_, elapsed, *mixer_);
    reactions_.Hear(session, listener_, elapsed, *mixer_);
}

void RaceAudio::PlayEvent(const sim::RaceSession& session, const sim::SoundEvent& event) {
    const Vector3& car = session.Racers()[event.racer].Body().motion.position;
    const auto turboLevel = static_cast<std::size_t>(event.level);
    switch (event.sound) {
        case sim::RaceSound::CountdownBeep:
            PlayFlat(kCountdownBeep);
            break;
        case sim::RaceSound::Go:
            PlayFlat(kGo);
            break;
        case sim::RaceSound::EngineStart:
            PlayAt(kEngineStart, car);
            if (event.racer == player_) {
                engine_.Start();
            }
            break;
        case sim::RaceSound::TurboStart:
            PlayAt(kTurboStart + turboLevel, car);
            PlayAt(kTurboWhoosh, car);
            break;
        case sim::RaceSound::TurboFade:
            PlayTurboEnd(event.level, car);
            break;
        case sim::RaceSound::DriftStart:
            PlayAt(kDriftStart, car);
            break;
        case sim::RaceSound::WallHit:
            PlayAt(variants_.Below(2) == 0 ? kWallHit : kWallScrape, car);
            break;
        case sim::RaceSound::CarScrape:
            PlayAt(event.variant == 0 ? kCarScrape : kCarScrapeAlternate, car, {}, kScrapeRange);
            break;
        case sim::RaceSound::Finish:
            break;
        case sim::RaceSound::BrickRespawn:
            PlayAt(kBrickRespawn, BrickAt(session, event), {}, kBrickRange);
            break;
        case sim::RaceSound::BrickCollect:
            PlayAt(kBrickCollect, BrickAt(session, event), {}, kBrickRange);
            break;
        case sim::RaceSound::BrickSwap:
            PlayAt(kBrickSwap, BrickAt(session, event), {}, kBrickRange);
            break;
        case sim::RaceSound::WhiteBrickCollect:
            PlayAt(kWhiteBrickBeforeFirst + event.whiteCount, car);
            break;
        default:
            PlayPowerupEvent(event, car);
            break;
    }
}

void RaceAudio::PlayPowerupEvent(const sim::SoundEvent& event, const Vector3& car) {
    const bool isPlayer = event.racer == player_;
    switch (event.sound) {
        case sim::RaceSound::WarpEnter:
            if (isPlayer) {
                PlayFlat(kWarpStart);
            }
            PlayAt(kTurboWhoosh, car, {}, kWarpRange);
            break;
        case sim::RaceSound::WarpLand:
            if (isPlayer) {
                PlayFlat(kWarpLand);
            }
            break;
        case sim::RaceSound::ShieldAbsorb:
            PlayAt(ShieldAbsorbSound(event.variant), car, {}, kShieldRange);
            break;
        case sim::RaceSound::ShieldExpire:
            PlayAt(kShieldExpire, car + kShieldExpireOffset, {}, kShieldRange);
            break;
        case sim::RaceSound::GrappleMiss:
            PlayAt(kGrappleMiss, event.position, {}, kWeaponFireRange);
            PlayAt(kGrappleRetract, event.position, {}, kWeaponFireRange);
            break;
        default:
            if (const std::optional<WeaponCue> cue = WeaponCueFor(event.sound)) {
                PlayAt(cue->effect, event.position, {}, cue->range);
            }
            break;
    }
}

void RaceAudio::PlayTurboEnd(sim::TurboLevel level, const Vector3& car) {
    switch (level) {
        case sim::TurboLevel::Weak:
            PlayAt(kTurboEnd, car, {.volume = kWeakTurboEndVolume});
            break;
        case sim::TurboLevel::Strong:
            PlayAt(kTurboEnd, car, {.volume = kStrongTurboEndVolume});
            break;
        case sim::TurboLevel::Hover:
            PlayAt(kHoverTurboEnd, car);
            break;
    }
}

void RaceAudio::PlayAt(std::size_t effect, const Vector3& source, const audio::VoiceMix& mix,
                       SoundRange range) {
    const std::array cue{audio::SoundCue{.sound = sounds_.effects[effect],
                                         .mix = PlaceSound(listener_, source, mix, range)}};
    mixer_->Play(cue);
}

void RaceAudio::PlayFlat(std::size_t effect) {
    const std::array cue{audio::SoundCue{.sound = sounds_.effects[effect]}};
    mixer_->Play(cue);
}

void RaceAudio::HearEngine(const sim::RaceSession& session, Milliseconds elapsed) {
    const sim::Racer& racer = session.Racers()[player_];
    const sim::BodyState& body = racer.Body();
    const std::array mixes =
        engine_.Update({.thrust = body.thrust,
                        .hasTurbo = racer.ActiveTurbo().has_value(),
                        .speed = body.derived.speed,
                        .airborneMs = body.airborneMs,
                        .hasFinished = session.Progress().OfRacer(player_).finishPlace.has_value()},
                       elapsed);
    for (std::size_t i = 0; i < kEngineLoopCount; ++i) {
        if (mixes.at(i).volume > 0.0F) {
            engineLoops_.at(i).Play(*mixer_, sounds_.effects[kEngineLoopSounds.at(i)],
                                    PlaceSound(listener_, body.motion.position, mixes.at(i)));
        } else {
            engineLoops_.at(i).Stop(*mixer_);
        }
    }
    const BrakeSound brake = brakes_.Update(body);
    if (brake.hasStarted) {
        PlayAt(kBrakeStart, body.motion.position);
    }
    if (brake.loopPitch) {
        brakeLoop_.Play(*mixer_, sounds_.effects[kBrakeLoop],
                        PlaceSound(listener_, body.motion.position, {.pitch = *brake.loopPitch}));
    } else {
        brakeLoop_.Stop(*mixer_);
    }
}

void RaceAudio::HearSurfaces(const sim::RaceSession& session, Milliseconds elapsed) {
    for (std::size_t i = 0; i < surfaces_.size(); ++i) {
        const sim::BodyState& body = session.Racers()[i].Body();
        const std::optional<SurfaceLoop> loop = surfaces_[i].Update(body, elapsed);
        const std::optional<audio::SoundId> sound = loop.and_then([&](const SurfaceLoop& playing) {
            return AmbientSoundAt(sounds_.ambient, playing.soundId);
        });
        if (loop && sound) {
            surfaceLoops_[i].Play(*mixer_, *sound,
                                  PlaceSound(listener_, body.motion.position, loop->mix));
        } else {
            surfaceLoops_[i].Stop(*mixer_);
        }
    }
}

void RaceAudio::HearShields(const sim::RaceSession& session) {
    for (std::size_t i = 0; i < shieldLoops_.size(); ++i) {
        const sim::Racer& racer = session.Racers()[i];
        if (const std::optional<std::uint8_t>& level = racer.Status().shield.level) {
            shieldLoops_[i].Play(
                *mixer_, sounds_.effects[kShieldLoops.at(*level)],
                PlaceSound(listener_, racer.Body().motion.position, {}, kShieldRange));
        } else {
            shieldLoops_[i].Stop(*mixer_);
        }
    }
}

void RaceAudio::HearNearestOpponent(const sim::RaceSession& session) {
    const Vector3& player = session.Racers()[player_].Body().motion.position;
    const sim::BodyState* nearest = nullptr;
    float nearestDistance = kOpponentRange.max * kOpponentRange.max;
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        const sim::BodyState& body = session.Racers()[i].Body();
        const Vector3 offset = body.motion.position - player;
        if (i != player_ && Dot(offset, offset) < nearestDistance) {
            nearest = &body;
            nearestDistance = Dot(offset, offset);
        }
    }
    if (nearest == nullptr || !session.Clock().IsRunning() || listener_.earCount > 1) {
        opponentLoop_.Stop(*mixer_);
        return;
    }
    const float pitch = OpponentEnginePitch(nearest->derived.speed, nearest->airborneMs, 1.0F);
    opponentLoop_.Play(
        *mixer_, sounds_.effects[kOpponentEngine],
        PlaceSound(listener_, nearest->motion.position,
                   {.volume = kOpponentEngineVolume, .pitch = pitch}, kOpponentRange));
}

}
