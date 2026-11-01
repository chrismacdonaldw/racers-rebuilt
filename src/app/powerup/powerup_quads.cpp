#include "app/powerup/powerup_quads.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "app/frame_loop.hpp"
#include "app/powerup/beam_span.hpp"
#include "app/powerup/visual_random.hpp"
#include "app/track_space.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/grapple_effect.hpp"
#include "sim/powerup/lightning_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::app {
namespace {

using sim::PowerupKind;

constexpr std::string_view kCannonballLook = "CANNON";
constexpr std::string_view kCannonballTrailLook = "CANSTRK";
constexpr std::string_view kMissileTrailLook = "MSLSTRK";
constexpr std::string_view kRopeLook = "TETHER";
constexpr std::string_view kBeamLook = "LIGHTNG";
constexpr std::string_view kBeamFlashLook = "LTFLASH";
constexpr std::string_view kFlashLook = "EXFLASH";
constexpr std::string_view kScarLook = "EXSCAR";
constexpr std::string_view kOilSlickLook = "OILSLCK";
constexpr std::array kLooks{kCannonballLook, kCannonballTrailLook, kMissileTrailLook, kRopeLook,
                            kBeamLook,       kBeamFlashLook,       kFlashLook,        kScarLook,
                            kOilSlickLook};

constexpr float kChannelMax = 255.0F;
constexpr Vector3 kSourceHeight{.z = 3.0F};
constexpr Vector3 kGameUp{.z = 1.0F};
constexpr Vector3 kGameSide{.x = 1.0F};

constexpr float kCannonballSize = 5.0F;
constexpr TrailStyle kCannonballTrail{.width = 3.0F,
                                      .lifeMs = 300.0F,
                                      .colour = {.r = 0x20 / kChannelMax,
                                                 .g = 0x20 / kChannelMax,
                                                 .b = 0x20 / kChannelMax,
                                                 .a = 0xc8 / kChannelMax}};
constexpr TrailStyle kMissileTrail{
    .width = 1.0F, .lifeMs = 400.0F, .colour = {.a = 0xc8 / kChannelMax}};

constexpr render::Color Rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return {.r = static_cast<float>(r) / kChannelMax,
            .g = static_cast<float>(g) / kChannelMax,
            .b = static_cast<float>(b) / kChannelMax};
}

constexpr float kRopeWidth = 0.6F;
constexpr float kRopeSag = 4.0F;
constexpr std::size_t kRopeSpans = 5;
constexpr BeamColours kRopeColours{.base = Rgb(0x64, 0x3c, 0x0e),
                                   .outward = Rgb(0x8f, 0x5a, 0x1c),
                                   .inward = Rgb(0x14, 0x14, 0x00)};

constexpr float kBeamWidth = 0.85F;
constexpr float kBeamBulge = 9.0F;
constexpr float kBeamJitter = 5.0F;
constexpr Milliseconds kBeamJitterStep{50};
constexpr std::size_t kBeamSpans = 4;
constexpr BeamColours kBeamColours{.base = Rgb(0x19, 0x41, 0xf5),
                                   .outward = Rgb(0x19, 0x41, 0xff),
                                   .inward = Rgb(0x19, 0x41, 0xeb)};
constexpr BeamColours kStrikingBeamColours{};
constexpr float kBeamFlashSize = 8.0F;

constexpr float kFlashBaseWidth = 0.1F;
constexpr float kFlashBaseHeight = 0.2F;
constexpr float kBlastFlashSize = 15.0F;
constexpr float kSpikeFlashSize = 5.0F;
constexpr float kScarAlpha = 180.0F / kChannelMax;
constexpr float kOilSlickSize = 15.0F;

bool IsPhase(const sim::EffectSnapshot& effect, auto phase) {
    return effect.phase == static_cast<std::uint8_t>(phase);
}

float Grown(float base, float full, float growth) {
    return base + ((full - base) * growth);
}

float Jitter(VisualRandom& random) {
    return random.Next() - 0.5F;
}

}

std::span<const std::string_view> PowerupQuadMaterials() {
    return kLooks;
}

PowerupQuads::PowerupQuads(const assets::NameMap<render::Material>& materials) {
    for (const auto& [name, material] : materials) {
        looks_.emplace(name, LookOf(material));
    }
}

void PowerupQuads::Update(const sim::RenderSnapshot& snapshot, const Transform& camera,
                          float elapsedMs) {
    quads_.clear();
    for (SlotTrail& trail : trails_) {
        trail.isLive = false;
    }
    for (const sim::EffectSnapshot& effect : snapshot.effects) {
        AppendEffect(effect, snapshot, camera, elapsedMs);
    }
    for (SlotTrail& trail : trails_) {
        if (!trail.isLive) {
            trail.trail.Clear();
        }
    }
    for (const sim::ExplosionSnapshot& explosion : snapshot.explosions) {
        AppendExplosion(explosion, camera);
    }
}

const QuadLook* PowerupQuads::Find(std::string_view material) const {
    const auto found = looks_.find(material);
    return found == looks_.end() ? nullptr : &found->second;
}

