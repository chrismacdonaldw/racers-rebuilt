#include "app/powerup/powerup_quads.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

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
constexpr std::string_view kFlashLook = "EXFLASH";
constexpr std::string_view kScarLook = "EXSCAR";
constexpr std::string_view kOilSlickLook = "OILSLCK";
constexpr std::array kLooks{
    kCannonballLook, kCannonballTrailLook, kMissileTrailLook, kRopeLook,
    kBeamLook,       kFlashLook,           kScarLook,         kOilSlickLook};

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

constexpr float kRopeWidth = 0.6F;
constexpr float kRopeSag = 4.0F;
constexpr std::size_t kRopeSegments = 8;
constexpr render::Color kRopeColour{
    .r = 0x64 / kChannelMax, .g = 0x3c / kChannelMax, .b = 0x0e / kChannelMax};

constexpr float kBeamWidth = 0.85F;
constexpr float kBeamJitter = 9.0F;
constexpr std::size_t kBeamSegments = 4;
constexpr render::Color kBeamColour{
    .r = 0x19 / kChannelMax, .g = 0x41 / kChannelMax, .b = 0xf5 / kChannelMax};

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

std::uint32_t Scramble(std::uint32_t value) {
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    return value;
}

float Jitter(std::uint32_t& random) {
    constexpr float kRandomRange = 4294967296.0F;
    random = Scramble(random);
    return (static_cast<float>(random) / kRandomRange) - 0.5F;
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
    path_.clear();
    for (std::size_t i = 0; i <= kRopeSegments; ++i) {
        const float share = static_cast<float>(i) / static_cast<float>(kRopeSegments);
        const float droop = sag * std::sin(std::numbers::pi_v<float> * share);
        path_.push_back(ToEngine(Lerp(owner, hook, share) - (kGameUp * droop)));
    }
    AppendRibbon(
        {.texture = look->texture, .tint = Modulate(look->tint, kRopeColour), .blend = look->blend},
        path_, kRopeWidth, camera, quads_);
}

void PowerupQuads::AppendBeam(const sim::EffectSnapshot& effect, const Vector3& owner,
                              std::uint64_t tick, const Transform& camera) {
    const QuadLook* look = Find(kBeamLook);
    if (look == nullptr || IsPhase(effect, sim::LightningPhase::Done)) {
        return;
    }
    const Vector3 end = ToGame(effect.transform.position);
    const Vector3 side = Normalized(Cross(end - owner, kGameUp), kGameSide);
    std::uint32_t random = Scramble(static_cast<std::uint32_t>(tick) + effect.slot + 1U);
    path_.clear();
    for (std::size_t i = 0; i <= kBeamSegments; ++i) {
        const float share = static_cast<float>(i) / static_cast<float>(kBeamSegments);
        Vector3 point = Lerp(owner, end, share);
        if (i != 0 && i != kBeamSegments) {
            point += (side * (Jitter(random) * kBeamJitter)) +
                     (kGameUp * (Jitter(random) * kBeamJitter));
        }
        path_.push_back(ToEngine(point));
    }
    AppendRibbon(
        {.texture = look->texture, .tint = Modulate(look->tint, kBeamColour), .blend = look->blend},
        path_, kBeamWidth, camera, quads_);
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