void PowerupQuads::AppendEffect(const sim::EffectSnapshot& effect,
                                const sim::RenderSnapshot& snapshot, const Transform& camera,
                                float elapsedMs) {
    const Vector3 at = effect.transform.position;
    const std::size_t owner = effect.use.racer;
    const Vector3 source = owner < snapshot.entities.size()
                               ? ToGame(snapshot.entities[owner].transform.position) + kSourceHeight
                               : ToGame(at);
    switch (effect.kind) {
        case PowerupKind::Cannonball:
            if (IsPhase(effect, sim::ShotPhase::Flying)) {
                if (const QuadLook* look = Find(kCannonballLook)) {
                    AppendBillboard(*look, at, kCannonballSize, kCannonballSize, camera, quads_);
                }
                AppendTrail(effect, kCannonballTrailLook, kCannonballTrail, elapsedMs);
            }
            break;
        case PowerupKind::Missile:
            if (IsPhase(effect, sim::ShotPhase::Flying)) {
                AppendTrail(effect, kMissileTrailLook, kMissileTrail, elapsedMs);
            }
            break;
        case PowerupKind::GrapplingHook:
            AppendRope(effect, source, camera);
            break;
        case PowerupKind::Lightning:
            AppendBeam(effect, source, snapshot.tick, camera);
            break;
        case PowerupKind::OilSlick:
            if (const QuadLook* look = Find(kOilSlickLook);
                look != nullptr && IsPhase(effect, sim::TrapPhase::Armed)) {
                AppendDecal(*look, at, kOilSlickSize, quads_);
            }
            break;
        default:
            break;
    }
}

void PowerupQuads::AppendTrail(const sim::EffectSnapshot& effect, std::string_view material,
                               const TrailStyle& style, float elapsedMs) {
    if (effect.slot >= trails_.size()) {
        trails_.resize(effect.slot + std::size_t{1});
    }
    SlotTrail& slot = trails_[effect.slot];
    if (slot.kind != effect.kind) {
        slot.trail.Clear();
        slot.kind = effect.kind;
    }
    slot.isLive = true;
    slot.trail.Follow(ToGame(effect.transform.position), style, elapsedMs);
    if (const QuadLook* look = Find(material)) {
        slot.trail.Append(*look, style, quads_);
    }
}

void PowerupQuads::AppendRope(const sim::EffectSnapshot& effect, const Vector3& owner,
                              const Transform& camera) {
    const QuadLook* look = Find(kRopeLook);
    if (look == nullptr || IsPhase(effect, sim::GrapplePhase::Done)) {
        return;
    }
    const Vector3 hook = ToGame(effect.transform.position);
    const float sag = IsPhase(effect, sim::GrapplePhase::Pulling) ? 0.0F : kRopeSag;
    const auto knot = [&](std::size_t i) {
        const float share = static_cast<float>(i) / static_cast<float>(kRopeSpans);
        return Lerp(owner, hook, share) -
               (kGameUp * (sag * std::sin(std::numbers::pi_v<float> * share)));
    };
    path_.assign({{.position = ToEngine(owner), .tint = kRopeColours.base}});
    for (std::size_t i = 0; i < kRopeSpans; ++i) {
        AppendBeamSpan(knot(i), knot(i + 1), kGameSide, 0.0F, kRopeColours, path_);
    }
    AppendRibbon(*look, path_, kRopeWidth, camera, quads_);
}

void PowerupQuads::AppendBeam(const sim::EffectSnapshot& effect, const Vector3& owner,
                              std::uint64_t tick, const Transform& camera) {
    const QuadLook* look = Find(kBeamLook);
    if (look == nullptr || IsPhase(effect, sim::LightningPhase::Done)) {
        return;
    }
    const Vector3 end = ToGame(effect.transform.position);
    const Vector3 side = Normalized(Cross(end - owner, kGameUp), kGameSide);
    const BeamColours& colours = effect.isHoldingRacer ? kStrikingBeamColours : kBeamColours;
    path_.assign({{.position = ToEngine(owner), .tint = colours.base}});
    float bulge = kBeamBulge;
    for (std::size_t i = 0; i < kBeamSpans; ++i) {
        AppendBeamSpan(Lerp(owner, end, static_cast<float>(i) / static_cast<float>(kBeamSpans)),
                       Lerp(owner, end, static_cast<float>(i + 1) / static_cast<float>(kBeamSpans)),
                       side, bulge, colours, path_);
        bulge = -bulge;
    }
    VisualRandom random(
        static_cast<std::uint32_t>(tick * kSimTick.count() / kBeamJitterStep.count()) +
        effect.slot + 1U);
    for (std::size_t i = 1; i + 1 < path_.size(); ++i) {
        path_[i].position += ToEngine((side * (Jitter(random) * kBeamJitter)) +
                                      (kGameUp * (Jitter(random) * kBeamJitter)));
    }
    AppendRibbon(*look, path_, kBeamWidth, camera, quads_);
    if (const QuadLook* flash = Find(kBeamFlashLook); flash != nullptr && effect.isHoldingRacer) {
        AppendBillboard(*flash, effect.transform.position, kBeamFlashSize, kBeamFlashSize, camera,
                        quads_);
    }
}

void PowerupQuads::AppendExplosion(const sim::ExplosionSnapshot& explosion,
                                   const Transform& camera) {
    const bool isBlast = explosion.kind == sim::ExplosionKind::Blast;
    const float size = isBlast ? kBlastFlashSize : kSpikeFlashSize;
    const Vector3& at = explosion.transform.position;
    if (const QuadLook* flash = Find(kFlashLook);
        flash != nullptr && isBlast && explosion.growth > 0.0F) {
        AppendBillboard(*flash, at, Grown(kFlashBaseWidth, size, explosion.growth),
                        Grown(kFlashBaseHeight, size, explosion.growth), camera, quads_);
    }
    if (const QuadLook* scar = Find(kScarLook); scar != nullptr && explosion.scarLeft > 0.0F) {
        QuadLook faded = *scar;
        faded.tint.a = kScarAlpha * explosion.scarLeft;
        AppendDecal(faded, at, size, quads_);
    }
}

}
